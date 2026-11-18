#include "platform/servlets/organization/OrganizationReposAddServlet.hpp"

#include <chen/log/log.h>

#include "../../../common/util/Util.hpp"
#include "platform/manager/OrganizationManager.hpp"
#include "platform/manager/OrganizationRepoManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationReposAddServlet::OrganizationReposAddServlet()
    : BlogLoginedServlet("OrganizationReposAddServlet")
{
}

int32_t OrganizationReposAddServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");
        DEFINE_AND_CHECK_STRING(result, name, "name");
        DEFINE_AND_CHECK_STRING(result, url, "url");
        std::string language = request->getParamAs<std::string>("language");
        std::string description = request->getParamAs<std::string>("description");
        std::string token = request->getParamAs<std::string>("token");

        // 校验 name 长度
        if (name.size() > 128) {
            result->setErrno(errcode::PARAM_INVALID, "仓库名称最长128字符");
            break;
        }
        // 校验 url 长度
        if (url.size() > 512) {
            result->setErrno(errcode::PARAM_INVALID, "仓库地址最长512字符");
            break;
        }
        // 仅支持 GitHub 仓库
        if (url.find("github.com") == std::string::npos) {
            result->setErrno(errcode::PARAM_INVALID, "仅支持 GitHub 仓库");
            break;
        }
        // 校验 language 长度
        if (language.size() > 64) {
            result->setErrno(errcode::PARAM_INVALID, "编程语言最长64字符");
            break;
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        // 检查组织是否存在
        auto org = OrganizationMgr::GetInstance()->get(org_id);
        if (!org) {
            result->setErrno(errcode::ORG_NOT_FOUND);
            break;
        }

        // 检查用户权限：需要研发主管及以上（role >= 4），系统管理员可管理任意组织仓库
        auto user = UserMgr::GetInstance()->get(uid);
        if (!user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        int32_t system_role = user->getRole();
        bool is_admin = (system_role == UserManager::Role::ADMIN);

        if (!is_admin) {
            auto rel = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, uid);
            if (!rel || rel->getStatus() != OrganizationUserRelManager::Status::APPROVED) {
                result->setErrno(errcode::ACCESS_DENIED, "无权访问该组织");
                break;
            }
            int32_t org_role = rel->getRole();
            if (org_role != OrganizationManager::Role::ORG_ADMIN && org_role != OrganizationManager::Role::DEV_LEAD) {
                result->setErrno(errcode::ACCESS_DENIED, "仅研发主管及以上角色可添加仓库");
                break;
            }
        }

        // 检查同组织下名称是否重复
        auto existing = OrganizationRepoMgr::GetInstance()->getByOrgAndName(org_id, name);
        if (existing) {
            result->setErrno(errcode::ORG_REPO_NAME_EXISTS, "该组织下已存在同名仓库");
            break;
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        data::OrganizationReposInfo::ptr info(new data::OrganizationReposInfo);
        info->setOrgId(org_id);
        info->setName(name);
        info->setUrl(url);
        info->setLanguage(language);
        info->setDescription(description);
        info->setToken(token);
        info->setStarsCount(0);
        info->setSortOrder(0);
        info->setSyncStatus("idle");
        info->setPrSyncStatus("idle");
        info->setPrSyncedAt(0);
        info->setCreateTime(time(0));
        info->setUpdateTime(time(0));

        // 自动从 URL 提取 github_full_name（owner/repo）
        {
            std::string full_name;
            std::string marker = "github.com/";
            auto pos = url.find(marker);
            if (pos != std::string::npos) {
                full_name = url.substr(pos + marker.size());
            } else {
                marker = "github.com:";
                pos = url.find(marker);
                if (pos != std::string::npos) {
                    full_name = url.substr(pos + marker.size());
                }
            }
            if (!full_name.empty()) {
                // 去掉尾部 .git
                if (full_name.size() > 4 && full_name.substr(full_name.size() - 4) == ".git") {
                    full_name = full_name.substr(0, full_name.size() - 4);
                }
                info->setGithubFullName(full_name);
            }
        }

        if (data::OrganizationReposInfoDao::Insert(info, db)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "添加仓库失败");
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        OrganizationRepoMgr::GetInstance()->add(info);

        result->set("id", info->getId());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
