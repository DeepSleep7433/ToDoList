#include "SyncController.h"

#include <QDebug>
#include <QMetaObject>
#include <QSettings>
#include <QThread>
#include <functional>

#include "TodoCore/HttpClient.h"
#include "TodoCore/Json.h"
#include "TodoCore/SqliteRepository.h"
#include "TodoCore/SyncEngine.h"
#include "TodoListModel.h"

namespace {
constexpr const char* kKeyServer = "sync/serverUrl";
constexpr const char* kKeyToken = "sync/token";
constexpr const char* kDefaultServer = "http://localhost:8080/api/v1";
} // namespace

SyncController::SyncController(const QString& dbPath, TodoListModel* model, QObject* parent)
    : QObject(parent), model_(model), dbPath_(dbPath) {}

SyncController::~SyncController() {
    stop_ = true;
    if (worker_.joinable()) worker_.join();
}

void SyncController::loadSettings() {
    QSettings s;
    serverUrl_ = s.value(kKeyServer, QString(kDefaultServer)).toString();
    token_ = s.value(kKeyToken).toString();
    emit serverUrlChanged();
    emit tokenChanged();
    setStatusText(token_.isEmpty()
                      ? QStringLiteral("未注册设备：填服务器地址后点「注册设备」")
                      : QStringLiteral("设备已注册，点「同步」"));
}

void SyncController::saveSettings() {
    QSettings s;
    s.setValue(kKeyServer, serverUrl_);
    s.setValue(kKeyToken, token_);
}

void SyncController::setServerUrl(const QString& url) {
    if (serverUrl_ == url) return;
    serverUrl_ = url.trimmed();
    emit serverUrlChanged();
}

void SyncController::setToken(const QString& token) {
    if (token_ == token) return;
    token_ = token.trimmed();
    emit tokenChanged();
}

void SyncController::setSyncing(bool on) {
    if (syncing_ == on) return;
    syncing_ = on;
    emit syncingChanged();
}

void SyncController::setStatusText(const QString& text) {
    if (statusText_ == text) return;
    statusText_ = text;
    emit statusTextChanged();
}

// 把 worker 线程的收尾切回主线程（避免跨线程碰 QObject/model）
void SyncController::finishFromWorker(std::function<void()> fn) {
    if (QThread::currentThread() == thread()) {
        fn();
        return;
    }
    QMetaObject::invokeMethod(this, std::move(fn), Qt::QueuedConnection);
}

void SyncController::registerDevice() {
    if (syncing_ || serverUrl_.isEmpty()) return;
    setSyncing(true);
    setStatusText(QStringLiteral("正在注册设备…"));
    saveSettings();
    const QString base = serverUrl_;
    if (worker_.joinable()) worker_.join();
    worker_ = std::thread([this, base] { runRegisterWorker(base, serverUrl_); });
}

void SyncController::runRegisterWorker(const QString&, const QString& base) {
    auto client = todolist::http::create();
    todolist::json::Value body = todolist::json::Value::makeObject();
    body.set("name", todolist::json::Value::makeString("desktop-qt"));
    todolist::http::Response r =
        client->post(base.toStdString() + "/devices", "", todolist::json::dump(body));
    if (!r.ok()) {
        const QString msg = QStringLiteral("注册失败(%1)：%2")
                                .arg(r.status)
                                .arg(QString::fromStdString(r.error.empty() ? r.body : r.error));
        finishFromWorker([this, msg] {
            setSyncing(false);
            setStatusText(msg);
            emit deviceRegistered(false, msg);
        });
        return;
    }
    todolist::json::Value parsed;
    if (!todolist::json::parse(r.body, parsed)) {
        finishFromWorker([this] {
            setSyncing(false);
            setStatusText(QStringLiteral("注册失败：响应解析错误"));
            emit deviceRegistered(false, statusText());
        });
        return;
    }
    const todolist::json::Value* tok = parsed.find("token");
    const QString newToken = (tok && tok->isString())
        ? QString::fromStdString(tok->asString())
        : QString();
    finishFromWorker([this, newToken] {
        setSyncing(false);
        if (newToken.isEmpty()) {
            setStatusText(QStringLiteral("注册失败：响应里没有 token"));
            emit deviceRegistered(false, statusText());
            return;
        }
        setToken(newToken);
        saveSettings();
        setStatusText(QStringLiteral("注册成功，设备 token 已保存"));
        emit deviceRegistered(true, QStringLiteral("设备已注册"));
    });
}

void SyncController::startSync() {
    if (syncing_) return;
    if (token_.isEmpty()) {
        setStatusText(QStringLiteral("请先注册设备"));
        return;
    }
    setSyncing(true);
    setStatusText(QStringLiteral("同步中…"));
    saveSettings();
    const QString base = serverUrl_;
    const QString token = token_;
    const QString dbPath = dbPath_;
    if (worker_.joinable()) worker_.join();
    worker_ = std::thread([this, base, token, dbPath] { runSyncWorker(base, token); });
    Q_UNUSED(dbPath);
}

void SyncController::runSyncWorker(const QString& base, const QString& token) {
    todolist::SqliteRepository repo(dbPath_.toStdString());
    if (!repo.open()) {
        const QString msg = QStringLiteral("本地库打开失败");
        finishFromWorker([this, msg] {
            setSyncing(false);
            setStatusText(msg);
            emit syncDone(false, 0, 0, 0, msg);
        });
        return;
    }
    auto client = todolist::http::create();
    todolist::SyncEngine engine(repo, *client, base.toStdString(), token.toStdString());
    todolist::SyncEngine::Report rep = engine.syncOnce();
    const int pushed = rep.pushed;
    const int pulled = rep.pulled;
    const int conflicts = rep.conflicts;
    const bool ok = rep.ok;
    const QString error = QString::fromStdString(rep.error);
    finishFromWorker([this, ok, pushed, pulled, conflicts, error] {
        setSyncing(false);
        if (ok) {
            setStatusText(QStringLiteral("同步完成：推送 %1 · 拉取 %2 · 冲突 %3")
                              .arg(pushed).arg(pulled).arg(conflicts));
            if (model_) model_->reload();
        } else {
            setStatusText(QStringLiteral("同步失败：%1").arg(error));
        }
        emit syncDone(ok, pushed, pulled, conflicts, error);
    });
}

#include "SyncController.moc"