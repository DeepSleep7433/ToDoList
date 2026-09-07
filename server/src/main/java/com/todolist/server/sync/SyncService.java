package com.todolist.server.sync;

import com.todolist.server.common.ApiException;
import com.todolist.server.sync.MergePolicy.Verdict;
import com.todolist.server.sync.dto.PushItemRequest;
import com.todolist.server.sync.dto.PushItemResult;
import com.todolist.server.sync.dto.PushResponse;
import com.todolist.server.sync.dto.PullResponse;
import com.todolist.server.sync.dto.TodoItemDto;
import org.springframework.http.HttpStatus;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.UUID;

@Service
public class SyncService {

    private final TodoDao todoDao;

    public SyncService(TodoDao todoDao) {
        this.todoDao = todoDao;
    }

    /** 增量拉取：返回 since 之后所有变更（含 tombstone）。 */
    public PullResponse pull(Instant since) {
        List<TodoItemDto> items = todoDao.findUpdatedAfter(since).stream()
                .map(TodoRow::toDto)
                .toList();
        return new PullResponse(Instant.now().toString(), items);
    }

    /** 逐条 LWW 合并推送；同一事务内原子完成。 */
    @Transactional
    public PushResponse push(UUID deviceId, List<PushItemRequest> items) {
        List<PushItemResult> results = new ArrayList<>(items.size());
        for (PushItemRequest in : items) {
            UUID id;
            try {
                id = UUID.fromString(in.id());
            } catch (IllegalArgumentException e) {
                throw new ApiException(HttpStatus.BAD_REQUEST, "bad_id",
                        "非法条目 id: " + in.id());
            }
            Optional<TodoRow> server = todoDao.findById(id);
            if (server.isEmpty()) {
                TodoRow row = TodoRow.fromNew(in, deviceId);
                todoDao.insert(row);
                results.add(accepted(row));
                continue;
            }
            TodoRow existing = server.get();
            Verdict verdict = MergePolicy.decide(
                    Instant.parse(in.updatedAt()), deviceId,
                    existing.updatedAt, existing.lastModifiedBy);
            if (verdict == Verdict.ACCEPT) {
                existing.applyIncoming(in, deviceId);
                todoDao.update(existing);
                results.add(accepted(existing));
            } else {
                results.add(new PushItemResult(existing.id.toString(), "conflict",
                        existing.serverVersion, existing.toDto()));
            }
        }
        return new PushResponse(results);
    }

    private static PushItemResult accepted(TodoRow row) {
        return new PushItemResult(row.id.toString(), "accepted",
                row.serverVersion, row.toDto());
    }
}
