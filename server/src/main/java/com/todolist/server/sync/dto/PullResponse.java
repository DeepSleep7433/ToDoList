package com.todolist.server.sync.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public record PullResponse(
        @JsonProperty("serverTime") String serverTime,
        @JsonProperty("items") java.util.List<TodoItemDto> items) {
}
