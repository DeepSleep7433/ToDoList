#pragma once

#include <string>
#include <vector>

#include "TodoCore/HttpClient.h"
#include "TodoCore/SqliteRepository.h"

namespace todolist {

/**
 * SyncEngine — 离线优先增量同步状态机（手写核心）。
 *
 * 一次 syncOnce() 走 Pull → Push → Reconcile → Done；
 * 网络错误自动重试（默认 3 次），认证失败视为不可恢复。
 *
 * 约定（v1）：
 *   - 本地 SQLite 始终可用；dirty=1 的行 = 待推送
 *   - 服务端按 LWW 判定；本地在 push 收到 conflict 时采纳服务端权威项
 *   - 同步游标存于 meta["sync_cursor"]（服务端 serverTime）
 */
class SyncEngine {
public:
    enum class Phase { Idle, Pull, Push, Reconcile, Done, Failed };

    struct Report {
        bool ok = false;
        Phase phase = Phase::Idle;
        int pushed = 0;
        int pulled = 0;
        int conflicts = 0;
        int attempts = 0;
        std::string error;
        // 本次冲突明细（"id|服务端权威内容"），供冲突日志落盘
        std::vector<std::string> conflictDetails;
    };

    SyncEngine(SqliteRepository& repo, http::Client& http,
               std::string baseUrl, std::string token);
    ~SyncEngine() = default;

    // 执行一次完整同步；成功时 phase=Done。
    Report syncOnce();

    const std::string& cursor() const { return cursor_; }

private:
    Report attemptSync();
    // Pull 阶段：拉取 since 之后增量并落库（跳过本地 dirty 行）
    int applyPullItems(const std::string& body);
    // Push 阶段：推送全部 dirty 行；冲突明细写入 outDetails，返回冲突数
    int applyPushResults(const std::string& body,
                         std::vector<std::string>& outDetails);

    SqliteRepository& repo_;
    http::Client& http_;
    std::string baseUrl_;
    std::string token_;
    std::string cursor_;
    static constexpr int kMaxAttempts = 3;
};

} // namespace todolist