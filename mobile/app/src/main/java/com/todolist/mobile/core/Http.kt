package com.todolist.mobile.core

import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStream
import java.net.HttpURLConnection
import java.net.URL

/** 极简 HTTP 客户端（与桌面 Winsock 实现对应，JVM/Android 通用）。 */
data class HttpResponse(
    val status: Int,
    val body: String,
    val error: String = "",
) {
    val ok: Boolean get() = error.isEmpty() && status in 200..299
}

interface Http {
    fun get(url: String, token: String): HttpResponse
    fun post(url: String, token: String, body: String): HttpResponse
}

class UrlConnectionHttp : Http {
    override fun get(url: String, token: String): HttpResponse = roundtrip("GET", url, token, null)

    override fun post(url: String, token: String, body: String): HttpResponse =
        roundtrip("POST", url, token, body)

    private fun roundtrip(method: String, url: String, token: String, body: String?): HttpResponse {
        return try {
            val conn = URL(url).openConnection() as HttpURLConnection
            conn.requestMethod = method
            conn.connectTimeout = 10_000
            conn.readTimeout = 10_000
            conn.setRequestProperty("Accept", "application/json")
            if (token.isNotEmpty()) conn.setRequestProperty("Authorization", "Bearer $token")
            if (body != null) {
                conn.setRequestProperty("Content-Type", "application/json")
                conn.doOutput = true
                val bytes = body.toByteArray(Charsets.UTF_8)
                conn.setRequestProperty("Content-Length", bytes.size.toString())
                conn.outputStream.use { it.write(bytes) }
            }
            val status = conn.responseCode
            val stream = if (status in 200..299) conn.inputStream else conn.errorStream
            val text = stream?.let { BufferedReader(InputStreamReader(it, Charsets.UTF_8)).use { r -> r.readText() } } ?: ""
            HttpResponse(status, text)
        } catch (e: Exception) {
            HttpResponse(0, "", e.message ?: "network error")
        }
    }
}
