#include "platform/logic/ChunkUpload.hpp"

#include <chen/log/log.h>
#include <chen/util/util.h>

#include <algorithm>

static chen::Logger::ptr g_logger = LOG_ROOT();

ChunkUploadSession::ptr ChunkUploadManager::createSession(
    const std::string& upload_id,
    const std::string& file_name,
    size_t total_size,
    size_t chunk_size,
    size_t total_chunks,
    const std::string& biz_type,
    int64_t biz_id,
    const std::string& dir_name)
{
    std::lock_guard lock(m_mtx);

    auto session = std::make_shared<ChunkUploadSession>();
    session->upload_id = upload_id;
    session->file_name = file_name;
    session->total_size = total_size;
    session->chunk_size = chunk_size;
    session->total_chunks = total_chunks;
    session->biz_type = biz_type;
    session->biz_id = biz_id;
    session->dir_name = dir_name;
    session->created_at = time(0);
    session->received_chunks.resize(total_chunks, false);
    session->chunk_data.resize(total_chunks);

    m_sessions[upload_id] = session;

    return session;
}

ChunkUploadSession::ptr ChunkUploadManager::getSession(const std::string& upload_id)
{
    std::shared_lock lock(m_mtx);
    auto it = m_sessions.find(upload_id);
    if (it != m_sessions.end()) {
        return it->second;
    }
    return nullptr;
}

void ChunkUploadManager::removeSession(const std::string& upload_id)
{
    std::lock_guard lock(m_mtx);
    m_sessions.erase(upload_id);
}

void ChunkUploadManager::cleanupExpiredSessions()
{
    time_t now = time(0);
    std::vector<std::string> expired_ids;
    std::vector<std::string> temp_files_to_remove;

    {
        std::shared_lock lock(m_mtx);
        for (auto& kv : m_sessions) {
            auto& session = kv.second;
            if (!session) {
                continue;
            }
            if (now - session->created_at > k_session_ttl) {
                expired_ids.push_back(kv.first);
                // 收集临时文件路径
                std::lock_guard<std::mutex> slock(session->m_mtx);
                size_t count = std::min(session->total_chunks, session->chunk_data.size());
                for (size_t i = 0; i < count; ++i) {
                    if (!session->chunk_data[i].empty()) {
                        temp_files_to_remove.push_back(session->chunk_data[i]);
                    }
                }
            }
        }
    }

    // 清理临时文件
    for (auto& fp : temp_files_to_remove) {
        chen::FSUtil::Unlink(fp, true);
    }

    // 移除过期会话
    if (!expired_ids.empty()) {
        std::lock_guard lock(m_mtx);
        for (auto& id : expired_ids) {
            m_sessions.erase(id);
        }
        INFO(g_logger) << "Cleaned up " << expired_ids.size() << " expired upload sessions";
    }
}
