package com.todolist.mobile.core

/** LWW 合并判定 —— 与 server/MergePolicy、桌面 SyncEngine 同一规则。 */
object MergePolicy {
    enum class Verdict { ACCEPT, CONFLICT }

    fun decide(
        incomingUpdatedMs: Long,
        incomingDevice: String?,
        serverUpdatedMs: Long,
        serverDevice: String?,
    ): Verdict {
        val cmp = incomingUpdatedMs.compareTo(serverUpdatedMs)
        if (cmp > 0) return Verdict.ACCEPT
        if (cmp < 0) return Verdict.CONFLICT
        val a = incomingDevice ?: ""
        val b = serverDevice ?: ""
        return if (a >= b) Verdict.ACCEPT else Verdict.CONFLICT
    }
}
