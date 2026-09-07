#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "TodoCore/TodoItem.h"

namespace todolist {

// SqliteRepository — 本地 SQLite 仓储（手写 CRUD，无 ORM）
// 生命周期：构造 → open() → CRUD；失败时可用 lastError() 查看原因。
class SqliteRepository {
public:
    explicit SqliteRepository(const std::string& dbPath); // 文件路径或 ":memory:"
    ~SqliteRepository();

    SqliteRepository(const SqliteRepository&) = delete;
    SqliteRepository& operator=(const SqliteRepository&) = delete;

    // 打开（自动建库建表）。成功返回 true。
    bool open();
    bool isOpen() const;

    // --- CRUD（M1 本地能力）---
    // 新增/upsert：id 已存在则覆盖整行；自动置 dirty=1
    bool add(const TodoItem& item);
    // 按 id 更新；自动刷新 updatedAtMs 并置 dirty=1
    bool update(const TodoItem& item);
    // 逻辑删除（tombstone）：deleted=1、updatedAtMs 刷新、dirty=1
    bool remove(const std::string& id);
    // 返回未删除的条目（deleted=0），按创建时间倒序
    std::vector<TodoItem> listAll() const;
    std::optional<TodoItem> findById(const std::string& id) const;
    // 全部行数（含 tombstone），测试用
    int64_t countAll() const;

    std::string lastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace todolist