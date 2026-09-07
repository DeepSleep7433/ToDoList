package com.todolist.server.device;

import com.todolist.server.auth.TokenUtil;
import org.springframework.stereotype.Service;

import java.util.UUID;

@Service
public class DeviceService {

    private final DeviceDao deviceDao;

    public DeviceService(DeviceDao deviceDao) {
        this.deviceDao = deviceDao;
    }

    /** 注册设备：签发一次性 token（服务端只存 SHA-256）。 */
    public DeviceRegistration register(String name) {
        UUID deviceId = UUID.randomUUID();
        String token = TokenUtil.randomToken();
        String cleanName = name == null ? "" : name;
        deviceDao.insert(deviceId, TokenUtil.sha256Hex(token), cleanName);
        return new DeviceRegistration(deviceId, token);
    }

    public record DeviceRegistration(UUID deviceId, String token) {
    }
}
