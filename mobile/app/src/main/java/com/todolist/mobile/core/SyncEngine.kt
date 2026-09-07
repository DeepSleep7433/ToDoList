package com.todolist.mobile.core

import kotlinx.serialization.encodeToString

/** SyncEngine —— 离线优先增量同步（Kotlin 版，流程镜像桌面 SyncEngine）。 */
class SyncEngine(
    private val store: TodoStore,
    private val http: Http,
    private val baseUrl: String,
    private val token: String,
) {
    enum class Phase { IDLE, PULL, PUSH, RECONCILE, DONE, FAILED }

    data class Report(
        val ok: Boolean,
        val phase: Phase,
        val pushed: Int = 0,
        val pulled: Int = 0,
        val conflicts: Int = 0,
        val attempts: Int = 0,
        val error: String = "",
        val conflictDetails: List<String> = emptyList(),
    )

    companion object {
        private const val EPOCH = "1970-01-01T00:00:00.000Z"
        private const val MAX_ATTEMPTS = 3
    }

    fun syncOnce(): Report {
        var cursor = store.getMeta("sync_cursor") ?: EPOCH
        var last = Report(ok = false, phase = Phase.FAILED)
        for (attempt in 1..MAX_ATTEMPTS) {
            last = attemptSync(cursor)
            last = last.copy(attempts = attempt)
            if (last.ok || last.phase == Phase.FAILED) return last
            Thread.sleep(300L * attempt) // 退避后重试，游标不变
        }
        return last.copy(ok = false, phase = Phase.FAILED, error = "network failure after retries")
    }

    private fun attemptSync(cursor: String): Report {
        // ---- PULL ----
        val pullResp = http.get("$baseUrl/sync/pull?since=$cursor", token)
        if (!pullResp.ok) {
            return Report(
                ok = false,
                phase = if (pullResp.status == 401) Phase.FAILED else Phase.PULL,
                error = pullResp.error.ifEmpty { "pull http ${pullResp.status}" },
            )
        }
        val pull: PullResponse = try {
            wireJson.decodeFromString(pullResp.body)
        } catch (e: Exception) {
            return Report(ok = false, phase = Phase.FAILED, error = "pull 解析失败")
        }
        var pulled = 0
        for (w in pull.items) {
            val incoming = w.toItem()
            val local = store.findById(incoming.id)
            when {
                local == null -> {
                    store.syncFromServer(incoming)
                    pulled++
                }
                local.dirty -> Unit // 留给 push
                incoming.serverVersion > local.serverVersion -> {
                    store.syncFromServer(incoming)
                    pulled++
                }
            }
        }

        // ---- PUSH ----
        var pushed = 0
        var conflicts = 0
        val details = mutableListOf<String>()
        val dirty = store.listDirty()
        if (dirty.isNotEmpty()) {
            val req = PushRequest(dirty.map { WireItem.from(it) })
            val pushResp = http.post("$baseUrl/sync/push", token, wireJson.encodeToString(req))
            if (!pushResp.ok) {
                return Report(
                    ok = false,
                    phase = if (pushResp.status == 401) Phase.FAILED else Phase.PUSH,
                    error = pushResp.error.ifEmpty { "push http ${pushResp.status}" },
                )
            }
            val parsed: PushResponse = try {
                wireJson.decodeFromString(pushResp.body)
            } catch (e: Exception) {
                return Report(ok = false, phase = Phase.FAILED, error = "push 解析失败")
            }
            pushed = dirty.size
            for (r in parsed.results) {
                when (r.status) {
                    "conflict" -> {
                        store.syncFromServer(r.item.toItem())
                        conflicts++
                        details += "${r.id}|${r.item.content}|v${r.serverVersion}"
                    }
                    else -> { // accepted
                        val wire = r.item
                        val existing = store.findById(wire.id)
                        if (existing != null) store.syncFromServer(wire.toItem())
                        else store.upsertLocal(wire.toItem())
                    }
                }
            }
        }

        // ---- RECONCILE ----
        store.setMeta("sync_cursor", pull.serverTime)
        return Report(ok = true, phase = Phase.DONE, pushed = pushed, pulled = pulled,
            conflicts = conflicts, conflictDetails = details)
    }
}
