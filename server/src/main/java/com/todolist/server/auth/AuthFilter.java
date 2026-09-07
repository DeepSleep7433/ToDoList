package com.todolist.server.auth;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.todolist.server.device.DeviceDao;
import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.http.MediaType;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;

/**
 * Bearer token 认证：除「设备注册」与 OPTIONS 外，所有 /api 请求必须带
 * Authorization: Bearer <token>。通过后把 deviceId 放入 request attribute。
 */
@Component
public class AuthFilter extends OncePerRequestFilter {

    public static final String ATTR_DEVICE_ID = "deviceId";

    private final DeviceDao deviceDao;
    private final ObjectMapper mapper = new ObjectMapper();

    public AuthFilter(DeviceDao deviceDao) {
        this.deviceDao = deviceDao;
    }

    @Override
    protected boolean shouldNotFilter(HttpServletRequest req) {
        String uri = req.getRequestURI();
        boolean isRegister = "POST".equalsIgnoreCase(req.getMethod())
                && uri.equals("/api/v1/devices");
        boolean isOptions = "OPTIONS".equalsIgnoreCase(req.getMethod());
        return isRegister || isOptions;
    }

    @Override
    protected void doFilterInternal(HttpServletRequest req, HttpServletResponse res,
                                    FilterChain chain) throws ServletException, IOException {
        String header = req.getHeader("Authorization");
        if (header == null || !header.startsWith("Bearer ")) {
            unauthorized(res, "missing_token", "缺少 Authorization: Bearer <token>");
            return;
        }
        String token = header.substring("Bearer ".length()).trim();
        if (token.isEmpty()) {
            unauthorized(res, "missing_token", "token 为空");
            return;
        }
        Optional<UUID> deviceId = deviceDao.findDeviceIdByTokenHash(TokenUtil.sha256Hex(token));
        if (deviceId.isEmpty()) {
            unauthorized(res, "bad_token", "token 无效");
            return;
        }
        req.setAttribute(ATTR_DEVICE_ID, deviceId.get());
        chain.doFilter(req, res);
    }

    private void unauthorized(HttpServletResponse res, String code, String message) throws IOException {
        res.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
        res.setContentType(MediaType.APPLICATION_JSON_VALUE);
        mapper.writeValue(res.getWriter(), Map.of("error", code, "message", message));
    }
}
