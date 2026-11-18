#include "user/client/ArticleClient.hpp"

#include "common/rpc/RpcEndpoint.hpp"
#include "common/rpc/RpcHubClient.hpp"

namespace blog {
namespace client {

int64_t ArticleClient::countByUser(int64_t uid, bool include_deleted)
{
    int64_t count = 0;
    if (!rpc::RpcHubClientMgr::GetInstance()->tryCall(rpc::FUNC_ARTICLE, ARTICLE_CMD_COUNT_BY_USER, count, uid, include_deleted)) {
        return 0;
    }
    return count;
}

tagArticleBrief ArticleClient::getArticleBrief(int64_t id)
{
    tagArticleBrief brief;
    rpc::RpcHubClientMgr::GetInstance()->tryCall(rpc::FUNC_ARTICLE, ARTICLE_CMD_GET_BRIEF, brief, id);
    return brief;
}

std::vector<tagArticleBrief> ArticleClient::getArticleBriefs(const std::vector<int64_t>& ids)
{
    std::vector<tagArticleBrief> briefs;
    if (!rpc::RpcHubClientMgr::GetInstance()->tryCall(rpc::FUNC_ARTICLE, ARTICLE_CMD_GET_BRIEFS, briefs, ids)) {
        return {};
    }
    return briefs;
}

} // namespace client
} // namespace blog
