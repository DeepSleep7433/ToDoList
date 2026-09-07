package com.todolist.server.sync;

import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Repository;

import java.sql.Timestamp;
import java.time.Instant;
import java.util.List;
import java.util.Optional;
import java.util.UUID;

/** todos 表数据访问：手写 SQL，无 ORM。 */
@Repository
public class TodoDao {

    private final JdbcTemplate jdbc;

    public TodoDao(JdbcTemplate jdbc) {
        this.jdbc = jdbc;
    }

    public Optional<TodoRow> findById(UUID id) {
        return jdbc.query("""
                        SELECT id, content, is_done, created_at, updated_at, deleted,
                               server_version, last_modified_by
                        FROM todos WHERE id = ?
                        """, (rs, i) -> new TodoRow(
                        rs.getObject("id", UUID.class),
                        rs.getString("content"),
                        rs.getBoolean("is_done"),
                        rs.getTimestamp("created_at").toInstant(),
                        rs.getTimestamp("updated_at").toInstant(),
                        rs.getBoolean("deleted"),
                        rs.getLong("server_version"),
                        rs.getObject("last_modified_by", UUID.class)),
                id).stream().findFirst();
    }

    /** 增量拉取：updated_at &gt; since，含 tombstone，按时间升序。 */
    public List<TodoRow> findUpdatedAfter(Instant since) {
        return jdbc.query("""
                        SELECT id, content, is_done, created_at, updated_at, deleted,
                               server_version, last_modified_by
                        FROM todos WHERE updated_at > ? ORDER BY updated_at ASC
                        """, (rs, i) -> new TodoRow(
                        rs.getObject("id", UUID.class),
                        rs.getString("content"),
                        rs.getBoolean("is_done"),
                        rs.getTimestamp("created_at").toInstant(),
                        rs.getTimestamp("updated_at").toInstant(),
                        rs.getBoolean("deleted"),
                        rs.getLong("server_version"),
                        rs.getObject("last_modified_by", UUID.class)),
                Timestamp.from(since));
    }

    public void insert(TodoRow row) {
        jdbc.update("""
                        INSERT INTO todos
                            (id, content, is_done, created_at, updated_at, deleted,
                             server_version, last_modified_by)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                        """,
                row.id, row.content, row.isDone,
                Timestamp.from(row.createdAt), Timestamp.from(row.updatedAt),
                row.deleted, row.serverVersion, row.lastModifiedBy);
    }

    public void update(TodoRow row) {
        jdbc.update("""
                        UPDATE todos
                        SET content = ?, is_done = ?, updated_at = ?, deleted = ?,
                            server_version = ?, last_modified_by = ?
                        WHERE id = ?
                        """,
                row.content, row.isDone, Timestamp.from(row.updatedAt),
                row.deleted, row.serverVersion, row.lastModifiedBy, row.id);
    }
}
