#include "TodoCore/SqliteRepository.h"

#include <sqlite3.h>
#include <memory>

namespace todolist {

namespace {
constexpr const char* kCreateTable =
    "CREATE TABLE IF NOT EXISTS todos ("
    "  id             TEXT PRIMARY KEY,"
    "  content        TEXT NOT NULL,"
    "  is_done        INTEGER NOT NULL DEFAULT 0,"
    "  created_at     INTEGER NOT NULL,"
    "  updated_at     INTEGER NOT NULL,"
    "  deleted        INTEGER NOT NULL DEFAULT 0,"
    "  server_version INTEGER NOT NULL DEFAULT 0,"
    "  dirty          INTEGER NOT NULL DEFAULT 0"
    ");";
}

struct SqliteRepository::Impl {
    sqlite3* db = nullptr;
    bool open = false;
    std::string error;
    std::string path;

    ~Impl() {
        if (db) sqlite3_close(db);
    }
};

SqliteRepository::SqliteRepository(const std::string& dbPath)
    : d_(new Impl) {
    d_->path = dbPath;
}

SqliteRepository::~SqliteRepository() = default;

bool SqliteRepository::open() {
    if (d_->open) return true;
    const int rc = sqlite3_open(d_->path.c_str(), &d_->db);
    if (rc != SQLITE_OK) {
        d_->error = sqlite3_errmsg(d_->db);
        return false;
    }
    char* errmsg = nullptr;
    if (sqlite3_exec(d_->db, kCreateTable, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        d_->error = errmsg ? errmsg : "CREATE TABLE failed";
        sqlite3_free(errmsg);
        sqlite3_close(d_->db);
        d_->db = nullptr;
        return false;
    }
    d_->error.clear();
    d_->open = true;
    return true;
}

bool SqliteRepository::isOpen() const { return d_->open; }

bool SqliteRepository::add(const TodoItem& item) {
    if (!d_->open) return false;
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "INSERT OR REPLACE INTO todos"
        "(id, content, is_done, created_at, updated_at, deleted, server_version, dirty)"
        " VALUES(?,?,?,?,?,?,?,?);";
    if (sqlite3_prepare_v2(d_->db, sql, -1, &st, nullptr) != SQLITE_OK) {
        d_->error = sqlite3_errmsg(d_->db);
        return false;
    }
    sqlite3_bind_text(st, 1, item.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, item.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 3, item.isDone ? 1 : 0);
    sqlite3_bind_int64(st, 4, item.createdAtMs);
    sqlite3_bind_int64(st, 5, item.updatedAtMs);
    sqlite3_bind_int(st, 6, item.deleted ? 1 : 0);
    sqlite3_bind_int64(st, 7, item.serverVersion);
    sqlite3_bind_int(st, 8, item.dirty ? 1 : 0);
    const bool ok = sqlite3_step(st) == SQLITE_DONE;
    if (!ok) d_->error = sqlite3_errmsg(d_->db);
    sqlite3_finalize(st);
    return ok;
}

bool SqliteRepository::update(const TodoItem& item) {
    if (!d_->open) return false;
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "UPDATE todos SET content=?, is_done=?, updated_at=?, deleted=?,"
        " server_version=?, dirty=? WHERE id=?;";
    if (sqlite3_prepare_v2(d_->db, sql, -1, &st, nullptr) != SQLITE_OK) {
        d_->error = sqlite3_errmsg(d_->db);
        return false;
    }
    sqlite3_bind_text(st, 1, item.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, item.isDone ? 1 : 0);
    sqlite3_bind_int64(st, 3, TodoItem::nowMs()); // 刷新修改时间（LWW 时钟）
    sqlite3_bind_int(st, 4, item.deleted ? 1 : 0);
    sqlite3_bind_int64(st, 5, item.serverVersion);
    sqlite3_bind_int(st, 6, 1); // dirty = 待推送
    sqlite3_bind_text(st, 7, item.id.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(st) == SQLITE_DONE;
    if (!ok) d_->error = sqlite3_errmsg(d_->db);
    sqlite3_finalize(st);
    return ok;
}

bool SqliteRepository::remove(const std::string& id) {
    if (!d_->open) return false;
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "UPDATE todos SET deleted=1, updated_at=?, dirty=1 WHERE id=?;";
    if (sqlite3_prepare_v2(d_->db, sql, -1, &st, nullptr) != SQLITE_OK) {
        d_->error = sqlite3_errmsg(d_->db);
        return false;
    }
    sqlite3_bind_int64(st, 1, TodoItem::nowMs());
    sqlite3_bind_text(st, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(st) == SQLITE_DONE;
    if (!ok) d_->error = sqlite3_errmsg(d_->db);
    sqlite3_finalize(st);
    return ok;
}

namespace {
TodoItem rowToItem(sqlite3_stmt* st) {
    TodoItem it;
    const unsigned char* id = sqlite3_column_text(st, 0);
    const unsigned char* content = sqlite3_column_text(st, 1);
    it.id = id ? reinterpret_cast<const char*>(id) : "";
    it.content = content ? reinterpret_cast<const char*>(content) : "";
    it.isDone = sqlite3_column_int(st, 2) != 0;
    it.createdAtMs = sqlite3_column_int64(st, 3);
    it.updatedAtMs = sqlite3_column_int64(st, 4);
    it.deleted = sqlite3_column_int(st, 5) != 0;
    it.serverVersion = sqlite3_column_int64(st, 6);
    it.dirty = sqlite3_column_int(st, 7) != 0;
    return it;
}
} // namespace

std::vector<TodoItem> SqliteRepository::listAll() const {
    std::vector<TodoItem> out;
    if (!d_->open) return out;
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "SELECT id, content, is_done, created_at, updated_at, deleted, server_version, dirty"
        " FROM todos WHERE deleted=0 ORDER BY created_at DESC, id ASC;";
    if (sqlite3_prepare_v2(d_->db, sql, -1, &st, nullptr) != SQLITE_OK) {
        d_->error = sqlite3_errmsg(d_->db);
        return out;
    }
    while (sqlite3_step(st) == SQLITE_ROW) out.push_back(rowToItem(st));
    if (sqlite3_errmsg(d_->db)[0] != '\0' && sqlite3_errcode(d_->db) != SQLITE_DONE)
        d_->error = sqlite3_errmsg(d_->db);
    sqlite3_finalize(st);
    return out;
}

std::optional<TodoItem> SqliteRepository::findById(const std::string& id) const {
    if (!d_->open) return std::nullopt;
    sqlite3_stmt* st = nullptr;
    const char* sql =
        "SELECT id, content, is_done, created_at, updated_at, deleted, server_version, dirty"
        " FROM todos WHERE id=?;";
    if (sqlite3_prepare_v2(d_->db, sql, -1, &st, nullptr) != SQLITE_OK) {
        d_->error = sqlite3_errmsg(d_->db);
        return std::nullopt;
    }
    sqlite3_bind_text(st, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<TodoItem> out;
    if (sqlite3_step(st) == SQLITE_ROW) out = rowToItem(st);
    sqlite3_finalize(st);
    return out;
}

int64_t SqliteRepository::countAll() const {
    if (!d_->open) return -1;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(d_->db, "SELECT COUNT(*) FROM todos;", -1, &st, nullptr) != SQLITE_OK) {
        d_->error = sqlite3_errmsg(d_->db);
        return -1;
    }
    int64_t n = -1;
    if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return n;
}

std::string SqliteRepository::lastError() const { return d_->error; }

} // namespace todolist