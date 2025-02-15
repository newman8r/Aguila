#include <QDebug>
#include <QFile>
#include <QDir>
#include <QApplication>
#include <QTimer>
#include <QStandardPaths>
#include "docksigint.h"
#include "ui_docksigint.h"

// Custom message handler to ensure debug messages are displayed
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtInfoMsg:
        fprintf(stderr, "Info: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        abort();
    }
}

DockSigint::DockSigint(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::DockSigint)
{
    // Install custom message handler
    qInstallMessageHandler(messageHandler);
    
    ui->setupUi(this);

    // Initialize network manager
    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished, this, &DockSigint::onNetworkReply);

    // Connect signals/slots
    connect(ui->sendButton, &QPushButton::clicked, this, &DockSigint::onSendClicked);
    connect(ui->chatInput, &QLineEdit::returnPressed, this, &DockSigint::onReturnPressed);

    // Load environment variables
    loadEnvironmentVariables();

    // Set initial welcome message
    ui->chatDisplay->append("<span style='color: #569cd6;'>Welcome to the SIGINT Chat Interface!</span>");
    ui->chatDisplay->append("<span style='color: #569cd6;'>Connected to: " + currentModel + "</span>");
    
    qDebug() << "\n=== SIGINT Panel Initialization ===";
    qDebug() << "App directory:" << QCoreApplication::applicationDirPath();
    qDebug() << "Current working directory:" << QDir::currentPath();
    qDebug() << "Config directory:" << QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gqrx";
    qDebug() << "Model:" << currentModel;
    qDebug() << "API Key status:" << (!anthropicApiKey.isEmpty() ? "Found" : "Missing");
    qDebug() << "=================================\n";
}

DockSigint::~DockSigint()
{
    delete ui;
}

void DockSigint::loadEnvironmentVariables()
{
    qDebug() << "\n=== Loading Environment Variables ===";
    
    // Try multiple possible locations for .env
    QStringList possiblePaths = {
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gqrx/.env",  // ~/.config/gqrx/.env
        QDir::currentPath() + "/.env",
        QCoreApplication::applicationDirPath() + "/.env",
        QCoreApplication::applicationDirPath() + "/../.env",
        QCoreApplication::applicationDirPath() + "/../../.env"
    };

    bool found = false;
    for (const QString &path : possiblePaths) {
        qDebug() << "Trying .env path:" << path;
        QFile envFile(path);
        if (envFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "Successfully opened .env at:" << path;
            while (!envFile.atEnd()) {
                QString line = envFile.readLine().trimmed();
                if (line.startsWith("ANTHROPIC_API_KEY=")) {
                    anthropicApiKey = line.mid(18).trimmed();
                    qDebug() << "API key found, length:" << anthropicApiKey.length();
                }
                else if (line.startsWith("AI_MODEL=")) {
                    currentModel = line.mid(9).trimmed();
                    qDebug() << "Model found:" << currentModel;
                }
            }
            envFile.close();
            found = true;
            break;
        } else {
            qDebug() << "Could not open file:" << path << "Error:" << envFile.errorString();
        }
    }

    if (!found) {
        qDebug() << "Failed to find .env file in any location";
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gqrx";
        qDebug() << "Please create .env file in:" << configDir;
        qDebug() << "Required format:";
        qDebug() << "ANTHROPIC_API_KEY=your_api_key_here";
        qDebug() << "AI_MODEL=claude-3-opus-20240229";
        anthropicApiKey = "";
        currentModel = "claude-3-opus-20240229"; // fallback
    }
    qDebug() << "=================================\n";
}

void DockSigint::saveSettings(QSettings *settings)
{
    if (!settings)
        return;

    settings->beginGroup("SIGINT");
    // Add settings as needed
    settings->endGroup();
}

void DockSigint::readSettings(QSettings *settings)
{
    if (!settings)
        return;

    settings->beginGroup("SIGINT");
    // Add settings as needed
    settings->endGroup();
}

void DockSigint::onSendClicked()
{
    QString message = ui->chatInput->text().trimmed();
    if (!message.isEmpty()) {
        appendMessage(message);
        sendToClaude(message);
        ui->chatInput->clear();
    }
}

void DockSigint::onReturnPressed()
{
    onSendClicked();
}

void DockSigint::appendMessage(const QString &message, bool isUser)
{
    QString formattedMessage;
    if (isUser) {
        formattedMessage = QString("<div style='margin: 5px;'><span style='color: #4ec9b0;'>User:</span> %1</div>")
                          .arg(message.toHtmlEscaped());
    } else {
        formattedMessage = QString("<div style='margin: 5px;'><span style='color: #569cd6;'>Assistant:</span> %1</div>")
                          .arg(message.toHtmlEscaped());
    }
    ui->chatDisplay->append(formattedMessage);
}

void DockSigint::sendToClaude(const QString &message)
{
    qDebug() << "Preparing to send message to Claude...";
    
    if (anthropicApiKey.isEmpty()) {
        qDebug() << "Error: API key is empty";
        appendMessage("Error: API key not found. Please check your .env file.", false);
        return;
    }

    // Prepare the messages array
    messageHistory.append(qMakePair(QString("user"), message));
    
    QJsonArray messages;
    for (const auto &msg : messageHistory) {
        messages.append(QJsonObject{
            {"role", msg.first},
            {"content", msg.second}
        });
    }

    // Create the request body
    QJsonObject requestBody{
        {"model", currentModel},
        {"messages", messages},
        {"max_tokens", 4096},
        {"temperature", 0.7}
    };

    // Log the request details
    qDebug() << "Request URL: https://api.anthropic.com/v1/messages";
    qDebug() << "Request body:" << QJsonDocument(requestBody).toJson();

    // Prepare the network request
    QNetworkRequest request(QUrl("https://api.anthropic.com/v1/messages"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-api-key", anthropicApiKey.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");

    // Send the request
    qDebug() << "Sending request to Claude...";
    networkManager->post(request, QJsonDocument(requestBody).toJson());
}

void DockSigint::onNetworkReply(QNetworkReply *reply)
{
    qDebug() << "Received network reply";
    qDebug() << "Reply status:" << reply->error();
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        qDebug() << "Response received:" << response;
        
        QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
        QJsonObject jsonObject = jsonResponse.object();

        if (jsonObject.contains("content")) {
            QString assistantMessage = jsonObject["content"].toArray()[0].toObject()["text"].toString();
            qDebug() << "Extracted message:" << assistantMessage;
            messageHistory.append(qMakePair(QString("assistant"), assistantMessage));
            appendMessage(assistantMessage, false);
        }
        else {
            qDebug() << "Response does not contain 'content' field";
            qDebug() << "Full response object:" << jsonResponse.toJson();
        }
    }
    else {
        QString errorMessage = QString("Error: %1").arg(reply->errorString());
        qDebug() << "Network error:" << reply->errorString();
        qDebug() << "Error details:" << reply->readAll();
        appendMessage(errorMessage, false);
    }

    reply->deleteLater();
} 