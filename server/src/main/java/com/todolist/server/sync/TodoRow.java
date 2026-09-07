package com.todolist.server.sync;

import com.todolist.server.sync.dto.PushItemRequest;
import com.todolist.server.sync.dto.TodoItemDto;

import java.time.Instant;
import java.util.UUID;

/** todos 表行模型（服务端权威副本）。 */
public final class TodoRow {
    public final UUID id;
    public String content;
    public boolean isDone;
    public final Instant createdAt;
    public Instant updatedAt;
    public boolean deleted;
    public long serverVersion;
    public UUID lastModifiedBy;

    public TodoRow(UUID id, String content, boolean isDone, Instant createdAt,
                   Instant updatedAt, boolean deleted, long serverVersion, UUID lastModifiedBy) {
        this.id = id;
        this.content = content;
        this.isDone = isDone;
        this.createdAt = createdAt;
        this.updatedAt = updatedAt;
        this.deleted = deleted;
        this.serverVersion = serverVersion;
        this.lastModifiedBy = lastModifiedBy;
    }

    /** 服务端不存在 → 接受为全新行：version=1。 */
    public static TodoRow fromNew(PushItemRequest in, UUID deviceId) {
        Instant t = Instant.parse(in.updatedAt());
        Instant c = in.createdAt() == null ? t : Instant.parse(in.createdAt());
        return new TodoRow(UUID.fromString(in.id()), in.content(), in.isDone(), c,
                t, in.deleted(), 1L, deviceId);
    }

    /** LWW 判定为接受：以客户端时间为准，版本 +1。 */
    public void applyIncoming(PushItemRequest in, UUID deviceId) {
        this.content = in.content();
        this.isDone = in.isDone();
        this.updatedAt = Instant.parse(in.updatedAt());
        this.deleted = in.deleted();
        this.serverVersion += 1;
        this.lastModifiedBy = deviceId;
    }

    public TodoItemDto toDto() {
        return new TodoItemDto(id.toString(), content, isDone,
                createdAt.toString(), updatedAt.toString(), deleted, serverVersion);
    }
}
