# TodoCore — 纯 C++ 核心库（数据模型 + SQLite 仓储）
# SQLite amalgamation 来自官方 sqlite.org（public domain），SHA3-256 已核验：
#   628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e

set(SQLITE_AMALGAMATION ${CMAKE_CURRENT_LIST_DIR}/../third_party/sqlite)

add_library(todo_core STATIC
  src/TodoItem.cpp
  src/SqliteRepository.cpp
  ${SQLITE_AMALGAMATION}/sqlite3.c
)

target_include_directories(todo_core PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(todo_core PRIVATE ${SQLITE_AMALGAMATION})

# 安全默认：线程安全开启；禁止运行时加载扩展（SQLITE_OMIT_LOAD_EXTENSION）
target_compile_definitions(todo_core PRIVATE
  SQLITE_THREADSAFE=1
  SQLITE_OMIT_LOAD_EXTENSION
)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()