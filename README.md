# ToDoList — 三端同步待办（C++/Qt + Java/Spring + Kotlin 路线）

一个跨端 TODO 同步小项目：**桌面端离线优先，经自建服务器中转同步**。
按"桌面 → 服务端 → 移动端"的长期路线推进，核心逻辑全部手写（无 ORM、
无现成同步库），作为 C++/Java/Kotlin 的练手与开源作品。

## 进度（M1–M4）

| 里程碑 | 内容 | 状态 |
| --- | --- | --- |
| M1 | 桌面本地 CRUD：`TodoCore`(纯 C++) + SQLite + Qt6 QML 壳 | ✅ |
| M2 | 桌面 ⇄ 服务器双向增量同步（LWW + token + 双设备对账） | ✅ |
| M3 | 打磨与开源发布（README/LICENSE/CI/冲突日志） | 🚧 本分支 |
| M4 | Android：Kotlin + Room + Retrofit 接入同一套 API | ⏳ |

## 架构

```
desktop/                       # C++20 + Qt 6（Qt Quick/QML 壳）
├─ TodoCore/                   # 纯 C++ 库（不依赖 Qt，可单测、可移植）
│  ├─ TodoItem / SqliteRepository   # 模型 + 手写 SQLite CRUD
│  ├─ Json / HttpClient             # 手写极简 JSON、Winsock HTTP（含 chunked）
│  └─ SyncEngine                    # Pull→Push→Reconcile 状态机（LWW）
├─ app/                        # QML 薄壳：列表 CRUD + 同步设置/按钮
└─ third_party/sqlite/         # 官方 amalgamation 3.53.4（public domain，SHA3 已核验）

server/                        # Java 21 + Spring Boot 3.3 + JdbcTemplate（手写 SQL）
├─ POST /api/v1/devices        # 设备注册：一次性 token（服务端只存 SHA-256）
├─ GET  /api/v1/sync/pull?since=  # 增量拉取（含 tombstone）
└─ POST /api/v1/sync/push      # 逐条 LWW 合并（时间 → deviceId 平局兜底）
```

**同步模型（v1）**：离线优先、双向增量。本地 SQLite 始终可用（`dirty` 待推送、
`server_version` 版本、`deleted` 逻辑删除）；拉取按 `since` 游标；冲突按
`updatedAt` 判定，相同时按 `deviceId` 字典序兜底；桌面端采纳服务端权威项并把
明细记入 `<db>.conflicts.log`。已知取舍：纯 LWW 依赖客户端时钟，极端偏移可能
丢改动（刻意简化的 v1 语义）。

## 快速开始

### 服务端
```bash
# PostgreSQL（任选其一）
docker compose -f server/compose.yaml up -d        # 或本机原生 PG 16
# 启动（默认连 localhost:5432, 库/账号 todolist/todolist，可用环境变量覆盖）
cd server && mvn spring-boot:run
```

### 桌面端
前置：CMake + MSVC（或 MinGW）+ Qt 6.x（msvc2022_64）。Qt 可用 aqt 安装：
```bash
pip install aqtinstall
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -O C:/Qt
```
构建：
```bash
cmake -S desktop -B desktop/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build desktop/build
ctest --test-dir desktop/build --output-on-failure   # 仓储/JSON/SyncEngine 全绿
desktop/build/app/todolist_app.exe                   # 运行（Qt DLL 用 windeployqt 部署）
```
使用：填服务器地址（默认 `http://localhost:8080/api/v1`）→「注册设备」→「立即同步」。

### 双设备对账 E2E（需服务器在线）
```bash
desktop/build/TodoCore/sync_e2e http://localhost:8080/api/v1
# PASS: 双设备对账一致（LWW 收敛）
```

## 测试
- 桌面：`test_repository`（持久化/删除/脏标记）、`test_json`、`test_syncengine`（LWW 六场景）
- 服务端：`MergePolicyTest`（纯逻辑 5 例）
- E2E：`sync_e2e`（真实 HTTP ⇄ 真实 PostgreSQL 双设备收敛）

## 路线图（后续）
- M3 收尾：CI 已内置；补截图文档、包体积优化
- M4 Android：Kotlin data class 重写数据模型、Room DAO、Retrofit 调同套 API

## License
[Apache-2.0](./LICENSE)
