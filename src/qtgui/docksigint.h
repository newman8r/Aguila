#ifndef DOCKSIGINT_H
#define DOCKSIGINT_H

#include <QDockWidget>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

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

private:
    Ui::DockSigint *ui;
    void appendMessage(const QString &message, bool isUser = true);
    
    // Claude API integration
    QNetworkAccessManager *networkManager;
    QString anthropicApiKey;
    QString currentModel;
    QVector<QPair<QString, QString>> messageHistory;  // pairs of role,content
    
    void sendToClaude(const QString &message);
    void loadEnvironmentVariables();
};

#endif // DOCKSIGINT_H 