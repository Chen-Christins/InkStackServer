#include "chunk_upload_servlet.h"

#include "../../util.h"

#include <chen/config/config.h>
#include <chen/log/log.h>
#include <chen/util/encryptor_util.h>
#include <chen/util/util.h>

#include <fstream>
#include <vector>

#include "../../chunk_upload.h"
#include "../../event/event_define.h"
#include "../../manager/assignment_manager.h"
#include "../../manager/assignment_organization_rel_manager.h"
#include "../../manager/assignment_user_rel_manager.h"
#include "../../manager/organization_user_rel_manager.h"
#include "../../manager/resource_manager.h"
#include "../../manager/user_manager.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();
static chen::ConfigVar<std::string>::ptr server_work_path = chen::Config::Lookup<std::string>("server.work_path");

ChunkUploadServlet::ChunkUploadServlet()
    : BlogLoginedServlet("ChunkUploadServlet")
{
}

int32_t ChunkUploadServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        auto path = request->getPath();
        if (path == "/upload/init") {
            return handleInit(request, response, session, result);
        } else if (path == "/upload/complete") {
            return handleComplete(request, response, session, result);
        } else if (path == "/upload/cancel") {
            return handleCancel(request, response, session, result);
        } else if (path == "/upload/chunk") {
            return handleUpload(request, response, session, result);
        } else if (path == "/upload/status") {
            return handleStatus(request, response, session, result);
        } else {
            result->setErrno(errcode::FILE_NOT_FOUND);
            break;
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

void ChunkUploadServlet::parseBizInfo(const std::string& biz_info, std::string& biz_type, std::string& biz_id)
{
    biz_type = "";
    biz_id = "";
    do {
        auto pos = biz_info.find("|");
        if (pos == std::string::npos) {
            break;
        }
        biz_type = biz_info.substr(0, pos);
        biz_id = biz_info.substr(pos + 1);
    } while (0);
}

int32_t ChunkUploadServlet::handleInit(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string type = request->getHeader("X-Upload-Type");
        std::string file_name = chen::StringUtil::URLDecode(request->getHeader("X-Upload-Filename"));
        std::string dir_name = chen::StringUtil::URLDecode(request->getHeader("X-Upload-Dir-Name"));
        std::string biz_info = chen::StringUtil::URLDecode(request->getHeader("X-Upload-Biz-Info"));
        uint64_t total_size = std::stoull(request->getHeader("X-Upload-Total-Size"));
        uint64_t total_chunks = std::stoull(request->getHeader("X-Upload-Total-Chunks"));
        uint32_t chunk_size = std::stoul(request->getHeader("X-Upload-Chunk-Size"));

        if (type != "chunked") {
            result->setErrno(errcode::UPLOAD_INVALID_TYPE);
            break;
        }
        if (file_name.empty() || dir_name.empty() || biz_info.empty() || total_size == 0 || total_chunks == 0 || chunk_size == 0) {
            result->setErrno(errcode::UPLOAD_INVALID_PARAMS);
            break;
        }

        std::string biz_type, biz_id;
        parseBizInfo(biz_info, biz_type, biz_id);

        int64_t uid = getUserId(request);

        // 生成 uploadId
        time_t now = time(0);
        std::string upload_id =
            chen::EncryptorUtil::MD5(std::to_string(now) + "|" + file_name + "|" + std::to_string(uid) + chen::RandomUtil::RandString(6));

        auto upload_session = ChunkUploadMgr::GetInstance()->createSession(
            upload_id, file_name, total_size, chunk_size, total_chunks, biz_type, chen::TypeUtil::Atoi(biz_id), dir_name);

        result->set("upload_id", upload_id);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

int32_t ChunkUploadServlet::handleComplete(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    std::string upload_id;
    bool assembly_ok = false;
    do {
        std::string type = request->getHeader("X-Upload-Type");
        upload_id = chen::StringUtil::URLDecode(request->getHeader("X-Upload-Id"));

        if (type != "chunked") {
            result->setErrno(errcode::UPLOAD_INVALID_TYPE);
            break;
        }
        if (upload_id.empty()) {
            result->setErrno(errcode::UPLOAD_INVALID_PARAMS);
            break;
        }

        auto upload_session = ChunkUploadMgr::GetInstance()->getSession(upload_id);
        if (!upload_session) {
            result->setErrno(errcode::UPLOAD_SESSION_NOT_FOUND);
            break;
        }

        // 检查是否所有分块都已接收
        {
            std::lock_guard<std::mutex> lock(upload_session->m_mtx);
            if (upload_session->received_count != upload_session->total_chunks) {
                result->setErrno(errcode::UPLOAD_CHUNK_INCOMPLETE);
                break;
            }
            upload_session->completed = true;
        }

        int64_t uid = getUserId(request);
        auto assignment = AssignmentMgr::GetInstance()->get(upload_session->biz_id);
        if (!assignment) {
            result->setErrno(errcode::ASSIGNMENT_NOT_FOUND);
            break;
        }
        auto uploader = UserMgr::GetInstance()->get(uid);
        if (!uploader) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        std::string subject_name = assignment->getSubjectName();
        std::string user_name = uploader->getName();
        std::string dir_name = upload_session->biz_type + "/" + subject_name + "/" + upload_session->dir_name + "/" + user_name;
        std::string save_dir = server_work_path->getValue() + "/uploads/" + dir_name;

        INFO(logger) << "File upload save dir: " << save_dir;

        std::string filename = save_dir + "/" + upload_session->file_name;
        std::ofstream ofs;
        if (!chen::FSUtil::OpenForWrite(ofs, filename, std::ios::binary)) {
            result->setErrno(errcode::UPLOAD_FILE_CREATE_FAILED);
            break;
        }

        // 流式组装分块，buffer 在循环外分配复用
        std::vector<char> buffer(65536); // 64KB
        for (size_t i = 0; i < upload_session->total_chunks; ++i) {
            std::ifstream temp_file(upload_session->chunk_data[i], std::ios::binary);
            if (!temp_file.is_open()) {
                ERROR(logger) << "Failed to open temp file: " << upload_session->chunk_data[i];
                result->setErrno(errcode::UPLOAD_FILE_ASSEMBLE_FAILED);
                goto assembly_done;
            }
            while (temp_file.read(buffer.data(), buffer.size())) {
                ofs.write(buffer.data(), temp_file.gcount());
            }
            if (temp_file.gcount() > 0) {
                ofs.write(buffer.data(), temp_file.gcount());
            }
            temp_file.close();
            // 删除临时文件
            chen::FSUtil::Unlink(upload_session->chunk_data[i], true);
        }
        assembly_ok = true;
    assembly_done:
        ofs.close();

        if (!assembly_ok) {
            // 组装失败，清理已删除的临时文件并移除会话
            for (size_t i = 0; i < upload_session->total_chunks; ++i) {
                if (!upload_session->chunk_data[i].empty()) {
                    chen::FSUtil::Unlink(upload_session->chunk_data[i], true);
                }
            }
            ChunkUploadMgr::GetInstance()->removeSession(upload_id);
            break;
        }

        INFO(logger) << "File saved: " << filename << " (Size: " << upload_session->total_size << " bytes -- "
                     << (1.0 * upload_session->total_size / 1024) << " kb -- " << (1.0 * upload_session->total_size / (1024 * 1024)) << " mb)";

        // 流式 MD5 计算，避免全量读入内存
        std::string hash_key = chen::EncryptorUtil::MD5File(filename);
        if (hash_key.empty()) {
            ERROR(logger) << "Failed to open final file for hash calculation: " << filename;
            result->setErrno(errcode::UPLOAD_FILE_HASH_FAILED);
            break;
        }

        std::string path = "/uploads/" + dir_name + "/" + upload_session->file_name;
        if (!dumpToResource(upload_session->biz_type, upload_session->biz_id, path, hash_key, uid, upload_session->total_size)) {
            result->setErrno(errcode::FILE_DUMP_FAILED);
            break;
        }

        time_t now = time(0);
        auto db = getDB();
        if (!db) {
            ERROR(logger) << "Get SQLite3 connection fail";
            result->setErrno(errcode::DB_CONNECTION_FAILED);
            break;
        }
        // 查找是否已存在该用户该作业的记录
        auto old_info = AssignmentUserRelMgr::GetInstance()->getByAssignAndUser(upload_session->biz_id, uid);
        auto info = std::make_shared<data::AssignmentUserRelInfo>();
        info->setAssignmentId(upload_session->biz_id);
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
        // Notify org admins about assignment submission
        {
            int64_t assign_id = upload_session->biz_id;
            auto assignment = AssignmentMgr::GetInstance()->get(assign_id);
            std::string assign_name = assignment ? assignment->getName() : std::to_string(assign_id);
            std::string submitter_name = user_name;

            std::vector<int64_t> admin_ids;
            std::vector<data::AssignmentOrganizationRelInfo::ptr> org_rels;
            AssignmentOrganizationRelMgr::GetInstance()->getByAssignmentId(org_rels, assign_id);
            for (auto& org_rel : org_rels) {
                if (org_rel->getIsDeleted())
                    continue;
                int64_t org_id = org_rel->getOrganizationId();
                std::vector<data::OrganizationUserRelInfo::ptr> members;
                OrganizationUserRelMgr::GetInstance()->getByPages(members, org_id, 0, 10000, -1, true);
                for (auto& m : members) {
                    if (m->getRole() == 5) {
                        admin_ids.push_back(m->getUserId());
                    }
                }
            }

            EventAssignmentSubmittedData data = {};
            data.submitter_id = uid;
            data.submitter_name = submitter_name;
            data.assignment_id = assign_id;
            data.assignment_name = assign_name;
            data.admin_user_ids = admin_ids;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_ASSIGNMENT_SUBMITTED, std::move(data));
        }

        result->set("file_path", path);
        // 成功后清理会话
        ChunkUploadMgr::GetInstance()->removeSession(upload_id);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

int32_t ChunkUploadServlet::handleCancel(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string type = request->getHeader("X-Upload-Type");
        std::string upload_id = chen::StringUtil::URLDecode(request->getHeader("X-Upload-Id"));

        if (type != "chunked") {
            result->setErrno(errcode::UPLOAD_INVALID_TYPE);
            break;
        }
        if (upload_id.empty()) {
            result->setErrno(errcode::UPLOAD_INVALID_PARAMS);
            break;
        }

        auto upload_session = ChunkUploadMgr::GetInstance()->getSession(upload_id);
        if (!upload_session) {
            result->setErrno(errcode::UPLOAD_SESSION_NOT_FOUND);
            break;
        }

        // 清理所有临时文件
        {
            std::lock_guard<std::mutex> lock(upload_session->m_mtx);
            for (size_t i = 0; i < upload_session->total_chunks; ++i) {
                if (!upload_session->chunk_data[i].empty()) {
                    chen::FSUtil::Unlink(upload_session->chunk_data[i], true);
                }
            }
        }

        ChunkUploadMgr::GetInstance()->removeSession(upload_id);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

int32_t ChunkUploadServlet::handleUpload(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string type = request->getHeader("X-Upload-Type");
        std::string upload_id = chen::StringUtil::URLDecode(request->getHeader("X-Upload-Id"));
        uint64_t chunk_index = std::stoul(request->getHeader("X-Upload-Chunk-Index"));
        // uint64_t offset = std::stoull(request->getHeader("X-Upload-Offset"));
        uint64_t chunk_size = std::stoul(request->getHeader("X-Upload-Chunk-Size"));
        // 数据部分
        const std::string& body = request->getBody();

        if (type != "chunked") {
            result->setErrno(errcode::UPLOAD_INVALID_TYPE);
            break;
        }
        if (upload_id.empty() || chunk_size == 0 || body.empty()) {
            result->setErrno(errcode::UPLOAD_INVALID_PARAMS);
            break;
        }
        if (body.size() > chunk_size) {
            result->setErrno(errcode::UPLOAD_CHUNK_SIZE_EXCEED);
            break;
        }

        auto upload_session = ChunkUploadMgr::GetInstance()->getSession(upload_id);
        if (!upload_session) {
            result->setErrno(errcode::UPLOAD_SESSION_NOT_FOUND);
            break;
        }

        std::lock_guard<std::mutex> lock(upload_session->m_mtx);
        if (chunk_index >= upload_session->total_chunks) {
            result->setErrno(errcode::UPLOAD_CHUNK_INVALID);
            break;
        }
        if (upload_session->received_chunks[chunk_index]) {
            result->setErrno(errcode::UPLOAD_CHUNK_ALREADY_DONE);
            break;
        }
        // 确保临时目录存在
        std::string temp_dir = server_work_path->getValue() + "/temp";
        if (!chen::FSUtil::Mkdir(temp_dir)) {
            ERROR(logger) << "Failed to create temp directory: " << temp_dir;
            result->setErrno(errcode::UPLOAD_TEMP_DIR_FAILED);
            break;
        }

        // 保存分块数据到临时文件而不是内存
        std::string temp_file_path = temp_dir + "/" + upload_id + "_" + std::to_string(chunk_index);
        std::ofstream temp_file(temp_file_path, std::ios::binary);
        if (!temp_file.is_open()) {
            result->setErrno(errcode::UPLOAD_TEMP_FILE_FAILED);
            break;
        }
        temp_file.write(body.c_str(), body.size());
        temp_file.close();

        // 只保存文件路径，不保存数据内容
        upload_session->chunk_data[chunk_index] = temp_file_path;
        upload_session->received_chunks[chunk_index] = true;
        upload_session->received_count++;
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

int32_t ChunkUploadServlet::handleStatus(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string type = request->getHeader("X-Upload-Type");
        std::string upload_id = chen::StringUtil::URLDecode(request->getHeader("X-Upload-Id"));

        if (type != "chunked") {
            result->setErrno(errcode::UPLOAD_INVALID_TYPE);
            break;
        }
        if (upload_id.empty()) {
            result->setErrno(errcode::UPLOAD_INVALID_PARAMS);
            break;
        }

        auto upload_session = ChunkUploadMgr::GetInstance()->getSession(upload_id);
        if (!upload_session) {
            result->setErrno(errcode::UPLOAD_SESSION_NOT_FOUND);
            break;
        }

        // FIXME: 这里返回的信息可以更详细一些，比如哪些分块已接收，哪些未接收
        result->set("upload_id", upload_session->upload_id);
        result->set("file_name", upload_session->file_name);
        result->set("total_size", upload_session->total_size);
        result->set("total_chunks", upload_session->total_chunks);
        result->set("received_chunks", upload_session->received_count);
        result->set("completed", upload_session->completed);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

bool ChunkUploadServlet::dumpToResource(
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
