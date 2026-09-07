#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>

#include "SyncController.h"
#include "TodoListModel.h"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("ToDoList");
    app.setOrganizationName("ToDoList");

    const auto dbDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dbDir);
    const QString dbPath = dbDir + QStringLiteral("/todolist.db");

    TodoListModel model;
    model.openRepository(dbPath);

    SyncController sync(dbPath, &model);
    sync.loadSettings();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("todos", &model);
    engine.rootContext()->setContextProperty("sync", &sync);
    engine.loadFromModule("Todolist", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;
    return app.exec();
}