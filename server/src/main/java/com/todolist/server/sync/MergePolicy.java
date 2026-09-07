package com.todolist.server.sync;

import java.time.Instant;
import java.util.UUID;

/**
 * LWW 合并判定（纯函数，手写核心，单元测试覆盖）。
 * 规则（v1 契约）：
 *   1. incoming.updatedAt &gt; server.updatedAt  → 接受
 *   2. incoming.updatedAt &lt; server.updatedAt  → 冲突（服务端权威）
 *   3. 相等 → 按 deviceId 字典序兜底（确定性，防乒乓）
 */
public final class MergePolicy {

    public enum Verdict { ACCEPT, CONFLICT }

    private MergePolicy() {
    }

    public static Verdict decide(Instant incomingTime, UUID incomingDevice,
                                 Instant serverTime, UUID serverDevice) {
        int cmp = incomingTime.compareTo(serverTime);
        if (cmp > 0) {
            return Verdict.ACCEPT;
        }
        if (cmp < 0) {
            return Verdict.CONFLICT;
        }
        String a = incomingDevice == null ? "" : incomingDevice.toString();
        String b = serverDevice == null ? "" : serverDevice.toString();
        return a.compareTo(b) >= 0 ? Verdict.ACCEPT : Verdict.CONFLICT;
    }
}
