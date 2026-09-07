# Qt6 QML 薄壳：只做视图，数据全部走 TodoCore
set(CMAKE_AUTOMOC ON)

qt_add_executable(todolist_app
  main.cpp
  TodoListModel.h
  TodoListModel.cpp
  SyncController.h
  SyncController.cpp
)

qt_add_qml_module(todolist_app
  URI Todolist
  VERSION 1.0
  QML_FILES qml/Main.qml
)

target_link_libraries(todolist_app PRIVATE
  Qt6::Quick
  Qt6::Qml
  Qt6::Gui
  todo_core
)

target_include_directories(todolist_app PRIVATE ${CMAKE_CURRENT_LIST_DIR})