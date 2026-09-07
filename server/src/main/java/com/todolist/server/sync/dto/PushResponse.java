package com.todolist.server.sync.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

import java.util.List;

public record PushResponse(@JsonProperty("results") List<PushItemResult> results) {
}
