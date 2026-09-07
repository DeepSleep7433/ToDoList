#include <cstdio>
#include <filesystem>
#include <string>

#include "TodoCore/SqliteRepository.h"
#include "test_harness.h"

using todolist::SqliteRepository;
using todolist::TodoItem;

static void test_openMemoryAndAdd() {
    SqliteRepository repo(":memory:");
    CHECK(repo.open());
    CHECK(repo.isOpen());

    TodoItem it = TodoItem::create("买牛奶");
    CHECK(!it.id.empty());
    CHECK(it.createdAtMs > 0);
    CHECK(it.dirty); // 本地新建 → 待同步

    CHECK(repo.add(it));
    auto all = repo.listAll();
    CHECK_EQ(static_cast<long long>(all.size()), 1LL);
    CHECK(all[0].content == "买牛奶");
    CHECK(!all[0].isDone);
    CHECK(all[0].createdAtMs == it.createdAtMs);
    CHECK_EQ(all[0].serverVersion, 0LL);

    // 再次 add 同 id → upsert 覆盖
    TodoItem it2 = it;
    it2.content = "买牛奶（两盒）";
    CHECK(repo.add(it2));
    auto again = repo.listAll();
    CHECK_EQ(static_cast<long long>(again.size()), 1LL);
    CHECK(again[0].content == "买牛奶（两盒）");
}

static void testUpdatePersists() {
    SqliteRepository repo(":memory:");
    CHECK(repo.open());
    TodoItem it = TodoItem::create("写周报");
    CHECK(repo.add(it));

    TodoItem changed = it;
    changed.content = "写周报并发送";
    changed.isDone = true;
    CHECK(repo.update(changed));

    auto found = repo.findById(it.id);
    CHECK(found.has_value());
    CHECK(found->content == "写周报并发送");
    CHECK(found->isDone);
    CHECK(found->updatedAtMs >= it.updatedAtMs); // 修改时间被刷新
    CHECK(found->dirty);                         // 修改 → 待同步
    CHECK(found->createdAtMs == it.createdAtMs); // 创建时间不被改动
}

static void testLogicalDelete() {
    SqliteRepository repo(":memory:");
    CHECK(repo.open());
    TodoItem a = TodoItem::create("A");
    TodoItem b = TodoItem::create("B");
    CHECK(repo.add(a));
    CHECK(repo.add(b));
    CHECK_EQ(repo.countAll(), 2LL);

    CHECK(repo.remove(a.id));
    CHECK_EQ(repo.countAll(), 2LL);             // tombstone 仍占行
    CHECK_EQ(repo.listAll().size(), 1U);        // 列表隐藏已删除
    auto gone = repo.findById(a.id);
    CHECK(gone.has_value());
    CHECK(gone->deleted);
    CHECK(gone->dirty);                         // 删除也要同步
}

static void testPersistAcrossReopen() {
    namespace fs = std::filesystem;
    const fs::path dbFile =
        fs::temp_directory_path() / "todolist_reopen_test.db";
    std::error_code ec;
    fs::remove(dbFile, ec);

    {
        SqliteRepository repo(dbFile.string());
        CHECK(repo.open());
        CHECK(repo.add(TodoItem::create("持久化测试")));
    } // 关闭（析构）

    {
        SqliteRepository repo(dbFile.string());
        CHECK(repo.open());
        const auto all = repo.listAll();
        CHECK_EQ(static_cast<long long>(all.size()), 1LL);
        CHECK(all[0].content == "持久化测试");
    }
    fs::remove(dbFile, ec);
}

static void testNewIdUnique() {
    const std::string a = TodoItem::newId();
    const std::string b = TodoItem::newId();
    CHECK(a != b);
    CHECK_EQ(a.size(), 36U); // UUID 文本长度
    CHECK(a[14] == '4');     // version 4
}

int main() {
    RUN_TEST(test_openMemoryAndAdd);
    RUN_TEST(testUpdatePersists);
    RUN_TEST(testLogicalDelete);
    RUN_TEST(testPersistAcrossReopen);
    RUN_TEST(testNewIdUnique);

    std::printf("\n%d checks, %d failures\n", th::checks(), th::failures());
    return th::failures() == 0 ? 0 : 1;
}