#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QDir>

#include "TodoListModel.h"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("ToDoList");
    app.setOrganizationName("ToDoList");

    qmlRegisterType<TodoListModel>("Todolist", 1, 0, "TodoListModel");

    QQmlApplicationEngine engine;
    engine.loadFromModule("Todolist", "Main");

    // 打开根对象上的模型并指向本地库（M2 之前仅本地）
    const auto dbDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dbDir);
    const QString dbPath = dbDir + QStringLiteral("/todolist.db");
    for (QObject* obj : engine.rootObjects()) {
        auto* model = obj->findChild<TodoListModel*>();
        if (model) model->openRepository(dbPath);
    }

    if (engine.rootObjects().isEmpty())
        return -1;
    return app.exec();
}