package com.todolist.server.sync;

import com.todolist.server.auth.AuthFilter;
import com.todolist.server.sync.dto.PullResponse;
import com.todolist.server.sync.dto.PushRequest;
import com.todolist.server.sync.dto.PushResponse;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import java.time.Instant;
import java.util.UUID;

@RestController
@RequestMapping("/api/v1/sync")
public class SyncController {

    private final SyncService syncService;

    public SyncController(SyncService syncService) {
        this.syncService = syncService;
    }

    @GetMapping("/pull")
    public PullResponse pull(@RequestParam("since") String since) {
        return syncService.pull(Instant.parse(since));
    }

    @PostMapping("/push")
    public PushResponse push(@RequestBody PushRequest request, HttpServletRequest http) {
        UUID deviceId = (UUID) http.getAttribute(AuthFilter.ATTR_DEVICE_ID);
        return syncService.push(deviceId, request.items());
    }
}
