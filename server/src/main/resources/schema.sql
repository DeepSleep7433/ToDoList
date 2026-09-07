-- v1 冻结 schema（与桌面端 TodoCore 字段同语义）
-- 由 Spring 启动时执行；全部幂等。

CREATE TABLE IF NOT EXISTS devices (
    device_id   UUID PRIMARY KEY,
    token_hash  TEXT        NOT NULL UNIQUE,   -- SHA-256(deviceToken)，明文只下发一次
    name        TEXT        NOT NULL DEFAULT '',
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS todos (
    id              UUID PRIMARY KEY,
    content         TEXT        NOT NULL,
    is_done         BOOLEAN     NOT NULL DEFAULT FALSE,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
    deleted         BOOLEAN     NOT NULL DEFAULT FALSE,
    server_version  BIGINT      NOT NULL DEFAULT 1,
    last_modified_by UUID       NULL         -- 平局兜底：updated_at 相同时按 device_id 决胜负
);

CREATE INDEX IF NOT EXISTS idx_todos_updated_at ON todos (updated_at);
CREATE INDEX IF NOT EXISTS idx_todos_deleted    ON todos (deleted);
