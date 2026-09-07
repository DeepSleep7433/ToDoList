// sync_e2e — 双设备对账 E2E（需真实服务器运行在 baseUrl，如 http://localhost:8080/api/v1）
// 用法: sync_e2e <baseUrl>
// 场景：A 新建 → 同步；B 同步拿到；B 离线改；A 离线改(更晚)；各自同步 → 终态一致。
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "TodoCore/HttpClient.h"
#include "TodoCore/Json.h"
#include "TodoCore/SqliteRepository.h"
#include "TodoCore/SyncEngine.h"
#include "TodoCore/TodoItem.h"

using namespace todolist; // NOLINT

namespace {

std::string registerDevice(http::Client& http, const std::string& base, const std::string& name) {
    json::Value body = json::Value::makeObject();
    body.set("name", json::Value::makeString(name));
    http::Response r = http.post(base + "/devices", "", json::dump(body));
    if (!r.ok()) {
        std::printf("  register %s failed: %s (%d)\n", name.c_str(),
                    r.error.empty() ? "http" : r.error.c_str(), r.status);
        std::exit(2);
    }
    json::Value parsed;
    json::parse(r.body, parsed);
    const json::Value* tok = parsed.find("token");
    if (!tok) {
        std::printf("  register %s: status=%d body=%s\n", name.c_str(), r.status,
                    r.body.substr(0, 200).c_str());
    }
    return tok ? tok->asString() : "";
}

bool syncOnce(SqliteRepository& repo, http::Client& http, const std::string& base,
              const std::string& token, SyncEngine::Report& out) {
    SyncEngine engine(repo, http, base, token);
    out = engine.syncOnce();
    if (!out.ok) {
        std::printf("  sync FAILED: %s\n", out.error.c_str());
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: sync_e2e <baseUrl>\n");
        return 2;
    }
    const std::string base = argv[1];
    std::printf("== sync_e2e against %s ==\n", base.c_str());

    auto http = http::create();
    const std::string tokenA = registerDevice(*http, base, "e2e-A");
    const std::string tokenB = registerDevice(*http, base, "e2e-B");
    if (tokenA.empty() || tokenB.empty()) {
        std::printf("FAIL: 设备注册拿不到 token\n");
        return 1;
    }
    std::printf("  devices registered: A=%zu B=%zu\n", tokenA.size(), tokenB.size());

    namespace fs = std::filesystem;
    const fs::path dbA = fs::temp_directory_path() / "todolist_e2e_A.db";
    const fs::path dbB = fs::temp_directory_path() / "todolist_e2e_B.db";
    std::error_code ec;
    fs::remove(dbA, ec);
    fs::remove(dbB, ec);

    SqliteRepository repoA(dbA.string());
    SqliteRepository repoB(dbB.string());
    if (!repoA.open() || !repoB.open()) {
        std::printf("FAIL: open repos\n");
        return 1;
    }

    // 1) A 新建并同步（服务器 v1）
    TodoItem x = TodoItem::create("item-from-A");
    repoA.add(x);
    SyncEngine::Report ra;
    if (!syncOnce(repoA, *http, base, tokenA, ra)) return 1;
    std::printf("  A sync1: pushed=%d ok\n", ra.pushed);

    // 2) B 首次同步 → 拿到 X（serverVersion=1, clean）
    SyncEngine::Report rb;
    if (!syncOnce(repoB, *http, base, tokenB, rb)) return 1;
    auto onB = repoB.findById(x.id);
    if (!onB || onB->content != "item-from-A" || onB->dirty || onB->serverVersion != 1) {
        std::printf("FAIL: B 未正确拉取到 X（content=%s dirty=%d v=%lld）\n",
                    onB ? onB->content.c_str() : "(none)",
                    onB ? onB->dirty : -1,
                    onB ? static_cast<long long>(onB->serverVersion) : -1LL);
        return 1;
    }
    std::printf("  B pull: X 就位 (v1, clean)\n");

    // 3) B 先离线改（t2）；同步 → 服务端 v2
    TodoItem bEdit = *onB;
    bEdit.content = "edited-on-B(t2)";
    repoB.update(bEdit);
    if (!syncOnce(repoB, *http, base, tokenB, rb)) return 1;

    // 4) A 离线改（t3 更晚）；同步 → LWW 接受 v3
    auto onA = repoA.findById(x.id);
    TodoItem aEdit = *onA;
    aEdit.content = "edited-on-A(t3)";
    repoA.update(aEdit);
    if (!syncOnce(repoA, *http, base, tokenA, ra)) return 1;

    // 5) B 再次同步：拉取 v3（无 dirty → 采纳服务端权威）
    if (!syncOnce(repoB, *http, base, tokenB, rb)) return 1;

    // 6) 终态一致检查
    auto finA = repoA.findById(x.id);
    auto finB = repoB.findById(x.id);
    const bool same = finA && finB && finA->content == finB->content
                      && finA->serverVersion == finB->serverVersion
                      && !finA->dirty && !finB->dirty
                      && finA->content == "edited-on-A(t3)";
    std::printf("  A 终态: %s | v%lld | dirty=%d\n",
                finA ? finA->content.c_str() : "(gone)",
                finA ? static_cast<long long>(finA->serverVersion) : -1LL,
                finA ? finA->dirty : -1);
    std::printf("  B 终态: %s | v%lld | dirty=%d\n",
                finB ? finB->content.c_str() : "(gone)",
                finB ? static_cast<long long>(finB->serverVersion) : -1LL,
                finB ? finB->dirty : -1);
    if (!same) {
        std::printf("FAIL: 双设备未收敛\n");
        return 1;
    }
    std::printf("PASS: 双设备对账一致（LWW 收敛到 t3 编辑）\n");
    return 0;
}