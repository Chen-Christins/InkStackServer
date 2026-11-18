#include "platform/servlets/file/FileUploadServlet.hpp"

#include <chen/config/config.h>
#include <chen/log/log.h>
#include <chen/parser/multi_part_parser.h>
#include <chen/util/util.h>

#include "platform/manager/AssignmentManager.hpp"
#include "platform/manager/AssignmentUserRelManager.hpp"
#include "platform/manager/ResourceManager.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::string>::ptr server_work_path = chen::Config::Lookup<std::string>("server.work_path");

FileUploadServlet::FileUploadServlet()
    : BlogLoginedServlet("FileUploadServlet")
{
}

int32_t FileUploadServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string content_type = request->getHeader("content-type");
        chen::MultipartParser::ptr parser = std::make_shared<chen::MultipartParser>(content_type);

        auto data = parser->parseToMemory(request->getBody());

        // [0]:dir_name  [1]:biz_type|biz_id  [2...]:files
        if (data.size() < 3) {
            result->setErrno(errcode::FILE_PROTOCOL_ERROR);
            break;
        }

        auto biz_info = chen::StringUtil::Split(data[1].content, "|");
        if (biz_info.size() != 2) {
            result->setErrno(errcode::FILE_PROTOCOL_ERROR);
            break;
        }

        std::string biz_type = biz_info[0];
        int64_t biz_id = chen::TypeUtil::Atoi(biz_info[1]);

        auto assignment = AssignmentMgr::GetInstance()->get(biz_id);
        if (!assignment) {
            result->setErrno(errcode::ASSIGNMENT_NOT_FOUND);
            break;
        }

        int64_t uid = getUserId(request);
        auto uploader = UserMgr::GetInstance()->get(uid);
        if (!uploader) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        std::string subject_name = assignment->getSubjectName();
        std::string user_name = uploader->getName();
        std::string dir_name = biz_type + "/" + subject_name + "/" + data[0].content + "/" + user_name;
        std::string save_dir = server_work_path->getValue() + "/uploads/" + dir_name;

        INFO(logger) << "File upload save dir: " << save_dir;

        for (size_t i = 2; i < data.size(); ++i) {
            std::string filename = save_dir + "/" + data[i].filename;
            std::ofstream ofs;
            bool rt = chen::FSUtil::OpenForWrite(ofs, filename, std::ios::binary);
            if (rt) {
                ofs.write(data[i].content.c_str(), data[i].content.size());
            }
            ofs.close();
            INFO(logger) << "File saved: " << filename << " (Size: " << data[i].content.size() << " bytes -- "
                         << (1.0 * data[i].content.size() / 1024) << " kb -- " << (1.0 * data[i].content.size() / (1024 * 1024)) << " mb)";

            // generate hash key
            std::string hash_key = chen::EncryptorUtil::MD5(data[i].content);
            size_t size = data[i].content.size();

            std::string path = "/uploads/" + dir_name + "/" + data[i].filename;
            if (!dumpToResource(biz_type, biz_id, path, hash_key, uid, size)) {
                result->setErrno(errcode::FILE_DUMP_FAILED);
                break;
            }
        }

        time_t now = time(0);
        auto db = getDB();
        if (!db) {
            ERROR(logger) << "Get SQLite3 connection fail";
            result->setErrno(errcode::DB_CONNECTION_FAILED);
            break;
        }
        // 查找是否已存在该用户该作业的记录
        auto old_info = AssignmentUserRelMgr::GetInstance()->getByAssignAndUser(biz_id, uid);
        auto info = std::make_shared<data::AssignmentUserRelInfo>();
        info->setAssignmentId(biz_id);
        info->setUserId(uid);
        info->setStatus(AssignmentUserRelManager::SUBMITTED);
        info->setSubmitTime(now);
        if (old_info) {
            info->setId(old_info->getId()); // 复用原id，update
            info->setUpdateTime(now);
            if (blog::data::AssignmentUserRelInfoDao::Update(info, db)) {
                ERROR(logger) << "AssignmentUserRelInfo Update fail";
                result->setErrno(errcode::DB_OPERATION_FAILED);
                break;
            }
        } else {
            info->setCreateTime(now);
            if (blog::data::AssignmentUserRelInfoDao::Insert(info, db)) {
                ERROR(logger) << "AssignmentUserRelInfo Insert fail";
                result->setErrno(errcode::DB_OPERATION_FAILED);
                break;
            }
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

bool FileUploadServlet::dumpToResource(
    const std::string& biz_type,
    int64_t biz_id,
    const std::string& path,
    const std::string& hash_key,
    int64_t uid,
    size_t size)
{
    auto it = path.rfind("/");
    if (it == std::string::npos) {
        return false;
    }
    std::string filename = path.substr(it + 1);
    ResourceManager::ResourceType type = ResourceMgr::GetInstance()->GetResourceType(filename);
    auto db = getDB();
    if (!db) {
        ERROR(logger) << "Get SQLite3 connection fail";
        return false;
    }
    // 先查找是否已存在同业务同用户同名文件
    auto old_info = ResourceMgr::GetInstance()->getByBizUidName(biz_type, biz_id, uid, filename);
    auto resource_info = std::make_shared<data::ResourceInfo>();
    resource_info->setName(filename);
    resource_info->setPath(path);
    resource_info->setType(type);
    resource_info->setSize(size);
    resource_info->setHash(hash_key);
    resource_info->setOwnerId(uid);
    resource_info->setBizType(biz_type);
    resource_info->setBizId(biz_id);
    resource_info->setStatus(ResourceManager::Status::NORMAL);
    time_t now = time(0);
    resource_info->setCreateTime(now);
    resource_info->setUpdateTime(now);
    if (old_info) {
        resource_info->setId(old_info->getId()); // 复用原id，update
        if (blog::data::ResourceInfoDao::Update(resource_info, db)) {
            ERROR(logger) << "ResourceInfo Update fail";
            return false;
        }
    } else {
        if (blog::data::ResourceInfoDao::Insert(resource_info, db)) {
            ERROR(logger) << "ResourceInfo Insert fail";
            return false;
        }
    }
    ResourceMgr::GetInstance()->add(resource_info);
    return true;
}

} // namespace servlet
} // namespace blog
