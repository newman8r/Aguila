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
#include <QIcon>
#include "docksigint.h"
#include "ui_docksigint.h"

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
        {"max_tokens", 4096}
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
            QString errorDetails = QString("Error: %1\nResponse: %2").arg(reply->errorString(), reply->readAll().constData());
            emit errorOccurred(errorDetails);
        }
        reply->deleteLater();
    });
}

// DatabaseWorker implementation
DatabaseWorker::DatabaseWorker(const QString &dbPath, QObject *parent) : QObject(parent)
{
    qDebug() << "\n=== 🔧 Initializing Database Worker 🔧 ===";
    qDebug() << "📂 Database path:" << dbPath;

    // Create a unique connection name for this worker
    QString connectionName = QString("WorkerConnection_%1").arg(reinterpret_cast<quintptr>(QThread::currentThread()));
    qDebug() << "🔌 Creating database connection:" << connectionName;

    // Remove any existing connection with this name
    if (QSqlDatabase::contains(connectionName)) {
        qDebug() << "Removing existing connection:" << connectionName;
        QSqlDatabase::removeDatabase(connectionName);
    }

    db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qDebug() << "❌ Failed to open database:" << db.lastError().text();
        return;
    }

    // Create tables in a single transaction
    db.transaction();
    QSqlQuery query(db);
    
    // Create chats table
    if (!query.exec("CREATE TABLE IF NOT EXISTS chats ("
                   "id INTEGER PRIMARY KEY,"
                   "name TEXT NOT NULL,"
                   "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                   ")")) {
        qDebug() << "❌ Failed to create chats table:" << query.lastError().text();
        db.rollback();
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
        qDebug() << "❌ Failed to create messages table:" << query.lastError().text();
        db.rollback();
        return;
    }

    // Create default chat if it doesn't exist
    if (!query.exec("INSERT OR IGNORE INTO chats (id, name) VALUES (1, 'Chat 1')")) {
        qDebug() << "❌ Failed to create default chat:" << query.lastError().text();
        db.rollback();
        return;
    }

    db.commit();
    qDebug() << "✅ Database initialized successfully";
}

DatabaseWorker::~DatabaseWorker()
{
    QString connectionName = db.connectionName();
    if (db.isOpen()) {
        db.close();
    }
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

void DatabaseWorker::saveMessage(int chatId, const QString &role, const QString &content)
{
    qDebug() << "\n=== 💾 Saving Message to Database 💾 ===";
    qDebug() << "🆔 Chat ID:" << chatId;
    qDebug() << "👤 Role:" << role;
    qDebug() << "📝 Content:" << content.left(50) + "...";

    if (!db.isOpen() && !db.open()) {
        QString error = "Database not open: " + db.lastError().text();
        qDebug() << "❌ " << error;
        emit this->error(error);
        return;
    }

    db.transaction();
    QSqlQuery query(db);

    // Insert message
    query.prepare("INSERT INTO messages (chat_id, role, content) VALUES (?, ?, ?)");
    query.addBindValue(chatId);
    query.addBindValue(role);
    query.addBindValue(content);

    if (!query.exec()) {
        QString error = "Error saving message: " + query.lastError().text();
        qDebug() << "❌ " << error;
        db.rollback();
        emit this->error(error);
        return;
    }

    qint64 id = query.lastInsertId().toLongLong();
    
    // Verify the message was saved
    query.prepare("SELECT * FROM messages WHERE id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        qDebug() << "✅ Message saved and verified:";
        qDebug() << "   ID:" << query.value(0).toLongLong();
        qDebug() << "   Chat ID:" << query.value(1).toInt();
        qDebug() << "   Role:" << query.value(2).toString();
        qDebug() << "   Content:" << query.value(3).toString().left(50) + "...";
        db.commit();
        emit messageSaved(id);
    } else {
        QString error = "Failed to verify saved message";
        qDebug() << "❌ " << error;
        db.rollback();
        emit this->error(error);
    }
}

void DatabaseWorker::loadChatHistory(int chatId)
{
    qDebug() << "\n=== 📚 Loading Chat History 📚 ===";
    qDebug() << "🆔 Chat ID:" << chatId;

    if (!db.isOpen() && !db.open()) {
        QString error = "Database not open: " + db.lastError().text();
        qDebug() << "❌ " << error;
        emit this->error(error);
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT role, content FROM messages WHERE chat_id = ? ORDER BY timestamp ASC");
    query.addBindValue(chatId);
    
    if (!query.exec()) {
        QString error = "Error loading chat history: " + query.lastError().text();
        qDebug() << "❌ " << error;
        emit this->error(error);
        return;
    }

    QVector<QPair<QString, QString>> messages;
    while (query.next()) {
        QString role = query.value(0).toString();
        QString content = query.value(1).toString();
        messages.append(qMakePair(role, content));
        qDebug() << "📨 Loaded message:";
        qDebug() << "   Role:" << role;
        qDebug() << "   Content:" << content.left(50) + "...";
    }

    qDebug() << "✅ Loaded" << messages.size() << "messages from history";
    emit historyLoaded(messages);
}

DockSigint::DockSigint(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::DockSigint),
    currentChatId(1)
{
    qDebug() << "\n=== 🚀 SIGINT Panel Starting Up 🚀 ===";
    
    ui->setupUi(this);

    // Set icon explicitly
    setWindowIcon(QIcon(":/icons/icons/eagle.svg"));

    // Initialize web view
    qDebug() << "🌐 Initializing web view...";
    webView = new QWebEngineView(ui->chatDisplay);
    webView->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    webView->settings()->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
    webView->page()->setWebChannel(new QWebChannel(this));
    auto *layout = new QVBoxLayout(ui->chatDisplay);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(webView);
    ui->chatDisplay->setLayout(layout);
    qDebug() << "✅ Web view initialized";

    // Initialize network worker
    qDebug() << "🌍 Starting network worker...";
    networkWorker = new NetworkWorker();
    networkWorker->moveToThread(&networkThread);
    connect(&networkThread, &QThread::finished, networkWorker, &QObject::deleteLater);
    connect(this, &DockSigint::sendMessageToWorker, networkWorker, &NetworkWorker::sendMessage);
    connect(networkWorker, &NetworkWorker::messageReceived, this, &DockSigint::onWorkerMessageReceived);
    connect(networkWorker, &NetworkWorker::errorOccurred, this, &DockSigint::onWorkerErrorOccurred);
    networkThread.start();
    qDebug() << "✅ Network worker started";

    // Initialize database worker
    qDebug() << "💾 Starting database worker...";
    QString dbPath = getDatabasePath();
    qDebug() << "📂 Database path:" << dbPath;

    // Test direct database access
    {
        // Create a new scope for database objects
        {
            QSqlDatabase testDb = QSqlDatabase::addDatabase("QSQLITE", "TestConnection");
            testDb.setDatabaseName(dbPath);
            if (testDb.open()) {
                qDebug() << "🎯 Test database connection successful";
                // Create a new scope for query
                {
                    QSqlQuery query(testDb);
                    
                    // Create tables if they don't exist
                    if (!query.exec("CREATE TABLE IF NOT EXISTS chats ("
                                  "id INTEGER PRIMARY KEY,"
                                  "name TEXT NOT NULL,"
                                  "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                                  ")")) {
                        qDebug() << "❌ Failed to create chats table:" << query.lastError().text();
                    }
                    
                    if (!query.exec("CREATE TABLE IF NOT EXISTS messages ("
                                  "id INTEGER PRIMARY KEY,"
                                  "chat_id INTEGER,"
                                  "role TEXT NOT NULL,"
                                  "content TEXT NOT NULL,"
                                  "timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                                  "FOREIGN KEY (chat_id) REFERENCES chats(id)"
                                  ")")) {
                        qDebug() << "❌ Failed to create messages table:" << query.lastError().text();
                    }

                    // Insert test chat
                    if (!query.exec("INSERT OR IGNORE INTO chats (id, name) VALUES (1, 'Test Chat')")) {
                        qDebug() << "❌ Failed to insert test chat:" << query.lastError().text();
                    }
                } // query goes out of scope here
                testDb.close();
            } else {
                qDebug() << "❌ Test database connection failed:" << testDb.lastError().text();
            }
        } // testDb goes out of scope here
        QSqlDatabase::removeDatabase("TestConnection");
    }

    databaseWorker = new DatabaseWorker(dbPath);
    databaseWorker->moveToThread(&databaseThread);
    connect(&databaseThread, &QThread::finished, databaseWorker, &QObject::deleteLater);
    connect(this, &DockSigint::saveMessageToDb, databaseWorker, &DatabaseWorker::saveMessage);
    connect(this, &DockSigint::loadHistoryFromDb, databaseWorker, &DatabaseWorker::loadChatHistory);
    connect(databaseWorker, &DatabaseWorker::messageSaved, this, [this](qint64 id) {
        qDebug() << "✅ Message saved with ID:" << id;
        if (!messageHistory.isEmpty())
            messageHistory.last().id = id;
    });
    connect(databaseWorker, &DatabaseWorker::error, this, [](const QString &error) {
        qDebug() << "❌ Database worker error:" << error;
    });
    connect(databaseWorker, &DatabaseWorker::historyLoaded, this, [this](const QVector<QPair<QString, QString>> &messages) {
        qDebug() << "📚 Loading" << messages.size() << "messages into view";
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
    qDebug() << "✅ Database worker started";

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
                appendMessage("🦅 Welcome to the Aguila SIGINT platform. Claude has the helm.", false);
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
    qDebug() << "\n=== Appending Message ===";
    qDebug() << "Is User:" << isUser;
    qDebug() << "Content:" << message.left(50) + "...";

    Message msg;
    msg.id = -1;
    msg.role = isUser ? "user" : "assistant";
    msg.content = message;
    
    // Add to history
    messageHistory.append(msg);
    
    // Save to database asynchronously
    qDebug() << "Sending save message request to worker thread";
    emit saveMessageToDb(currentChatId, msg.role, msg.content);
    
    // Update view asynchronously
    qDebug() << "Updating view";
    appendMessageToView(msg.content, isUser);
    qDebug() << "=================================\n";
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

    // Prepare the messages array with proper format
    QJsonArray messages;
    for (const auto &msg : messageHistory) {
        messages.append(QJsonObject{
            {"role", msg.role == "user" ? "user" : "assistant"},
            {"content", msg.content}
        });
    }

    // Add the current message
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", message}
    });

    qDebug() << "Message history size:" << messages.size();
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