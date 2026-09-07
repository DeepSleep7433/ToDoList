package com.todolist.server.sync.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

/** push 请求体条目（额外带 baseVersion，作信息记录 / 预留乐观锁）。 */
public record PushItemRequest(
        String id,
        String content,
        @JsonProperty("isDone") boolean isDone,
        @JsonProperty("createdAt") String createdAt,
        @JsonProperty("updatedAt") String updatedAt,
        boolean deleted,
        Long baseVersion) {
}
