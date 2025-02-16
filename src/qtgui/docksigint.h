#ifndef DOCKSIGINT_H
#define DOCKSIGINT_H

#include <QDockWidget>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QWebEngineView>
#include <QWebChannel>
#include <QSqlDatabase>
#include <QThread>
#include <QDateTime>

// Worker class for network operations
class NetworkWorker : public QObject
{
    Q_OBJECT
public:
    explicit NetworkWorker(QObject *parent = nullptr);
    ~NetworkWorker();

public slots:
    void sendMessage(const QString &apiKey, const QString &model, const QJsonArray &messages);

signals:
    void messageReceived(const QString &message);
    void errorOccurred(const QString &error);

private:
    QNetworkAccessManager *networkManager;
};

// Worker class for database operations
class DatabaseWorker : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseWorker(const QString &dbPath, QObject *parent = nullptr);
    ~DatabaseWorker();

public slots:
    void saveMessage(int chatId, const QString &role, const QString &content);
    void loadChatHistory(int chatId);
    void loadAllChats();
    void createChat(const QString &name);

signals:
    void messageSaved(qint64 id);
    void historyLoaded(const QVector<QPair<QString, QString>> &messages);
    void chatsLoaded(const QVector<QPair<int, QString>> &chats);
    void chatCreated(int chatId, const QString &name);
    void error(const QString &message);

private:
    QSqlDatabase db;
    void initializeDatabase();
    void verifyDatabaseState();
};

namespace Ui {
    class DockSigint;
}

/*! \brief Dock widget with SIGINT chat interface. */
class DockSigint : public QDockWidget
{
    Q_OBJECT

public:
    explicit DockSigint(QWidget *parent = nullptr);
    ~DockSigint();

    void saveSettings(QSettings *settings);
    void readSettings(QSettings *settings);

signals:
    void sendMessageToWorker(const QString &apiKey, const QString &model, const QJsonArray &messages);
    void saveMessageToDb(int chatId, const QString &role, const QString &content);
    void loadHistoryFromDb(int chatId);

private slots:
    void onSendClicked();
    void onReturnPressed();
    void onWorkerMessageReceived(const QString &message);
    void onWorkerErrorOccurred(const QString &error);
    void onNewChatClicked();
    void onChatSelected(int index);
    void onChatsLoaded(const QVector<QPair<int, QString>> &chats);

private:
    struct Message {
        qint64 id;
        QString role;
        QString content;
    };

    struct Chat {
        int id;
        QString name;
        QDateTime createdAt;
    };

    Ui::DockSigint *ui;
    QWebEngineView *webView;
    NetworkWorker *networkWorker;
    DatabaseWorker *databaseWorker;
    QThread networkThread;
    QThread databaseThread;
    QString anthropicApiKey;
    QString currentModel;
    int currentChatId;
    QVector<Message> messageHistory;
    QVector<Chat> chatList;
    QString chatHtml;

    void loadEnvironmentVariables();
    QString getBaseHtml();
    void initializeWebView();
    void updateChatView();
    void appendMessage(const QString &message, bool isUser = true);
    void appendMessageToView(const QString &message, bool isUser);
    void sendToClaude(const QString &message);
    QString getDatabasePath();
    void loadChats();
    void createNewChat();
    void switchToChat(int chatId);
    void updateChatSelector();
    void clearChat();
};

#endif // DOCKSIGINT_H 