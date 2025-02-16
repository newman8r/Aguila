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

signals:
    void messageSaved(qint64 id);
    void historyLoaded(const QVector<QPair<QString, QString>> &messages);
    void error(const QString &error);

private:
    QSqlDatabase db;
    void initializeDatabase();
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
    void messageSubmitted(const QString &message);
    void sendMessageToWorker(const QString &apiKey, const QString &model, const QJsonArray &messages);
    void saveMessageToDb(int chatId, const QString &role, const QString &content);
    void loadHistoryFromDb(int chatId);

private slots:
    void onSendClicked();
    void onReturnPressed();
    void onWorkerMessageReceived(const QString &message);
    void onWorkerErrorOccurred(const QString &error);
    void initializeWebView();

private:
    Ui::DockSigint *ui;
    void appendMessage(const QString &message, bool isUser = true);
    void appendMessageToView(const QString &message, bool isUser);
    
    // Message tracking
    struct Message {
        qint64 id;  // -1 for unsaved messages
        QString role;
        QString content;
    };
    QVector<Message> messageHistory;
    
    // Worker threads
    QThread networkThread;
    QThread databaseThread;
    NetworkWorker *networkWorker;
    DatabaseWorker *databaseWorker;
    QString anthropicApiKey;
    QString currentModel;
    
    void sendToClaude(const QString &message);
    void loadEnvironmentVariables();

    // Web view related
    QWebEngineView *webView;
    QString chatHtml;
    void updateChatView();
    QString getBaseHtml();

    // Database related
    int currentChatId;
    QString getDatabasePath();
};

#endif // DOCKSIGINT_H 