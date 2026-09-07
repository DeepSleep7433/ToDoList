package com.todolist.server.device;

import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.Map;

@RestController
@RequestMapping("/api/v1/devices")
public class DeviceController {

    private final DeviceService deviceService;

    public DeviceController(DeviceService deviceService) {
        this.deviceService = deviceService;
    }

    @PostMapping
    public ResponseEntity<Map<String, String>> register(@RequestBody(required = false) Map<String, String> body) {
        String name = body == null ? null : body.get("name");
        DeviceService.DeviceRegistration r = deviceService.register(name);
        return ResponseEntity.status(HttpStatus.CREATED).body(Map.of(
                "deviceId", r.deviceId().toString(),
                "token", r.token()));
    }
}
