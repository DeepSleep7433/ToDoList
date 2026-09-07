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
    // 返回待推送条目（dirty=1，含 tombstone），按修改时间升序（M2 同步用）
    std::vector<TodoItem> listDirty() const;
    std::optional<TodoItem> findById(const std::string& id) const;
    // 全部行数（含 tombstone），测试用
    int64_t countAll() const;

    // 键值元数据（同步游标等；独立 meta 表）
    std::optional<std::string> getMeta(const std::string& key) const;
    bool setMeta(const std::string& key, const std::string& value);

    // 服务端权威覆盖：全字段（含时间/版本）按传入写入，dirty=0（M2 同步用）
    bool syncFromServer(const TodoItem& item);

    std::string lastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace todolist