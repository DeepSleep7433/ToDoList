package com.todolist.mobile.core

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json

// ---- wire 契约（与 server/桌面相同字段）----

@Serializable
data class WireItem(
    val id: String,
    val content: String,
    @SerialName("isDone") val isDone: Boolean = false,
    @SerialName("createdAt") val createdAt: String,
    @SerialName("updatedAt") val updatedAt: String,
    val deleted: Boolean = false,
    @SerialName("serverVersion") val serverVersion: Long = 0,
    val baseVersion: Long? = null,
) {
    fun toItem(): TodoItem = TodoItem(
        id = id,
        content = content,
        isDone = isDone,
        createdAtMs = parseIsoMs(createdAt),
        updatedAtMs = parseIsoMs(updatedAt),
        deleted = deleted,
        serverVersion = serverVersion,
        dirty = false,
    )

    companion object {
        fun from(item: TodoItem): WireItem = WireItem(
            id = item.id,
            content = item.content,
            isDone = item.isDone,
            createdAt = formatIsoMs(item.createdAtMs),
            updatedAt = formatIsoMs(item.updatedAtMs),
            deleted = item.deleted,
            baseVersion = item.serverVersion,
        )
    }
}

@Serializable
data class PullResponse(
    @SerialName("serverTime") val serverTime: String,
    @SerialName("items") val items: List<WireItem>,
)

@Serializable
data class PushRequest(@SerialName("items") val items: List<WireItem>)

@Serializable
data class PushItemResult(
    val id: String,
    val status: String,                 // accepted | conflict
    @SerialName("serverVersion") val serverVersion: Long,
    val item: WireItem,
)

@Serializable
data class PushResponse(@SerialName("results") val results: List<PushItemResult>)

/** 手写 JSON 编解码入口：忽略未知字段，向前兼容。 */
val wireJson: Json = Json { ignoreUnknownKeys = true; encodeDefaults = true }

/** ms → ISO-8601 UTC（毫秒精度，Z） */
fun formatIsoMs(ms: Long): String =
    java.time.Instant.ofEpochMilli(ms).toString()

/** ISO-8601 → ms（兼容任意小数位） */
fun parseIsoMs(iso: String): Long =
    java.time.Instant.parse(iso).toEpochMilli()
