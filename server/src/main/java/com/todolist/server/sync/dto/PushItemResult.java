package com.todolist.server.sync.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public record PushItemResult(
        @JsonProperty("id") String id,
        @JsonProperty("status") String status,          // accepted | conflict
        @JsonProperty("serverVersion") long serverVersion,
        @JsonProperty("item") TodoItemDto item) {
}
