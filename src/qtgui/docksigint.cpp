#include <QDebug>
#include <QFile>
#include <QDir>
#include <QApplication>
#include <QTimer>
#include <QStandardPaths>
#include <QClipboard>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebChannel>
#include <QVBoxLayout>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QThread>
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

// NetworkWorker implementation
NetworkWorker::NetworkWorker(QObject *parent) : QObject(parent)
{
    networkManager = new QNetworkAccessManager(this);
}

NetworkWorker::~NetworkWorker()
{
    delete networkManager;
}

void NetworkWorker::sendMessage(const QString &apiKey, const QString &model, const QJsonArray &messages)
{
    // Create the request body
    QJsonObject requestBody{
        {"model", model},
        {"messages", messages},
        {"max_tokens", 4096},
        {"temperature", 0.7}
    };

    // Prepare the network request
    QNetworkRequest request(QUrl("https://api.anthropic.com/v1/messages"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-api-key", apiKey.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");

    // Send the request and connect to its finished signal
    QNetworkReply *reply = networkManager->post(request, QJsonDocument(requestBody).toJson());
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
            QJsonObject jsonObject = jsonResponse.object();

            if (jsonObject.contains("content")) {
                QString assistantMessage = jsonObject["content"].toArray()[0].toObject()["text"].toString();
                emit messageReceived(assistantMessage);
            }
            else {
                emit errorOccurred("Error: Response does not contain 'content' field");
            }
        }
        else {
            emit errorOccurred(QString("Error: %1").arg(reply->errorString()));
        }
        reply->deleteLater();
    });
}

// DatabaseWorker implementation
DatabaseWorker::DatabaseWorker(const QString &dbPath, QObject *parent) : QObject(parent)
{
    db = QSqlDatabase::addDatabase("QSQLITE", "WorkerConnection");
    db.setDatabaseName(dbPath);
    initializeDatabase();
}

DatabaseWorker::~DatabaseWorker()
{
    if (db.isOpen())
        db.close();
}

void DatabaseWorker::initializeDatabase()
{
    if (!db.open()) {
        emit error("Error opening database: " + db.lastError().text());
        return;
    }

    QSqlQuery query(db);
    
    // Create chats table
    if (!query.exec("CREATE TABLE IF NOT EXISTS chats ("
                   "id INTEGER PRIMARY KEY,"
                   "name TEXT NOT NULL,"
                   "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                   ")")) {
        emit error("Error creating chats table: " + query.lastError().text());
        return;
    }

    // Create messages table
    if (!query.exec("CREATE TABLE IF NOT EXISTS messages ("
                   "id INTEGER PRIMARY KEY,"
                   "chat_id INTEGER,"
                   "role TEXT NOT NULL,"
                   "content TEXT NOT NULL,"
                   "timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                   "FOREIGN KEY (chat_id) REFERENCES chats(id)"
                   ")")) {
        emit error("Error creating messages table: " + query.lastError().text());
        return;
    }

    // Create default chat if it doesn't exist
    query.prepare("INSERT OR IGNORE INTO chats (id, name) VALUES (?, ?)");
    query.addBindValue(1);
    query.addBindValue("Chat 1");
    if (!query.exec()) {
        emit error("Error creating default chat: " + query.lastError().text());
    }
}

void DatabaseWorker::saveMessage(int chatId, const QString &role, const QString &content)
{
    if (!db.isOpen() && !db.open()) {
        emit error("Database not open");
        return;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO messages (chat_id, role, content) VALUES (?, ?, ?)");
    query.addBindValue(chatId);
    query.addBindValue(role);
    query.addBindValue(content);
    
    if (!query.exec()) {
        emit error("Error saving message: " + query.lastError().text());
        return;
    }

    emit messageSaved(query.lastInsertId().toLongLong());
}

void DatabaseWorker::loadChatHistory(int chatId)
{
    if (!db.isOpen() && !db.open()) {
        emit error("Database not open");
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT role, content FROM messages WHERE chat_id = ? ORDER BY timestamp ASC");
    query.addBindValue(chatId);
    
    if (!query.exec()) {
        emit error("Error loading chat history: " + query.lastError().text());
        return;
    }

    QVector<QPair<QString, QString>> messages;
    while (query.next()) {
        messages.append(qMakePair(
            query.value(0).toString(),
            query.value(1).toString()
        ));
    }

    emit historyLoaded(messages);
}

DockSigint::DockSigint(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::DockSigint),
    currentChatId(1)
{
    // Install custom message handler
    qInstallMessageHandler(messageHandler);
    
    ui->setupUi(this);

    // Initialize web view
    webView = new QWebEngineView(ui->chatDisplay);
    webView->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    webView->settings()->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
    webView->page()->setWebChannel(new QWebChannel(this));
    auto *layout = new QVBoxLayout(ui->chatDisplay);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(webView);
    ui->chatDisplay->setLayout(layout);

    // Initialize network worker
    networkWorker = new NetworkWorker();
    networkWorker->moveToThread(&networkThread);
    connect(&networkThread, &QThread::finished, networkWorker, &QObject::deleteLater);
    connect(this, &DockSigint::sendMessageToWorker, networkWorker, &NetworkWorker::sendMessage);
    connect(networkWorker, &NetworkWorker::messageReceived, this, &DockSigint::onWorkerMessageReceived);
    connect(networkWorker, &NetworkWorker::errorOccurred, this, &DockSigint::onWorkerErrorOccurred);
    networkThread.start();

    // Initialize database worker
    databaseWorker = new DatabaseWorker(getDatabasePath());
    databaseWorker->moveToThread(&databaseThread);
    connect(&databaseThread, &QThread::finished, databaseWorker, &QObject::deleteLater);
    connect(this, &DockSigint::saveMessageToDb, databaseWorker, &DatabaseWorker::saveMessage);
    connect(this, &DockSigint::loadHistoryFromDb, databaseWorker, &DatabaseWorker::loadChatHistory);
    connect(databaseWorker, &DatabaseWorker::messageSaved, this, [this](qint64 id) {
        if (!messageHistory.isEmpty())
            messageHistory.last().id = id;
    });
    connect(databaseWorker, &DatabaseWorker::historyLoaded, this, [this](const QVector<QPair<QString, QString>> &messages) {
        messageHistory.clear();
        for (const auto &msg : messages) {
            Message newMsg;
            newMsg.id = -1;
            newMsg.role = msg.first;
            newMsg.content = msg.second;
            messageHistory.append(newMsg);
            appendMessageToView(newMsg.content, newMsg.role == "user");
        }
    });
    databaseThread.start();

    // Connect signals/slots
    connect(ui->sendButton, &QPushButton::clicked, this, &DockSigint::onSendClicked);
    connect(ui->chatInput, &QLineEdit::returnPressed, this, &DockSigint::onReturnPressed);

    // Load environment variables
    loadEnvironmentVariables();

    // Initialize chat HTML
    chatHtml = getBaseHtml();
    
    // Connect loadFinished signal before setting HTML content
    connect(webView->page(), &QWebEnginePage::loadFinished, this, [this](bool ok) {
        if (ok) {
            // Initialize JavaScript functions after page loads
            webView->page()->runJavaScript(
                "window.scrollToBottom = function() {"
                "    const container = document.getElementById('chat-container');"
                "    if (container) container.scrollTop = container.scrollHeight;"
                "};"
                "window.appendMessage = function(html) {"
                "    const messages = document.getElementById('messages');"
                "    if (messages) {"
                "        messages.insertAdjacentHTML('beforeend', html);"
                "        scrollToBottom();"
                "    }"
                "};"
                "if (!document.getElementById('messages')) {"
                "    const container = document.getElementById('chat-container');"
                "    if (container) {"
                "        const messages = document.createElement('div');"
                "        messages.id = 'messages';"
                "        container.appendChild(messages);"
                "    }"
                "}"
            );
            
            // Load chat history
            emit loadHistoryFromDb(currentChatId);
            if (messageHistory.isEmpty()) {
                appendMessage("Welcome to the SIGINT Chat Interface!", false);
                appendMessage("Connected to: " + currentModel, false);
            }
        }
    });

    // Set the HTML content
    updateChatView();
    
    qDebug() << "\n=== SIGINT Panel Initialization ===";
    qDebug() << "App directory:" << QCoreApplication::applicationDirPath();
    qDebug() << "Current working directory:" << QDir::currentPath();
    qDebug() << "Config directory:" << QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gqrx";
    qDebug() << "Database path:" << getDatabasePath();
    qDebug() << "Model:" << currentModel;
    qDebug() << "API Key status:" << (!anthropicApiKey.isEmpty() ? "Found" : "Missing");
    qDebug() << "=================================\n";
}

DockSigint::~DockSigint()
{
    networkThread.quit();
    networkThread.wait();
    databaseThread.quit();
    databaseThread.wait();
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

QString DockSigint::getBaseHtml()
{
    return QString(
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<style>"
        "html, body {"
        "    margin: 0;"
        "    padding: 0;"
        "    height: 100%;"
        "    background: #1e1e1e;"
        "    color: #d4d4d4;"
        "    font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", Roboto, \"Helvetica Neue\", Arial, sans-serif;"
        "}"
        "#chat-container {"
        "    padding: 16px;"
        "    height: 100%;"
        "    overflow-y: auto;"
        "    scroll-behavior: smooth;"
        "    display: flex;"
        "    flex-direction: column;"
        "}"
        "#messages {"
        "    flex-grow: 1;"
        "    min-height: min-content;"
        "}"
        ".message {"
        "    margin: 16px 0;"
        "    opacity: 0;"
        "    transform: translateY(20px);"
        "    animation: messageIn 0.3s ease-out forwards;"
        "}"
        "@keyframes messageIn {"
        "    to {"
        "        opacity: 1;"
        "        transform: translateY(0);"
        "    }"
        "}"
        ".message-content {"
        "    padding: 16px;"
        "    border-radius: 8px;"
        "    line-height: 1.5;"
        "    position: relative;"
        "    overflow: hidden;"
        "}"
        ".user-message .message-content {"
        "    background: #2d2d2d;"
        "    border: 1px solid #3d3d3d;"
        "    box-shadow: 0 2px 8px rgba(0,0,0,0.1);"
        "}"
        ".assistant-message .message-content {"
        "    background: #1e1e1e;"
        "}"
        ".sender {"
        "    font-weight: 500;"
        "    margin-bottom: 8px;"
        "}"
        ".user-message .sender {"
        "    color: #4ec9b0;"
        "}"
        ".assistant-message .sender {"
        "    color: #569cd6;"
        "}"
        ".copy-button {"
        "    position: absolute;"
        "    top: 8px;"
        "    right: 8px;"
        "    padding: 4px 8px;"
        "    background: #3d3d3d;"
        "    border: none;"
        "    border-radius: 4px;"
        "    color: #569cd6;"
        "    cursor: pointer;"
        "    opacity: 0.8;"
        "    transition: all 0.2s ease;"
        "    font-size: 14px;"
        "}"
        ".message-content:hover .copy-button {"
        "    opacity: 1;"
        "}"
        ".copy-button:hover {"
        "    background: #4d4d4d;"
        "}"
        ".welcome {"
        "    text-align: center;"
        "    padding: 24px 0;"
        "}"
        ".welcome-title {"
        "    color: #569cd6;"
        "    font-size: 18px;"
        "    font-weight: 500;"
        "    margin-bottom: 8px;"
        "}"
        ".welcome-subtitle {"
        "    color: #4ec9b0;"
        "    font-size: 14px;"
        "}"
        "pre {"
        "    background: #2d2d2d;"
        "    padding: 12px;"
        "    border-radius: 4px;"
        "    overflow-x: auto;"
        "}"
        "code {"
        "    font-family: \"Cascadia Code\", \"Source Code Pro\", Menlo, Monaco, Consolas, monospace;"
        "}"
        "</style>"
        "<script>"
        "function copyMessage(element) {"
        "    const text = element.parentElement.querySelector('.text').innerText;"
        "    if (navigator.clipboard) {"
        "        navigator.clipboard.writeText(text).then(() => {"
        "            const button = element;"
        "            button.innerHTML = '✓';"
        "            button.style.background = '#4ec9b0';"
        "            button.style.color = '#ffffff';"
        "            setTimeout(() => {"
        "                button.innerHTML = '📋';"
        "                button.style.background = '#3d3d3d';"
        "                button.style.color = '#569cd6';"
        "            }, 1000);"
        "        }).catch(err => {"
        "            console.error('Failed to copy:', err);"
        "        });"
        "    }"
        "}"
        "function scrollToBottom() {"
        "    const container = document.getElementById('chat-container');"
        "    container.scrollTop = container.scrollHeight;"
        "}"
        "function appendMessage(html) {"
        "    const messages = document.getElementById('messages');"
        "    messages.insertAdjacentHTML('beforeend', html);"
        "    scrollToBottom();"
        "}"
        "</script>"
        "</head>"
        "<body>"
        "<div id=\"chat-container\">"
        "    <div id=\"messages\"></div>"
        "</div>"
        "</body>"
        "</html>"
    );
}

void DockSigint::initializeWebView()
{
    webView->setContextMenuPolicy(Qt::NoContextMenu);
    webView->setStyleSheet("QWebEngineView { background: #1e1e1e; }");
}

void DockSigint::updateChatView()
{
    webView->setHtml(chatHtml);
}

void DockSigint::appendMessage(const QString &message, bool isUser)
{
    Message msg;
    msg.id = -1;
    msg.role = isUser ? "user" : "assistant";
    msg.content = message;
    
    // Add to history
    messageHistory.append(msg);
    
    // Save to database asynchronously
    emit saveMessageToDb(currentChatId, msg.role, msg.content);
    
    // Update view asynchronously
    appendMessageToView(msg.content, isUser);
}

void DockSigint::appendMessageToView(const QString &message, bool isUser)
{
    QString messageHtml = QString(
        "<div class=\"message %1\">"
        "<div class=\"message-content\">"
        "<button class=\"copy-button\" onclick=\"copyMessage(this)\">📋</button>"
        "<div class=\"sender\">%2</div>"
        "<div class=\"text\">%3</div>"
        "</div>"
        "</div>"
    ).arg(isUser ? "user-message" : "assistant-message",
          isUser ? "User" : "Assistant",
          message.toHtmlEscaped());

    // Run JavaScript asynchronously
    webView->page()->runJavaScript(QString("appendMessage(`%1`);").arg(messageHtml));
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
    QJsonArray messages;
    for (const auto &msg : messageHistory) {
        messages.append(QJsonObject{
            {"role", msg.role},
            {"content", msg.content}
        });
    }

    // Send message to worker thread
    emit sendMessageToWorker(anthropicApiKey, currentModel, messages);
}

void DockSigint::onWorkerMessageReceived(const QString &message)
{
    appendMessage(message, false);
}

void DockSigint::onWorkerErrorOccurred(const QString &error)
{
    appendMessage(error, false);
}

QString DockSigint::getDatabasePath()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gqrx";
    QDir().mkpath(configDir);  // Ensure directory exists
    return configDir + "/chat_history.db";
} 