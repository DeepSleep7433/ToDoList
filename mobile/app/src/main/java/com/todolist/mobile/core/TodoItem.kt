package com.todolist.mobile.core

/** 与桌面 TodoCore::TodoItem 同语义的 v1 数据模型（epoch ms UTC）。 */
data class TodoItem(
    val id: String,
    val content: String,
    val isDone: Boolean = false,
    val createdAtMs: Long = 0,
    val updatedAtMs: Long = 0,
    val deleted: Boolean = false,
    val serverVersion: Long = 0,
    val dirty: Boolean = false,
) {
    companion object {
        fun newId(): String = java.util.UUID.randomUUID().toString()

        fun create(content: String, nowMs: Long = System.currentTimeMillis()): TodoItem =
            TodoItem(
                id = newId(),
                content = content,
                createdAtMs = nowMs,
                updatedAtMs = nowMs,
                dirty = true,
            )

        fun nowMs(): Long = System.currentTimeMillis()
    }
}
