# 每个测试文件独立可执行（各自有 main），避免符号冲突
foreach(t test_repository test_json test_syncengine)
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE todo_core)
  add_test(NAME ${t} COMMAND ${t})
endforeach()