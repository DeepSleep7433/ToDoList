#include "TodoListModel.h"

#include <utility>

TodoListModel::TodoListModel(QObject* parent)
    : QAbstractListModel(parent) {}

bool TodoListModel::openRepository(const QString& dbPath) {
    repo_ = std::make_unique<todolist::SqliteRepository>(dbPath.toStdString());
    if (!repo_->open()) {
        qWarning("TodoListModel: cannot open repository at %s: %s",
                 qPrintable(dbPath), repo_->lastError().c_str());
        return false;
    }
    dbPath_ = dbPath;
    reload();
    ready_ = true;
    emit readyChanged();
    return true;
}

int TodoListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(items_.size());
}

QVariant TodoListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(items_.size()))
        return {};
    const todolist::TodoItem& it = items_[static_cast<size_t>(index.row())];
    switch (role) {
        case IdRole:
            return QString::fromStdString(it.id);
        case ContentRole:
            return QString::fromStdString(it.content);
        case DoneRole:
            return it.isDone;
        case CreatedMsRole:
            return static_cast<qint64>(it.createdAtMs);
        case UpdatedMsRole:
            return static_cast<qint64>(it.updatedAtMs);
        default:
            return {};
    }
}

QHash<int, QByteArray> TodoListModel::roleNames() const {
    return {
        {IdRole, "itemId"},
        {ContentRole, "content"},
        {DoneRole, "done"},
        {CreatedMsRole, "createdMs"},
        {UpdatedMsRole, "updatedMs"},
    };
}

void TodoListModel::reload() {
    if (!repo_) return;
    beginResetModel();
    items_ = repo_->listAll();
    endResetModel();
}

void TodoListModel::addItem(const QString& content) {
    if (!repo_ || content.trimmed().isEmpty()) return;
    repo_->add(todolist::TodoItem::create(content.trimmed().toStdString()));
    reload();
}

void TodoListModel::setDone(int row, bool done) {
    if (!repo_ || row < 0 || row >= static_cast<int>(items_.size())) return;
    todolist::TodoItem it = items_[static_cast<size_t>(row)];
    it.isDone = done;
    repo_->update(it);
    reload();
}

void TodoListModel::removeItem(int row) {
    if (!repo_ || row < 0 || row >= static_cast<int>(items_.size())) return;
    repo_->remove(items_[static_cast<size_t>(row)].id);
    reload();
}

#include "TodoListModel.moc"