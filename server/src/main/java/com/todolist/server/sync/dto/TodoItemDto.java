package com.todolist.server.sync.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

/** 与桌面端 TodoCore 契约一致的条目（wire 格式：ISO-8601 UTC 字符串）。 */
public record TodoItemDto(
        String id,
        String content,
        @JsonProperty("isDone") boolean isDone,
        @JsonProperty("createdAt") String createdAt,
        @JsonProperty("updatedAt") String updatedAt,
        boolean deleted,
        @JsonProperty("serverVersion") long serverVersion) {
}
