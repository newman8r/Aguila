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

private slots:
    void onSendClicked();
    void onReturnPressed();
    void onNetworkReply(QNetworkReply *reply);
    void initializeWebView();

private:
    Ui::DockSigint *ui;
    void appendMessage(const QString &message, bool isUser = true);
    void appendMessageToView(const QString &message, bool isUser);
    
    // Claude API integration
    QNetworkAccessManager *networkManager;
    QString anthropicApiKey;
    QString currentModel;
    QVector<QPair<QString, QString>> messageHistory;  // pairs of role,content
    
    void sendToClaude(const QString &message);
    void loadEnvironmentVariables();

    // Web view related
    QWebEngineView *webView;
    QString chatHtml;
    void updateChatView();
    QString getBaseHtml();

    // Database related
    QSqlDatabase db;
    int currentChatId;
    void initializeDatabase();
    void loadChatHistory();
    void saveMessage(const QString &role, const QString &content);
    QString getDatabasePath();
};

#endif // DOCKSIGINT_H 