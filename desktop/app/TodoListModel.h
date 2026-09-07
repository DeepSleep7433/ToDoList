#pragma once

#include <memory>
#include <vector>

#include <QAbstractListModel>
#include <QString>

#include "TodoCore/SqliteRepository.h"
#include "TodoCore/TodoItem.h"

// TodoListModel — TodoCore 与 QML 之间的薄桥接层
// （不包含任何业务规则，只负责把仓储数据暴露成列表模型）
class TodoListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        ContentRole,
        DoneRole,
        CreatedMsRole,
        UpdatedMsRole,
    };

    explicit TodoListModel(QObject* parent = nullptr);

    // 打开本地库（默认路径交给 main.cpp 决定，M2 后改为 settings）
    bool openRepository(const QString& dbPath);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool ready() const { return ready_; }
    QString dbPath() const { return dbPath_; }

public slots:
    void reload();
    void addItem(const QString& content);
    void setDone(int row, bool done);
    void removeItem(int row);

signals:
    void readyChanged();

private:
    std::unique_ptr<todolist::SqliteRepository> repo_;
    std::vector<todolist::TodoItem> items_;
    QString dbPath_;
    bool ready_ = false;
};