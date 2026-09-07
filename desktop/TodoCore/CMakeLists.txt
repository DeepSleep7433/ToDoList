# TodoCore — 纯 C++ 核心库（数据模型 + SQLite 仓储）
# SQLite amalgamation 来自官方 sqlite.org（public domain），SHA3-256 已核验：
#   628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e

set(SQLITE_AMALGAMATION ${CMAKE_CURRENT_LIST_DIR}/../third_party/sqlite)

add_library(todo_core STATIC
  src/TodoItem.cpp
  src/SqliteRepository.cpp
  src/Json.cpp
  src/HttpClient.cpp
  src/SyncEngine.cpp
  ${SQLITE_AMALGAMATION}/sqlite3.c
)

target_include_directories(todo_core PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(todo_core PRIVATE ${SQLITE_AMALGAMATION})

if(WIN32)
  target_link_libraries(todo_core PRIVATE ws2_32)
endif()

# 安全默认：线程安全开启；禁止运行时加载扩展（SQLITE_OMIT_LOAD_EXTENSION）
target_compile_definitions(todo_core PRIVATE
  SQLITE_THREADSAFE=1
  SQLITE_OMIT_LOAD_EXTENSION
)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

# 双设备对账 E2E 工具（手动运行，非 ctest 用例）
add_executable(sync_e2e tools/sync_e2e.cpp)
target_link_libraries(sync_e2e PRIVATE todo_core)