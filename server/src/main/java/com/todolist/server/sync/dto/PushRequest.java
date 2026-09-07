package com.todolist.server.sync.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

import java.util.List;

public record PushRequest(@JsonProperty("items") List<PushItemRequest> items) {
}
