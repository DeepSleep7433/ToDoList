package com.todolist.server.device;

import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Repository;

import java.util.Optional;
import java.util.UUID;

/** devices 表：手写 SQL，无 ORM。 */
@Repository
public class DeviceDao {

    private final JdbcTemplate jdbc;

    public DeviceDao(JdbcTemplate jdbc) {
        this.jdbc = jdbc;
    }

    public void insert(UUID deviceId, String tokenHash, String name) {
        jdbc.update("""
                INSERT INTO devices (device_id, token_hash, name)
                VALUES (?, ?, ?)
                """, deviceId, tokenHash, name);
    }

    /** 按 token 哈希反查设备；无则返回空。 */
    public Optional<UUID> findDeviceIdByTokenHash(String tokenHash) {
        return jdbc.query("SELECT device_id FROM devices WHERE token_hash = ?",
                        (rs, i) -> rs.getObject("device_id", UUID.class), tokenHash)
                .stream().findFirst();
    }
}
