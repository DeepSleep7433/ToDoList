#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "TodoCore/HttpClient.h"
#include "TodoCore/SqliteRepository.h"
#include "TodoCore/SyncEngine.h"
#include "test_harness.h"

using todolist::http::Client;
using todolist::http::Response;
using todolist::SqliteRepository;
using todolist::SyncEngine;
using todolist::TodoItem;

namespace {

// 脚本化 HTTP 桩：测试无需真实网络/服务器
struct StubHttp : Client {
    std::function<Response(const std::string& url, const std::string& token,
                           const std::string& body)>
        handler;
    std::vector<std::string> log;

    Response post(const std::string& url, const std::string& token,
                  const std::string& body) override {
        log.push_back("POST " + url);
        if (!handler) return err("no handler");
        return handler(url, token, body);
    }
    Response get(const std::string& url, const std::string& token) override {
        log.push_back("GET " + url);
        if (!handler) return err("no handler");
        return handler(url, token, "");
    }

    static Response ok(const std::string& body) {
        Response r;
        r.status = 200;
        r.body = body;
        return r;
    }
    static Response err(const std::string& m) {
        Response r;
        r.error = m;
        return r;
    }
};

Response pullEmpty(const std::string& serverTime) {
    return StubHttp::ok(R"({"serverTime":")" + serverTime + R"(","items":[]})");
}

const std::string kSinceHeader = "http://s/api/v1/sync/pull?since=";

} // namespace

static void testPushDirtyAcceptedAndCursor() {
    SqliteRepository repo(":memory:");
    CHECK(repo.open());
    TodoItem a = TodoItem::create("本地新建");
    CHECK(repo.add(a));
    CHECK(repo.countAll() == 1);

    StubHttp http;
    http.handler = [&](const std::string& url, const std::string&, const std::string&) -> Response {
        if (url.find("/pull") != std::string::npos) {
            CHECK(url.find(kSinceHeader) != std::string::npos); // 初值 epoch
            return pullEmpty("2026-09-07T00:00:01.000Z");
        }
        // push：echo accepted v1
        const std::string body = R"({"results":[{"id":")" + a.id +
            R"(","status":"accepted","serverVersion":1,"item":{"id":")" + a.id +
            R"(","content":"本地新建","isDone":false,"createdAt":"2026-09-07T00:00:00.000Z","updatedAt":"2026-09-07T00:00:00.000Z","deleted":false,"serverVersion":1}}]})";
        return StubHttp::ok(body);
    };

    SyncEngine engine(repo, http, "http://s/api/v1", "tok");
    auto rep = engine.syncOnce();
    CHECK(rep.ok);
    CHECK(rep.phase == SyncEngine::Phase::Done);
    CHECK_EQ(rep.pushed, 1);
    CHECK_EQ(rep.pulled, 0);
    CHECK_EQ(rep.conflicts, 0);
    CHECK_EQ(rep.attempts, 1);
    auto after = repo.findById(a.id);
    CHECK(after.has_value());
    CHECK(!after->dirty);
    CHECK_EQ(after->serverVersion, 1LL);
    CHECK(repo.getMeta("sync_cursor").value_or("") == "2026-09-07T00:00:01.000Z");
}

static void testPullServerNewerOverwrites() {
    SqliteRepository repo(":memory:");
    CHECK(repo.open());
    TodoItem local = TodoItem::create("本地旧内容");
    local.serverVersion = 1;
    local.dirty = false;
    CHECK(repo.add(local));

    StubHttp http;
    http.handler = [&](const std::string& url, const std::string&, const std::string&) -> Response {
        const std::string body = R"({"serverTime":"2026-09-07T00:00:02.000Z","items":[{"id":")" + local.id +
            R"(","content":"服务端新内容","isDone":true,"createdAt":"2026-09-07T00:00:00.000Z","updatedAt":"2026-09-07T00:00:02.000Z","deleted":false,"serverVersion":2}]})";
        if (url.find("/pull") != std::string::npos) return StubHttp::ok(body);
        return pullEmpty("2026-09-07T00:00:02.000Z");
    };
    SyncEngine engine(repo, http, "http://s/api/v1", "tok");
    auto rep = engine.syncOnce();
    CHECK(rep.ok);
    CHECK_EQ(rep.pulled, 1);
    auto after = repo.findById(local.id);
    CHECK(after.has_value());
    CHECK(after->content == "服务端新内容");
    CHECK(after->isDone);
    CHECK(!after->dirty);
    CHECK_EQ(after->serverVersion, 2LL);
}

static void testConflictAdoptsServerItem() {
    SqliteRepository repo(":memory:");
    CHECK(repo.open());
    TodoItem local = TodoItem::create("本地较旧修改");
    CHECK(repo.add(local)); // dirty=1

    StubHttp http;
    http.handler = [&](const std::string& url, const std::string&, const std::string&) -> Response {
        if (url.find("/pull") != std::string::npos) {
            return pullEmpty("2026-09-07T00:00:01.000Z");
        }
        const std::string body = R"({"results":[{"id":")" + local.id +
            R"(","status":"conflict","serverVersion":3,"item":{"id":")" + local.id +
            R"(","content":"服务端权威","isDone":false,"createdAt":"2026-09-07T00:00:00.000Z","updatedAt":"2026-09-07T00:00:03.000Z","deleted":false,"serverVersion":3}}]})";
        return StubHttp::ok(body);
    };
    SyncEngine engine(repo, http, "http://s/api/v1", "tok");
    auto rep = engine.syncOnce();
    CHECK(rep.ok);
    CHECK_EQ(rep.conflicts, 1);
    auto after = repo.findById(local.id);
    CHECK(after.has_value());
    CHECK(after->content == "服务端权威");
    CHECK(!after->dirty);
    CHECK_EQ(after->serverVersion, 3LL);
}

static void testDirtyLocalSurvivesPullAndWinsOnAccept() {
    SqliteRepository repo(":memory:");
    CHECK(repo.open());
    TodoItem local = TodoItem::create("离线编辑内容");
    CHECK(repo.add(local)); // dirty=1, v0

    StubHttp http;
    http.handler = [&](const std::string& url, const std::string&, const std::string&) -> Response {
        if (url.find("/pull") != std::string::npos) {
            // 极端情形：pull 竟带回同 id 服务端行 → 因本地 dirty 应跳过覆盖
            const std::string body = R"({"serverTime":"2026-09-07T00:00:01.000Z","items":[{"id":")" + local.id +
                R"(","content":"服务端v99","isDone":false,"createdAt":"2026-09-07T00:00:00.000Z","updatedAt":"2026-09-07T00:00:01.000Z","deleted":false,"serverVersion":99}]})";
            return StubHttp::ok(body);
        }
        const std::string body = R"({"results":[{"id":")" + local.id +
            R"(","status":"accepted","serverVersion":1,"item":{"id":")" + local.id +
            R"(","content":"离线编辑内容","isDone":false,"createdAt":"2026-09-07T00:00:00.000Z","updatedAt":"2026-09-07T00:00:00.000Z","deleted":false,"serverVersion":1}}]})";
        return StubHttp::ok(body);
    };
    SyncEngine engine(repo, http, "http://s/api/v1", "tok");
    auto rep = engine.syncOnce();
    CHECK(rep.ok);
    CHECK_EQ(rep.pushed, 1);
    auto after = repo.findById(local.id);
    CHECK(after.has_value());
    CHECK(after->content == "离线编辑内容"); // 未被服务端 v99 覆盖
    CHECK(!after->dirty);
    CHECK_EQ(after->serverVersion, 1LL);
}

static void testAuthFailureIsFatalNoRetryLoop() {
    SqliteRepository repo(":memory:");
    CHECK(repo.open());
    StubHttp http;
    http.handler = [](const std::string&, const std::string&, const std::string&) -> Response {
        Response r;
        r.status = 401;
        return r;
    };
    SyncEngine engine(repo, http, "http://s/api/v1", "bad-token");
    auto rep = engine.syncOnce();
    CHECK(!rep.ok);
    CHECK(rep.phase == SyncEngine::Phase::Failed);
    CHECK_EQ(rep.attempts, 1);
}

static void testNetworkRetrySucceedsOnSecondTry() {
    SqliteRepository repo(":memory:");
    CHECK(repo.open());
    int calls = 0;
    StubHttp http;
    http.handler = [&](const std::string& url, const std::string&, const std::string&) -> Response {
        ++calls;
        if (calls == 1) return StubHttp::err("connection reset");
        if (url.find("/pull") != std::string::npos) return pullEmpty("2026-09-07T00:00:01.000Z");
        return pullEmpty("2026-09-07T00:00:01.000Z");
    };
    SyncEngine engine(repo, http, "http://s/api/v1", "tok");
    auto rep = engine.syncOnce();
    CHECK(rep.ok);
    CHECK_EQ(rep.attempts, 2);
    CHECK(calls >= 2);
}

int main() {
    RUN_TEST(testPushDirtyAcceptedAndCursor);
    RUN_TEST(testPullServerNewerOverwrites);
    RUN_TEST(testConflictAdoptsServerItem);
    RUN_TEST(testDirtyLocalSurvivesPullAndWinsOnAccept);
    RUN_TEST(testAuthFailureIsFatalNoRetryLoop);
    RUN_TEST(testNetworkRetrySucceedsOnSecondTry);
    std::printf("\n%d checks, %d failures\n", th::checks(), th::failures());
    return th::failures() == 0 ? 0 : 1;
}