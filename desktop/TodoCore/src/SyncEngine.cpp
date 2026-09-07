#include "TodoCore/SyncEngine.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "TodoCore/Json.h"

namespace todolist {
namespace {

constexpr const char* kEpochSince = "1970-01-01T00:00:00.000Z";

// ---- 时间：epoch ms ↔ ISO-8601 UTC（毫秒，Z 结尾）----
std::string isoFromMs(int64_t ms) {
    time_t sec = static_cast<time_t>(ms / 1000);
    const int msec = static_cast<int>(ms % 1000);
    std::tm t{};
#ifdef _WIN32
    gmtime_s(&t, &sec);
#else
    gmtime_r(&sec, &t);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", &t);
    char out[40];
    std::snprintf(out, sizeof out, "%s.%03dZ", buf, msec);
    return out;
}

int64_t msFromIso(const std::string& iso) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
    int ms = 0;
    const int n = std::sscanf(iso.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &s);
    if (n < 6) return 0;
    const auto dot = iso.find('.');
    if (dot != std::string::npos) {
        int digits = 0;
        for (size_t i = dot + 1; i < iso.size() && iso[i] >= '0' && iso[i] <= '9' && digits < 3; ++i, ++digits)
            ms = ms * 10 + (iso[i] - '0');
        for (; digits < 3; ++digits) ms *= 10;
    }
    std::tm t{};
    t.tm_year = y - 1900;
    t.tm_mon = mo - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min = mi;
    t.tm_sec = s;
#if defined(_WIN32)
    const time_t secs = _mkgmtime(&t);
#else
    const time_t secs = timegm(&t);
#endif
    return static_cast<int64_t>(secs) * 1000 + ms;
}

// ---- TodoItem ↔ wire JSON ----
json::Value itemToWire(const TodoItem& it) {
    json::Value o = json::Value::makeObject();
    o.set("id", json::Value::makeString(it.id));
    o.set("content", json::Value::makeString(it.content));
    o.set("isDone", json::Value::makeBool(it.isDone));
    o.set("createdAt", json::Value::makeString(isoFromMs(it.createdAtMs)));
    o.set("updatedAt", json::Value::makeString(isoFromMs(it.updatedAtMs)));
    o.set("deleted", json::Value::makeBool(it.deleted));
    o.set("baseVersion", json::Value::makeNumber(static_cast<double>(it.serverVersion)));
    return o;
}

TodoItem itemFromWire(const json::Value& o) {
    TodoItem it;
    if (const json::Value* v = o.find("id")) it.id = v->asString();
    if (const json::Value* v = o.find("content")) it.content = v->asString();
    if (const json::Value* v = o.find("isDone")) it.isDone = v->asBool();
    if (const json::Value* v = o.find("deleted")) it.deleted = v->asBool();
    if (const json::Value* v = o.find("createdAt")) it.createdAtMs = msFromIso(v->asString());
    if (const json::Value* v = o.find("updatedAt")) it.updatedAtMs = msFromIso(v->asString());
    if (const json::Value* v = o.find("serverVersion")) it.serverVersion = v->asInt64();
    it.dirty = false; // 来自服务端的都是干净行
    return it;
}

} // namespace

SyncEngine::SyncEngine(SqliteRepository& repo, http::Client& http,
                       std::string baseUrl, std::string token)
    : repo_(repo), http_(http), baseUrl_(std::move(baseUrl)),
      token_(std::move(token)) {}

SyncEngine::Report SyncEngine::syncOnce() {
    Report rep;
    cursor_ = repo_.getMeta("sync_cursor").value_or(kEpochSince);
    int attempts = 0;
    for (; attempts < kMaxAttempts; ++attempts) {
        rep = attemptSync();
        rep.attempts = attempts + 1;
        if (rep.ok || rep.phase == Phase::Failed) return rep;
        // 其余：网络/5xx → 重试
    }
    rep.phase = Phase::Failed;
    rep.attempts = attempts;
    if (rep.error.empty()) rep.error = "network failure after retries";
    return rep;
}

SyncEngine::Report SyncEngine::attemptSync() {
    Report rep;
    rep.phase = Phase::Pull;

    const std::string pullUrl = baseUrl_ + "/sync/pull?since=" + cursor_;
    http::Response r = http_.get(pullUrl, token_);
    if (!r.ok()) {
        rep.error = r.error.empty()
            ? ("pull http " + std::to_string(r.status))
            : r.error;
        if (r.status == 401) rep.phase = Phase::Failed;
        return rep;
    }
    rep.pulled += applyPullItems(r.body);

    rep.phase = Phase::Push;
    const std::vector<TodoItem> dirty = repo_.listDirty();
    if (!dirty.empty()) {
        json::Value arr = json::Value::makeArray();
        for (const TodoItem& it : dirty) arr.arr.push_back(itemToWire(it));
        json::Value body = json::Value::makeObject();
        body.set("items", std::move(arr));
        const std::string reqBody = json::dump(body);
        http::Response pr = http_.post(baseUrl_ + "/sync/push", token_, reqBody);
        if (!pr.ok()) {
            rep.error = pr.error.empty()
                ? ("push http " + std::to_string(pr.status))
                : pr.error;
            if (pr.status == 401) rep.phase = Phase::Failed;
            return rep;
        }
        rep.conflicts += applyPushResults(pr.body, reqBody);
        rep.pushed = static_cast<int>(dirty.size());
    }

    rep.phase = Phase::Reconcile;
    if (!cursor_.empty()) repo_.setMeta("sync_cursor", cursor_);

    rep.phase = Phase::Done;
    rep.ok = true;
    return rep;
}

int SyncEngine::applyPullItems(const std::string& body) {
    json::Value root;
    std::string err;
    if (!json::parse(body, root, &err)) return 0; // 解析失败：服务端异常格式
    int applied = 0;
    const json::Value* serverTime = root.find("serverTime");
    if (serverTime && serverTime->isString()) cursor_ = serverTime->asString();
    const json::Value* items = root.find("items");
    if (!items || !items->isArray()) return 0;
    for (const json::Value& el : items->asArray()) {
        TodoItem wire = itemFromWire(el);
        std::optional<TodoItem> local = repo_.findById(wire.id);
        if (!local) {
            repo_.add(wire); // dirty=false → 直接入库
            ++applied;
            continue;
        }
        if (local->dirty) continue;               // 本地有未推送修改 → 让 push 阶段处理
        if (wire.serverVersion > local->serverVersion) {
            repo_.syncFromServer(wire);
            ++applied;
        }
    }
    return applied;
}

int SyncEngine::applyPushResults(const std::string& body, const std::string&) {
    json::Value root;
    std::string err;
    if (!json::parse(body, root, &err)) return 0;
    const json::Value* results = root.find("results");
    if (!results || !results->isArray()) return 0;
    int conflicts = 0;
    for (const json::Value& el : results->asArray()) {
        const json::Value* status = el.find("status");
        if (!status || !status->isString()) continue;
        const json::Value* item = el.find("item");
        if (!item || !item->isObject()) continue;
        const json::Value* idv = el.find("id");
        const std::string id = (idv && idv->isString()) ? idv->asString() : "";
        if (status->asString() == "accepted") {
            TodoItem wire = itemFromWire(*item);
            if (!wire.id.empty() && repo_.findById(wire.id))
                repo_.syncFromServer(wire);
            else if (!wire.id.empty())
                repo_.add(wire);
        } else if (status->asString() == "conflict") {
            TodoItem wire = itemFromWire(*item);
            if (!wire.id.empty() && repo_.findById(wire.id))
                repo_.syncFromServer(wire);
            else if (!wire.id.empty())
                repo_.add(wire);
            ++conflicts;
            (void)id;
        }
    }
    return conflicts;
}

} // namespace todolist