#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <thread>

class TodoListModel;

// SyncController — 把 TodoCore::SyncEngine 接进 UI（后台线程执行，不卡界面）
class SyncController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString token READ token WRITE setToken NOTIFY tokenChanged)
    Q_PROPERTY(bool syncing READ syncing NOTIFY syncingChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    SyncController(const QString& dbPath, TodoListModel* model, QObject* parent = nullptr);
    ~SyncController() override;

    QString serverUrl() const { return serverUrl_; }
    void setServerUrl(const QString& url);
    QString token() const { return token_; }
    void setToken(const QString& token);
    bool syncing() const { return syncing_; }
    QString statusText() const { return statusText_; }

    // 从 QSettings 恢复配置；在 QML 就绪后调用一次
    Q_INVOKABLE void loadSettings();
    Q_INVOKABLE void saveSettings();
    // 注册设备：POST {server}/devices → 拿到 token 并保存
    Q_INVOKABLE void registerDevice();
    // 后台跑一次完整同步
    Q_INVOKABLE void startSync();

signals:
    void serverUrlChanged();
    void tokenChanged();
    void syncingChanged();
    void statusTextChanged();
    // ok=false 时 error 说明原因；conflicts>0 表示本次采纳了服务端权威项
    void syncDone(bool ok, int pushed, int pulled, int conflicts, const QString& error);
    void deviceRegistered(bool ok, const QString& message);

private:
    void setSyncing(bool on);
    void setStatusText(const QString& text);
    void finishFromWorker(std::function<void()> fn);
    void runRegisterWorker(const QString& base, const QString& url);
    void runSyncWorker(const QString& base, const QString& url);

    TodoListModel* model_ = nullptr;
    QString dbPath_;
    QString serverUrl_;
    QString token_;
    QString statusText_;
    bool syncing_ = false;
    std::atomic<bool> stop_{false};
    std::thread worker_;
};
