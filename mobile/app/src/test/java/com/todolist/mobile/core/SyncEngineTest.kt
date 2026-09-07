package com.todolist.mobile.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class SyncEngineTest {

    private val base = "http://s/api/v1"

    private class FakeHttp(
        var handler: (url: String, method: String, body: String?) -> HttpResponse,
    ) : Http {
        override fun get(url: String, token: String) = handler(url, "GET", null)
        override fun post(url: String, token: String, body: String) =
            handler(url, "POST", body)
    }

    private fun httpOk(body: String) = HttpResponse(200, body)

    private val epochSince = "http://s/api/v1/sync/pull?since=1970-01-01T00:00:00.000Z"

    @Test
    fun pushDirtyAcceptedAndCursor() {
        val store = InMemoryTodoStore()
        val a = TodoItem.create("本地新建")
        store.upsertLocal(a)
        var sawSince = false
        val http = FakeHttp { url, method, _ ->
            when {
                url.contains("/pull") -> {
                    if (url == epochSince) sawSince = true
                    httpOk("""{"serverTime":"2026-09-07T00:00:01.000Z","items":[]}""")
                }
                method == "POST" -> httpOk(
                    """{"results":[{"id":"${a.id}","status":"accepted","serverVersion":1,""" +
                        """"item":{"id":"${a.id}","content":"本地新建","isDone":false,""" +
                        """"createdAt":"2026-09-07T00:00:00.000Z","updatedAt":"2026-09-07T00:00:00.000Z",""" +
                        """"deleted":false,"serverVersion":1}}]}"""
                )
                else -> HttpResponse(500, "", "no")
            }
        }
        val rep = SyncEngine(store, http, base, "tok").syncOnce()
        assertTrue(rep.ok)
        assertEquals(1, rep.pushed)
        assertEquals(0, rep.conflicts)
        assertTrue(sawSince)
        val after = store.findById(a.id)!!
        assertEquals(false, after.dirty)
        assertEquals(1L, after.serverVersion)
        assertEquals("2026-09-07T00:00:01.000Z", store.getMeta("sync_cursor"))
    }

    @Test
    fun pullServerNewerOverwrites() {
        val store = InMemoryTodoStore()
        val local = TodoItem.create("本地旧内容").copy(serverVersion = 1, dirty = false)
        store.upsertLocal(local)
        val http = FakeHttp { url, _, _ ->
            if (url.contains("/pull")) httpOk(
                """{"serverTime":"2026-09-07T00:00:02.000Z","items":[{""" +
                    """"id":"${local.id}","content":"服务端新内容","isDone":true,""" +
                    """"createdAt":"2026-09-07T00:00:00.000Z","updatedAt":"2026-09-07T00:00:02.000Z",""" +
                    """"deleted":false,"serverVersion":2}]}"""
            ) else httpOk("""{"serverTime":"2026-09-07T00:00:02.000Z","items":[]}""")
        }
        val rep = SyncEngine(store, http, base, "tok").syncOnce()
        assertTrue(rep.ok)
        assertEquals(1, rep.pulled)
        val after = store.findById(local.id)!!
        assertEquals("服务端新内容", after.content)
        assertTrue(after.isDone)
        assertEquals(false, after.dirty)
        assertEquals(2L, after.serverVersion)
    }

    @Test
    fun conflictAdoptsServerItem() {
        val store = InMemoryTodoStore()
        val local = TodoItem.create("本地较旧修改")
        store.upsertLocal(local)
        val http = FakeHttp { url, method, _ ->
            when {
                url.contains("/pull") -> httpOk("""{"serverTime":"2026-09-07T00:00:01.000Z","items":[]}""")
                method == "POST" -> httpOk(
                    """{"results":[{"id":"${local.id}","status":"conflict","serverVersion":3,""" +
                        """"item":{"id":"${local.id}","content":"服务端权威","isDone":false,""" +
                        """"createdAt":"2026-09-07T00:00:00.000Z","updatedAt":"2026-09-07T00:00:03.000Z",""" +
                        """"deleted":false,"serverVersion":3}}]}"""
                )
                else -> HttpResponse(500, "", "no")
            }
        }
        val rep = SyncEngine(store, http, base, "tok").syncOnce()
        assertTrue(rep.ok)
        assertEquals(1, rep.conflicts)
        val after = store.findById(local.id)!!
        assertEquals("服务端权威", after.content)
        assertEquals(false, after.dirty)
        assertEquals(3L, after.serverVersion)
    }

    @Test
    fun dirtyLocalSurvivesPullAndWinsOnAccept() {
        val store = InMemoryTodoStore()
        val local = TodoItem.create("离线编辑内容")
        store.upsertLocal(local)
        val http = FakeHttp { url, method, _ ->
            when {
                url.contains("/pull") -> httpOk(
                    """{"serverTime":"2026-09-07T00:00:01.000Z","items":[{""" +
                        """"id":"${local.id}","content":"服务端v99","isDone":false,""" +
                        """"createdAt":"2026-09-07T00:00:00.000Z","updatedAt":"2026-09-07T00:00:01.000Z",""" +
                        """"deleted":false,"serverVersion":99}]}"""
                )
                method == "POST" -> httpOk(
                    """{"results":[{"id":"${local.id}","status":"accepted","serverVersion":1,""" +
                        """"item":{"id":"${local.id}","content":"离线编辑内容","isDone":false,""" +
                        """"createdAt":"2026-09-07T00:00:00.000Z","updatedAt":"2026-09-07T00:00:00.000Z",""" +
                        """"deleted":false,"serverVersion":1}}]}"""
                )
                else -> HttpResponse(500, "", "no")
            }
        }
        val rep = SyncEngine(store, http, base, "tok").syncOnce()
        assertTrue(rep.ok)
        val after = store.findById(local.id)!!
        assertEquals("离线编辑内容", after.content)
        assertEquals(1L, after.serverVersion)
    }

    @Test
    fun authFailureIsFatal() {
        val store = InMemoryTodoStore()
        val http = FakeHttp { _, _, _ -> HttpResponse(401, """{"error":"bad_token"}""") }
        val rep = SyncEngine(store, http, base, "bad").syncOnce()
        assertTrue(!rep.ok)
        assertEquals(SyncEngine.Phase.FAILED, rep.phase)
        assertEquals(1, rep.attempts)
    }

    @Test
    fun networkRetrySucceedsOnSecondTry() {
        val store = InMemoryTodoStore()
        var calls = 0
        val http = FakeHttp { url, _, _ ->
            calls++
            if (calls == 1) HttpResponse(0, "", "connection reset")
            else if (url.contains("/pull")) httpOk("""{"serverTime":"2026-09-07T00:00:01.000Z","items":[]}""")
            else httpOk("""{"serverTime":"2026-09-07T00:00:01.000Z","items":[]}""")
        }
        val rep = SyncEngine(store, http, base, "tok").syncOnce()
        assertTrue(rep.ok)
        assertEquals(2, rep.attempts)
    }
}
