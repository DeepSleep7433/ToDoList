package com.todolist.mobile.core

/** 仓储抽象：Room 实现（真机阶段）与内存实现（JVM 测试）都实现本接口。 */
interface TodoStore {
    fun findById(id: String): TodoItem?
    fun listAll(): List<TodoItem>            // 未删除
    fun listDirty(): List<TodoItem>          // 含 tombstone，按 updatedAt 升序
    fun upsertLocal(item: TodoItem)          // 本地 CRUD 落库（保持 item.dirty）
    fun syncFromServer(item: TodoItem)       // 服务端权威覆盖，dirty=false
    fun getMeta(key: String): String?
    fun setMeta(key: String, value: String)
}

/** JVM/单测用内存实现。 */
class InMemoryTodoStore : TodoStore {
    private val rows = LinkedHashMap<String, TodoItem>()
    private val meta = HashMap<String, String>()

    override fun findById(id: String): TodoItem? = rows[id]

    override fun listAll(): List<TodoItem> =
        rows.values.filter { !it.deleted }.sortedByDescending { it.createdAtMs }

    override fun listDirty(): List<TodoItem> =
        rows.values.filter { it.dirty }.sortedBy { it.updatedAtMs }

    override fun upsertLocal(item: TodoItem) {
        rows[item.id] = item
    }

    override fun syncFromServer(item: TodoItem) {
        rows[item.id] = item.copy(dirty = false)
    }

    override fun getMeta(key: String): String? = meta[key]

    override fun setMeta(key: String, value: String) {
        meta[key] = value
    }
}
