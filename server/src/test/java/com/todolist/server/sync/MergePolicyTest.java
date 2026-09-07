package com.todolist.server.sync;

import com.todolist.server.sync.MergePolicy.Verdict;
import org.junit.jupiter.api.Test;

import java.time.Instant;
import java.util.UUID;

import static org.junit.jupiter.api.Assertions.assertEquals;

class MergePolicyTest {

    private static final Instant T = Instant.parse("2026-09-07T00:00:00Z");
    private static final UUID DEV_A = UUID.fromString("00000000-0000-4000-8000-00000000000a");
    private static final UUID DEV_B = UUID.fromString("00000000-0000-4000-8000-00000000000b");

    @Test
    void newerClientWins() {
        assertEquals(Verdict.ACCEPT, MergePolicy.decide(T.plusSeconds(5), DEV_A,
                T, DEV_B));
    }

    @Test
    void olderClientLoses() {
        assertEquals(Verdict.CONFLICT, MergePolicy.decide(T.minusSeconds(5), DEV_A,
                T, DEV_B));
    }

    @Test
    void equalTimeTieBreakByDeviceId() {
        // DEV_B > DEV_A（字典序）→ 由 B 提交的版本胜出
        assertEquals(Verdict.CONFLICT, MergePolicy.decide(T, DEV_A, T, DEV_B));
        assertEquals(Verdict.ACCEPT, MergePolicy.decide(T, DEV_B, T, DEV_A));
    }

    @Test
    void equalTimeSameDeviceSelfAccept() {
        assertEquals(Verdict.ACCEPT, MergePolicy.decide(T, DEV_A, T, DEV_A));
    }

    @Test
    void equalTimeNullServerDeviceAccepts() {
        assertEquals(Verdict.ACCEPT, MergePolicy.decide(T, DEV_A, T, null));
    }
}
