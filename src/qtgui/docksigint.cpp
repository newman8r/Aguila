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
#include <QShortcut>
#include <QAction>
#include <QTabWidget>
#include <QLabel>
#include <QSplitter>
#include <QPushButton>
#include <QHBoxLayout>
#include <QTemporaryFile>
#include <QTextStream>
#include "../applications/gqrx/mainwindow.h"
#include "docksigint.h"
#include "ui_docksigint.h"
#include "waterfall_display.h"
#include "plotter.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

// NetworkWorker implementation
NetworkWorker::NetworkWorker(QObject *parent) : QObject(parent)
{
    networkManager = new QNetworkAccessManager(this);
    pythonProcess = new QProcess(this);
    pythonProcess->setProgram("python3");
}

NetworkWorker::~NetworkWorker()
{
    delete networkManager;
    if (pythonProcess->state() == QProcess::Running) {
        pythonProcess->terminate();
        pythonProcess->waitForFinished();
    }
}

bool NetworkWorker::analyzeTuningRequest(const QString &message)
{
    qDebug() << "\n=== 🔍 Analyzing Tuning Request ===";
    qDebug() << "Message:" << message;

    // Create a temporary Python script
    QTemporaryFile scriptFile;
    if (scriptFile.open()) {
        qDebug() << "✅ Created temporary script at:" << scriptFile.fileName();
        
        QTextStream stream(&scriptFile);
        QString escapedMessage = message;
        escapedMessage.replace("\"", "\\\"");
        QString script = QString(
            "import sys\n"
            "import os\n"
            "import json\n"
            "import logging\n\n"
            "# Configure logging\n"
            "logging.basicConfig(level=logging.DEBUG)\n"
            "logger = logging.getLogger('TuningAnalyzer')\n\n"
            "# Add current directory to path\n"
            "current_dir = os.path.dirname(os.path.abspath(__file__))\n"
            "logger.debug(f'Current directory: {current_dir}')\n"
            "sys.path.append('.')\n"
            "logger.debug(f'Python path: {sys.path}')\n\n"
            "try:\n"
            "    logger.debug('Importing chat_coordinator...')\n"
            "    from resources.chat_coordinator import ChatCoordinator\n"
            "    logger.debug('Successfully imported ChatCoordinator')\n\n"
            "    logger.debug('Creating coordinator instance...')\n"
            "    coordinator = ChatCoordinator()\n"
            "    logger.debug('Successfully created coordinator')\n\n"
            "    message = %1\n"
            "    logger.debug(f'Analyzing message: {message}')\n"
            "    result = coordinator.evaluate_request(message)\n"
            "    logger.debug(f'Analysis result: {result}')\n\n"
            "    # Print result as JSON for parsing\n"
            "    print(json.dumps(result))\n"
            "except Exception as e:\n"
            "    logger.error(f'Error during analysis: {str(e)}')\n"
            "    import traceback\n"
            "    traceback.print_exc()\n"
            "    sys.exit(1)\n"
        ).arg(QString("\"%1\"").arg(escapedMessage));

        stream << script;
        scriptFile.close();
        
        qDebug() << "📜 Generated Python script:";
        qDebug().noquote() << script;

        // Set working directory to Aguila root
        QString aguilaRoot = QCoreApplication::applicationDirPath() + "/../../";
        QDir aguilaDir(aguilaRoot);
        QString absoluteAguilaPath = aguilaDir.absolutePath();
        pythonProcess->setWorkingDirectory(absoluteAguilaPath);
        
        // Set up environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PYTHONPATH", absoluteAguilaPath);
        env.insert("PYTHONUNBUFFERED", "1");  // Ensure Python output is not buffered
        
        // Copy over any existing environment variables from .env
        QFile envFile(absoluteAguilaPath + "/.env");
        if (envFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!envFile.atEnd()) {
                QString line = envFile.readLine().trimmed();
                if (!line.isEmpty() && !line.startsWith('#')) {
                    QStringList parts = line.split('=');
                    if (parts.size() == 2) {
                        QString key = parts[0].trimmed();
                        QString value = parts[1].trimmed();
                        // Remove quotes if present
                        if (value.startsWith('"') && value.endsWith('"')) {
                            value = value.mid(1, value.length() - 2);
                        }
                        env.insert(key, value);
                        qDebug() << "Setting env var:" << key << "=" << (key.contains("KEY") ? "***" : value);
                    }
                }
            }
            envFile.close();
        }
        
        pythonProcess->setProcessEnvironment(env);
        
        // Run the script
        pythonProcess->setArguments(QStringList() << scriptFile.fileName());
        qDebug() << "🚀 Running Python script with args:" << pythonProcess->arguments();
        qDebug() << "📂 Working directory:" << pythonProcess->workingDirectory();
        qDebug() << "📂 PYTHONPATH:" << env.value("PYTHONPATH");
        
        pythonProcess->start();
        
        if (pythonProcess->waitForStarted()) {
            qDebug() << "✅ Python process started";
            
            if (pythonProcess->waitForFinished()) {
                QString stdout = QString::fromUtf8(pythonProcess->readAllStandardOutput());
                QString stderr = QString::fromUtf8(pythonProcess->readAllStandardError());
                
                qDebug() << "\n=== Python Process Output ===";
                qDebug() << "Exit code:" << pythonProcess->exitCode();
                qDebug() << "Standard output:" << stdout;
                if (!stderr.isEmpty()) {
                    qDebug() << "Standard error:" << stderr;
                }
                
                // Try to parse the output as JSON
                QJsonDocument doc = QJsonDocument::fromJson(stdout.toUtf8());
                if (!doc.isNull()) {
                    QJsonObject result = doc.object();
                    bool requiresTuning = result["requires_tuning"].toString() == "true";
                    qDebug() << "Analysis result:";
                    qDebug() << "- Requires tuning:" << requiresTuning;
                    qDebug() << "- Confidence:" << result["confidence"].toString();
                    qDebug() << "- Frequency:" << result["frequency_mentioned"].toString();
                    
                    if (requiresTuning) {
                        qDebug() << "🎯 Tuning request detected!";
                        return true;
                    }
                } else {
                    qDebug() << "❌ Failed to parse Python output as JSON";
                }
            } else {
                qDebug() << "❌ Python process failed to finish";
                qDebug() << "Error:" << pythonProcess->errorString();
            }
        } else {
            qDebug() << "❌ Failed to start Python process";
            qDebug() << "Error:" << pythonProcess->errorString();
        }
    } else {
        qDebug() << "❌ Failed to create temporary script file";
    }
    
    qDebug() << "=== Analysis Complete ===\n";
    return false;
}

void NetworkWorker::sendMessage(const QString &apiKey, const QString &model, const QJsonArray &messages)
{
    qDebug() << "\n=== 📨 Processing Message ===";
    
    // Get the latest message from the array
    QString latestMessage = messages.last().toObject()["content"].toString();
    qDebug() << "Message content:" << latestMessage;
    
    // First, analyze with coordinator
    qDebug() << "🔍 Analyzing for tuning request...";
    if (analyzeTuningRequest(latestMessage)) {
        qDebug() << "✅ Tuning request confirmed - bypassing Claude";
        emit messageReceived("✅ Tuning request processed - adjusting radio frequency...");
        return;
    }
    qDebug() << "ℹ️ Not a tuning request - proceeding with Claude";

    // If not a tuning request, proceed with normal Claude request
    QJsonObject requestBody{
        {"model", model},
        {"messages", messages},
        {"max_tokens", 4096}
    };

    QNetworkRequest request(QUrl("https://api.anthropic.com/v1/messages"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-api-key", apiKey.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");

    qDebug() << "🌐 Sending request to Claude:";
    qDebug() << "  - Model:" << model;
    qDebug() << "  - Messages:" << messages.size();

    QNetworkReply *reply = networkManager->post(request, QJsonDocument(requestBody).toJson());
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
            QJsonObject jsonObject = jsonResponse.object();

            qDebug() << "✅ Received response from Claude";
            if (jsonObject.contains("content")) {
                QString assistantMessage = jsonObject["content"].toArray()[0].toObject()["text"].toString();
                emit messageReceived(assistantMessage);
            }
            else {
                QString error = "Error: Response does not contain 'content' field";
                qDebug() << "❌ " << error;
                emit errorOccurred(error);
            }
        }
        else {
            QString errorDetails = QString("Error: %1\nResponse: %2")
                                 .arg(reply->errorString(), reply->readAll().constData());
            qDebug() << "❌ Network error:" << errorDetails;
            emit errorOccurred(errorDetails);
        }
        reply->deleteLater();
    });
    
    qDebug() << "=== Message Processing Complete ===\n";
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

    // Set pragmas for better concurrency handling
    if (db.open()) {
        QSqlQuery query(db);
        query.exec("PRAGMA journal_mode = WAL");  // Write-Ahead Logging for better concurrency
        query.exec("PRAGMA busy_timeout = 5000");  // 5 second timeout for busy/locked DB
        query.exec("PRAGMA synchronous = NORMAL");  // Balanced durability/speed
        
        // Create tables in a single transaction
        db.transaction();
        
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

        // Create settings table
        if (!query.exec("CREATE TABLE IF NOT EXISTS settings ("
                       "key TEXT PRIMARY KEY,"
                       "value TEXT NOT NULL,"
                       "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                       ")")) {
            qDebug() << "❌ Failed to create settings table:" << query.lastError().text();
            db.rollback();
            return;
        }

        // Create default chat if it doesn't exist
        if (!query.exec("INSERT OR IGNORE INTO chats (id, name) VALUES (1, 'Chat 1')")) {
            qDebug() << "❌ Failed to create default chat:" << query.lastError().text();
            db.rollback();
            return;
        }

        if (!db.commit()) {
            qDebug() << "❌ Failed to commit initial transaction:" << db.lastError().text();
            db.rollback();
        }
        
        qDebug() << "✅ Database initialized successfully with WAL mode and busy timeout";
    } else {
        qDebug() << "❌ Failed to open database:" << db.lastError().text();
    }
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

    // Ensure database connection is valid
    if (!db.isOpen()) {
        if (!db.open()) {
            QString error = "Database not open: " + db.lastError().text();
            qDebug() << "❌ " << error;
            emit this->error(error);
            return;
        }
    }

    // Use a unique transaction for this save operation
    if (!db.transaction()) {
        QString error = "Failed to start transaction: " + db.lastError().text();
        qDebug() << "❌ " << error;
        emit this->error(error);
        return;
    }

    QSqlQuery query(db);
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
        
        if (!db.commit()) {
            QString error = "Failed to commit transaction: " + db.lastError().text();
            qDebug() << "❌ " << error;
            db.rollback();
            emit this->error(error);
            return;
        }
        
        emit messageSaved(id);
    } else {
        QString error = "Failed to verify saved message: " + query.lastError().text();
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

void DatabaseWorker::loadAllChats()
{
    qDebug() << "\n=== 📚 Loading All Chats 📚 ===";

    if (!db.isOpen() && !db.open()) {
        QString error = "Database not open: " + db.lastError().text();
        qDebug() << "❌ " << error;
        emit this->error(error);
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, name FROM chats ORDER BY id ASC");
    
    if (!query.exec()) {
        QString error = "Error loading chats: " + query.lastError().text();
        qDebug() << "❌ " << error;
        emit this->error(error);
        return;
    }

    QVector<QPair<int, QString>> chats;
    while (query.next()) {
        int id = query.value(0).toInt();
        QString name = query.value(1).toString();
        chats.append(qMakePair(id, name));
        qDebug() << "📂 Loaded chat:" << id << "-" << name;
    }

    qDebug() << "✅ Loaded" << chats.size() << "chats";
    emit chatsLoaded(chats);
}

void DatabaseWorker::createChat(const QString &name)
{
    qDebug() << "\n=== 📝 Creating New Chat 📝 ===";
    qDebug() << "Name:" << name;

    if (!db.isOpen() && !db.open()) {
        QString error = "Database not open: " + db.lastError().text();
        qDebug() << "❌ " << error;
        emit this->error(error);
        return;
    }

    db.transaction();
    QSqlQuery query(db);
    query.prepare("INSERT INTO chats (name) VALUES (?)");
    query.addBindValue(name);

    if (!query.exec()) {
        QString error = "Error creating chat: " + query.lastError().text();
        qDebug() << "❌ " << error;
        db.rollback();
        emit this->error(error);
        return;
    }

    int chatId = query.lastInsertId().toInt();
    db.commit();
    
    qDebug() << "✅ Created new chat with ID:" << chatId;
    emit chatCreated(chatId, name);
}

void DatabaseWorker::saveSetting(const QString &key, const QString &value)
{
    qDebug() << "\n=== 💾 Saving Setting ===";
    qDebug() << "Key:" << key;
    qDebug() << "Value:" << value;

    if (!db.isOpen() && !db.open()) {
        QString error = "Database not open: " + db.lastError().text();
        qDebug() << "❌ " << error;
        emit this->error(error);
        return;
    }

    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO settings (key, value, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP)");
    query.addBindValue(key);
    query.addBindValue(value);

    if (!query.exec()) {
        QString error = "Error saving setting: " + query.lastError().text();
        qDebug() << "❌ " << error;
        emit this->error(error);
        return;
    }

    qDebug() << "✅ Setting saved successfully";
}

void DatabaseWorker::loadSetting(const QString &key)
{
    qDebug() << "\n=== 📚 Loading Setting ===";
    qDebug() << "Key:" << key;

    if (!db.isOpen() && !db.open()) {
        QString error = "Database not open: " + db.lastError().text();
        qDebug() << "❌ " << error;
        emit this->error(error);
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT value FROM settings WHERE key = ?");
    query.addBindValue(key);

    if (!query.exec()) {
        QString error = "Error loading setting: " + query.lastError().text();
        qDebug() << "❌ " << error;
        emit this->error(error);
        return;
    }

    if (query.next()) {
        QString value = query.value(0).toString();
        qDebug() << "✅ Setting loaded:" << value;
        emit settingLoaded(key, value);
    } else {
        qDebug() << "⚠️ Setting not found";
        emit settingLoaded(key, QString());
    }
}

DockSigint::DockSigint(receiver *rx_ptr, QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::DockSigint),
    webView(nullptr),
    networkWorker(nullptr),
    databaseWorker(nullptr),
    networkThread(),
    databaseThread(),
    anthropicApiKey(),
    currentModel(),
    currentChatId(1),
    messageHistory(),
    chatList(),
    chatHtml(),
    spectrumCapture(nullptr),
    spectrumVisualizer(nullptr),
    waterfallDisplay(nullptr),
    rx_ptr(rx_ptr),
    dsp_running(false),
    currentTab("spectrum"),
    spectrumContainer(nullptr),
    waterfallContainer(nullptr)
{
    qDebug() << "\n=== 🚀 SIGINT Panel Starting Up 🚀 ===";
    
    ui->setupUi(this);

    // Connect to DSP state changes
    if (auto *mainWindow = qobject_cast<MainWindow*>(parent)) {
        connect(mainWindow, &MainWindow::dspStateChanged, 
                this, &DockSigint::onDspStateChanged);
    }

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

    // Load environment variables before creating NetworkWorker
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString envPath = QDir(configDir).filePath(".env");
    if (QFile::exists(envPath)) {
        QFile envFile(envPath);
        if (envFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!envFile.atEnd()) {
                QString line = envFile.readLine().trimmed();
                if (!line.isEmpty() && !line.startsWith('#')) {
                    QStringList parts = line.split('=');
                    if (parts.size() == 2) {
                        QString key = parts[0].trimmed();
                        QString value = parts[1].trimmed();
                        // Remove quotes if present
                        if (value.startsWith('"') && value.endsWith('"')) {
                            value = value.mid(1, value.length() - 2);
                        }
                        qputenv(key.toUtf8(), value.toUtf8());
                    }
                }
            }
            envFile.close();
            qDebug() << "✅ Environment variables loaded from" << envPath;
        }
    }

    // Create worker thread and move NetworkWorker to it
    QThread *networkThread = new QThread(this);
    NetworkWorker *networkWorker = new NetworkWorker();
    networkWorker->moveToThread(networkThread);

    // Set up network worker connections
    connect(networkThread, &QThread::finished, networkWorker, &QObject::deleteLater);
    connect(this, &DockSigint::sendMessageToWorker, networkWorker, &NetworkWorker::sendMessage);
    connect(networkWorker, &NetworkWorker::messageReceived, this, &DockSigint::onWorkerMessageReceived);
    connect(networkWorker, &NetworkWorker::errorOccurred, this, &DockSigint::onWorkerErrorOccurred);
    networkThread->start();
    qDebug() << "✅ Network worker started in separate thread";

    // Initialize database worker
    qDebug() << "💾 Starting database worker...";
    QString dbPath = getDatabasePath();
    qDebug() << "📂 Database path:" << dbPath;

    // Initialize spectrum capture
    qDebug() << "📡 Initializing spectrum capture...";
    spectrumCapture = std::make_unique<SpectrumCapture>(rx_ptr);
    connect(spectrumCapture.get(), &SpectrumCapture::captureStarted, 
            this, &DockSigint::onCaptureStarted);
    connect(spectrumCapture.get(), &SpectrumCapture::captureComplete,
            this, &DockSigint::onCaptureComplete);
    connect(spectrumCapture.get(), &SpectrumCapture::captureError,
            this, &DockSigint::onCaptureError);
    connect(spectrumCapture.get(), &SpectrumCapture::progressUpdate,
            this, &DockSigint::onCaptureProgress);
    qDebug() << "✅ Spectrum capture initialized";

    // Initialize spectrum visualizer
    spectrumVisualizer = ui->spectrumVisualizer;
    
    // Connect capture signals to visualizer
    connect(spectrumCapture.get(), &SpectrumCapture::captureComplete,
            this, [this](const SpectrumCapture::CaptureResult& result) {
        if (result.success) {
            double bandwidth = result.range.end_freq - result.range.start_freq;
            double center_freq = result.range.start_freq + bandwidth/2;
            
            // Update both visualizers with the same data
            spectrumVisualizer->updateData(result.fft_data, center_freq, bandwidth, result.range.sample_rate);
            if (waterfallDisplay) {
                qDebug() << "🌊 Sending data to waterfall display:";
                qDebug() << "  - FFT data size:" << result.fft_data.size();
                qDebug() << "  - Center freq:" << center_freq << "Hz";
                qDebug() << "  - Bandwidth:" << bandwidth << "Hz";
                qDebug() << "  - Sample rate:" << result.range.sample_rate << "Hz";
                waterfallDisplay->updateData(result.fft_data, center_freq, bandwidth, result.range.sample_rate);
            } else {
                qDebug() << "❌ Waterfall display not initialized";
            }
        }
    });

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
        
        // Add placeholder message if chat is empty
        if (messages.isEmpty()) {
            QString placeholder = "👋 Welcome to your new chat session!\n\n"
                                "I'm here to help you analyze signals and work with your SDR. "
                                "You can:\n"
                                "• Capture and analyze waterfall screenshots (Ctrl+P)\n"
                                "• Ask questions about signal types and characteristics\n"
                                "• Get help with SDR settings and configurations\n\n"
                                "What would you like to do?";
            appendMessageToView(placeholder, false);
        }
    });
    connect(databaseWorker, &DatabaseWorker::chatsLoaded, this, &DockSigint::onChatsLoaded);
    connect(databaseWorker, &DatabaseWorker::chatCreated, this, [this](int chatId, const QString &name) {
        Chat newChat;
        newChat.id = chatId;
        newChat.name = name;
        chatList.append(newChat);
        
        // First switch to the new chat
        currentChatId = chatId;
        messageHistory.clear();
        clearChat();
        emit loadHistoryFromDb(chatId);
        
        // Then update the selector and force the correct selection
        ui->chatSelector->blockSignals(true);
        updateChatSelector();
        int index = ui->chatSelector->findData(chatId);
        if (index != -1) {
            ui->chatSelector->setCurrentIndex(index);
        }
        ui->chatSelector->blockSignals(false);
    });
    connect(databaseWorker, &DatabaseWorker::settingLoaded, this, [this](const QString &key, const QString &value) {
        if (key == "last_active_chat" && !value.isEmpty()) {
            qDebug() << "\n=== 📢 Last Active Chat Setting Loaded ===";
            int chatId = value.toInt();
            qDebug() << "  - Loaded chat ID:" << chatId;
            
            if (chatId > 0) {
                m_lastActiveChatLoaded = true;
                currentChatId = chatId;
                qDebug() << "  - Set currentChatId to:" << currentChatId;
                
                // If chats are already loaded, switch to this chat
                if (m_chatsLoaded) {
                    qDebug() << "  - Chats already loaded, switching to chat:" << chatId;
                    switchToLastActiveChat();
                } else {
                    qDebug() << "  - Waiting for chats to load before switching";
                }
            }
            qDebug() << "=================================\n";
        }
    });
    connect(databaseWorker, &DatabaseWorker::settingLoaded, this, [this](const QString &key, const QString &value) {
        // Handle other settings as needed
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
                appendMessage("👋 Welcome to the Aguila SIGINT platform. Claude has the helm.", false);
            }
        }
    });

    // Set the HTML content
    updateChatView();
    
    // Connect chat management signals
    connect(ui->newChatButton, &QPushButton::clicked, this, &DockSigint::onNewChatClicked);
    connect(ui->chatSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DockSigint::onChatSelected);

    // Add spectrum capture test shortcut
    auto *captureShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_G), this);
    connect(captureShortcut, &QShortcut::activated, this, &DockSigint::testSpectrumCapture);

    // Add screenshot shortcut (Ctrl+P)
    auto *screenshotShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_P), this);
    connect(screenshotShortcut, &QShortcut::activated, this, &DockSigint::captureWaterfallScreenshot);

    // Load existing chats
    databaseWorker->loadAllChats();
    
    // Load last active chat setting
    databaseWorker->loadSetting("last_active_chat");

    // Initialize tab system
    currentTab = "spectrum";
    spectrumContainer = new QWidget(this);
    waterfallContainer = new QWidget(this);
    setupTabSystem();
    
    // Create the web channel
    QWebChannel *channel = new QWebChannel(this);
    webView->page()->setWebChannel(channel);
    
    // Register this object to handle JavaScript calls
    channel->registerObject(QStringLiteral("qt"), this);
    
    // Load the HTML
    webView->setHtml(getBaseHtml());
    
    qDebug() << "\n=== SIGINT Panel Initialization ===";
    qDebug() << "App directory:" << QCoreApplication::applicationDirPath();
    qDebug() << "Current working directory:" << QDir::currentPath();
    qDebug() << "Config directory:" << QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gqrx";
    qDebug() << "Database path:" << getDatabasePath();
    qDebug() << "Model:" << currentModel;
    qDebug() << "API Key status:" << (!anthropicApiKey.isEmpty() ? "Found" : "Missing");
    qDebug() << "=================================\n";

    // Add at the start of the class implementation, after member initialization
    m_lastActiveChatLoaded = false;
    m_chatsLoaded = false;
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
        QDir::currentPath() + ".env",
        QCoreApplication::applicationDirPath() + ".env",
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
    qDebug() << "\n=== 🚀 Send Button Clicked ===";
    QString message = ui->chatInput->text().trimmed();
    if (!message.isEmpty()) {
        qDebug() << "Message:" << message;
        appendMessage(message);
        sendToClaude(message);
        ui->chatInput->clear();
    }
    qDebug() << "=== Send Complete ===\n";
}

void DockSigint::onReturnPressed()
{
    onSendClicked();
}

void DockSigint::sendToClaude(const QString &message, std::function<void(const QString&)> callback)
{
    qDebug() << "\n=== 📤 Sending Message to Claude ===";
    qDebug() << "Message:" << message;
    
    // Call the three-parameter version with empty image data
    sendToClaude(message, QByteArray(), callback);
    
    qDebug() << "=== Send Initiated ===\n";
}

void DockSigint::sendToClaude(const QString &message, const QByteArray &imageData, std::function<void(const QString&)> callback)
{
    qDebug() << "\n=== 📤 Sending Message to Claude (with potential image) ===";
    qDebug() << "Message:" << message;
    qDebug() << "Has image data:" << !imageData.isEmpty();
    
    if (anthropicApiKey.isEmpty()) {
        qDebug() << "❌ Error: API key is empty";
        appendMessage("Error: API key not found. Please check your .env file.", false);
        return;
    }

    // Prepare the messages array with history
    QJsonArray messages;
    for (const auto &msg : messageHistory) {
        messages.append(QJsonObject{
            {"role", msg.role == "user" ? "user" : "assistant"},
            {"content", msg.content}
        });
    }

    // Add the current message
    if (imageData.isEmpty()) {
        // Text-only message
        messages.append(QJsonObject{
            {"role", "user"},
            {"content", message}
        });
    } else {
        // Message with image
        QString base64Image = QString::fromLatin1(imageData.toBase64());
        
        QJsonObject source;
        source.insert("type", "base64");
        source.insert("media_type", "image/png");
        source.insert("data", base64Image);

        QJsonObject imageContent;
        imageContent.insert("type", "image");
        imageContent.insert("source", source);

        QJsonObject textContent;
        textContent.insert("type", "text");
        textContent.insert("text", message);

        QJsonArray contentArray;
        contentArray.append(imageContent);
        contentArray.append(textContent);

        messages.append(QJsonObject{
            {"role", "user"},
            {"content", contentArray}
        });
    }

    qDebug() << "Message history size:" << messages.size();
    
    // Connect a one-time handler for the response if callback provided
    if (callback) {
        QMetaObject::Connection *connection = new QMetaObject::Connection;
        *connection = connect(networkWorker, &NetworkWorker::messageReceived,
                            this, [this, callback, connection](const QString &response) {
            // Disconnect after receiving the response
            QObject::disconnect(*connection);
            delete connection;
            
            // Call the callback with the response
            callback(response);
        });
    }
    
    qDebug() << "🚀 Emitting sendMessageToWorker signal";
    emit sendMessageToWorker(anthropicApiKey, currentModel, messages);
    
    qDebug() << "=== Send Complete ===\n";
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

void DockSigint::loadChats()
{
    qDebug() << "Loading all chats...";
    emit loadHistoryFromDb(currentChatId);
}

void DockSigint::createNewChat()
{
    int newChatNum = chatList.isEmpty() ? 1 : chatList.last().id + 1;
    QString chatName = QString("Chat %1").arg(newChatNum);
    databaseWorker->createChat(chatName);
}

void DockSigint::switchToChat(int chatId)
{
    if (chatId == currentChatId) return;

    currentChatId = chatId;
    messageHistory.clear();
    clearChat();
    emit loadHistoryFromDb(currentChatId);
    
    // Save the last active chat
    databaseWorker->saveSetting("last_active_chat", QString::number(chatId));
}

void DockSigint::updateChatSelector()
{
    ui->chatSelector->clear();
    for (const auto &chat : chatList) {
        ui->chatSelector->addItem(chat.name, chat.id);
    }
    
    // Set current chat
    int index = ui->chatSelector->findData(currentChatId);
    if (index != -1) {
        ui->chatSelector->setCurrentIndex(index);
    }
}

void DockSigint::clearChat()
{
    webView->page()->runJavaScript("document.getElementById('messages').innerHTML = '';");
}

void DockSigint::onNewChatClicked()
{
    createNewChat();
}

void DockSigint::onChatSelected(int index)
{
    if (index >= 0) {
        int chatId = ui->chatSelector->itemData(index).toInt();
        switchToChat(chatId);
    }
}

void DockSigint::onChatsLoaded(const QVector<QPair<int, QString>> &chats)
{
    qDebug() << "\n=== 📚 Chats Loaded ===";
    qDebug() << "  - Number of chats:" << chats.size();
    qDebug() << "  - Last active chat loaded:" << m_lastActiveChatLoaded;
    qDebug() << "  - Current chat ID:" << currentChatId;
    
    chatList.clear();
    for (const auto &chat : chats) {
        Chat newChat;
        newChat.id = chat.first;
        newChat.name = chat.second;
        chatList.append(newChat);
        qDebug() << "  - Loaded chat:" << newChat.id << "-" << newChat.name;
    }
    
    // Update the selector with all chats
    updateChatSelector();
    m_chatsLoaded = true;
    
    // If we have a last active chat, switch to it
    if (m_lastActiveChatLoaded) {
        qDebug() << "  - Have last active chat, switching to:" << currentChatId;
        switchToLastActiveChat();
    } else {
        qDebug() << "  - No last active chat, staying with current:" << currentChatId;
    }
    qDebug() << "=================================\n";
}

void DockSigint::onCaptureStarted(const SpectrumCapture::CaptureRange& range)
{
    qDebug() << "Starting spectrum capture:" 
             << range.start_freq << "Hz to" 
             << range.end_freq << "Hz";
    
    QString message = QString("📡 Starting spectrum capture from %1 MHz to %2 MHz...")
                     .arg(range.start_freq / 1e6, 0, 'f', 3)
                     .arg(range.end_freq / 1e6, 0, 'f', 3);
    appendMessage(message, false);
}

void DockSigint::onCaptureComplete(const SpectrumCapture::CaptureResult& result)
{
    if (result.success) {
        qDebug() << "Capture complete:" << result.fft_data.size() << "samples";
        
        // Phase 1A - Basic FFT Data Capture
        QString message = QString("✅ Phase 1A Capture Complete\n\n");
        message += QString("📊 Captured %1 FFT samples\n").arg(result.fft_data.size());
        message += QString("📡 Center Frequency: %1 MHz\n")
                  .arg((result.range.start_freq + result.range.end_freq) / 2e6, 0, 'f', 3);
        message += QString("📏 Bandwidth: %1 MHz\n")
                  .arg((result.range.end_freq - result.range.start_freq) / 1e6, 0, 'f', 3);
        message += QString("⚡ Sample Rate: %1 MHz\n")
                  .arg(result.range.sample_rate / 1e6, 0, 'f', 3);
        message += QString("🔍 Resolution: %1 kHz/bin\n")
                  .arg(result.range.sample_rate / result.range.fft_size / 1e3, 0, 'f', 2);
        message += QString("⏱️ Timestamp: %1")
                  .arg(QDateTime::fromMSecsSinceEpoch(result.timestamp * 1000)
                       .toString("yyyy-MM-dd HH:mm:ss.zzz"));
        
        appendMessage(message, false);
        
        // TODO: Phase 1B - Add FFT data visualization
        // TODO: Phase 1C - Add signal detection and analysis
    }
}

void DockSigint::onCaptureError(const std::string& error)
{
    qDebug() << "Capture error:" << QString::fromStdString(error);
    QString message = QString("❌ Capture failed: %1")
                     .arg(QString::fromStdString(error));
    appendMessage(message, false);
}

void DockSigint::onCaptureProgress(int percent)
{
    qDebug() << "Capture progress:" << percent << "%";
    // TODO: Add progress indicator to UI
}

void DockSigint::testSpectrumCapture()
{
    qDebug() << "\n=== 🔍 Starting Spectrum Capture Test ===";
    qDebug() << "Receiver pointer:" << (rx_ptr ? "Valid" : "Null");
    qDebug() << "DSP running:" << dsp_running;

    if (!rx_ptr) {
        qDebug() << "❌ Error: No receiver available";
        appendMessage("❌ Error: No receiver available", false);
        return;
    }

    if (!dsp_running) {
        qDebug() << "❌ Error: DSP not running";
        appendMessage("❌ Error: DSP is not running. Please start DSP first (click the power button).", false);
        return;
    }

    qDebug() << "🎯 Getting current parameters...";
    double center_freq = spectrumCapture->getCurrentCenterFreq();
    double sample_rate = spectrumCapture->getCurrentSampleRate();
    
    qDebug() << "Center frequency:" << center_freq << "Hz";
    qDebug() << "Sample rate:" << sample_rate << "Hz";
    
    if (center_freq == 0 || sample_rate == 0) {
        qDebug() << "❌ Error: Invalid frequency or sample rate";
        appendMessage("❌ Error: Invalid frequency or sample rate", false);
        return;
    }
    
    qDebug() << "📊 Creating capture range...";
    // Capture 50 kHz centered on current frequency
    SpectrumCapture::CaptureRange range {
        .start_freq = center_freq - 25000,  // 25 kHz below center
        .end_freq = center_freq + 25000,    // 25 kHz above center
        .fft_size = 4096,
        .sample_rate = 50000  // 50 kHz sample rate
    };

    qDebug() << "Range parameters:";
    qDebug() << "- Start freq:" << range.start_freq << "Hz";
    qDebug() << "- End freq:" << range.end_freq << "Hz";
    qDebug() << "- FFT size:" << range.fft_size;
    qDebug() << "- Sample rate:" << range.sample_rate << "Hz";

    qDebug() << "🚀 Attempting capture...";
    try {
        auto result = spectrumCapture->captureRange(range);
        if (result.success) {
            qDebug() << "\n=== 📊 FFT Data ===";
            qDebug() << "Timestamp:" << QDateTime::fromMSecsSinceEpoch(result.timestamp * 1000).toString("yyyy-MM-dd HH:mm:ss.zzz");
            qDebug() << "Center Frequency:" << (range.start_freq + range.end_freq) / 2 << "Hz";
            qDebug() << "Bandwidth:" << (range.end_freq - range.start_freq) << "Hz";
            qDebug() << "Sample Rate:" << range.sample_rate << "Hz";
            qDebug() << "FFT Size:" << range.fft_size;
            qDebug() << "Resolution:" << (range.sample_rate / range.fft_size) << "Hz/bin";
            qDebug() << "\nFFT Data (dB):";
            
            // Print FFT data in a format easy to copy/paste
            QString fftDataStr = "[\n";
            for (size_t i = 0; i < result.fft_data.size(); ++i) {
                fftDataStr += QString::number(result.fft_data[i], 'f', 2);
                if (i < result.fft_data.size() - 1) {
                    fftDataStr += ", ";
                }
                if ((i + 1) % 8 == 0) {  // 8 values per line
                    fftDataStr += "\n";
                }
            }
            fftDataStr += "\n]";
            qDebug().noquote() << fftDataStr;
            qDebug() << "===================\n";
            
            appendMessage("✅ FFT data captured successfully. Check the debug log for the data.", false);
        } else {
            qDebug() << "❌ Capture failed:" << QString::fromStdString(result.error_message);
            appendMessage(QString("❌ Capture failed: %1").arg(QString::fromStdString(result.error_message)), false);
        }
    }
    catch (const std::exception& e) {
        qDebug() << "❌ Exception during capture:" << e.what();
        appendMessage(QString("❌ Error during capture: %1").arg(e.what()), false);
    }
    catch (...) {
        qDebug() << "❌ Unknown exception during capture";
        appendMessage("❌ Unknown error during capture", false);
    }
    
    qDebug() << "=== Test Complete ===\n";
}

void DockSigint::onDspStateChanged(bool running)
{
    dsp_running = running;
    if (running) {
        appendMessage("✅ DSP started", false);
    } else {
        appendMessage("❌ DSP stopped", false);
    }
}

void DockSigint::setupTabSystem()
{
    // Create main splitter
    QSplitter *mainSplitter = new QSplitter(Qt::Vertical, this);
    mainSplitter->setChildrenCollapsible(false);  // Prevent areas from being collapsed completely
    
    // Create toolbar widget
    QWidget *toolbar = new QWidget();
    toolbar->setMinimumHeight(40);
    toolbar->setMaximumHeight(40);
    toolbar->setStyleSheet(R"(
        QWidget {
            background-color: #1e1e1e;
            border: 1px solid #2d2d2d;
            border-radius: 6px;
            margin: 4px 0px;
        }
        QPushButton {
            background-color: rgba(45, 45, 45, 0.7);
            color: #d4d4d4;
            border: 1px solid rgba(61, 61, 61, 0.8);
            border-radius: 4px;
            padding: 4px 12px;
            font-size: 13px;
            font-weight: 500;
            margin: 4px;
        }
        QPushButton:hover {
            background-color: rgba(61, 61, 61, 0.8);
            border: 1px solid rgba(86, 156, 214, 0.5);
            color: #569cd6;
        }
        QPushButton:pressed {
            background-color: rgba(14, 99, 156, 0.8);
            border: 1px solid rgba(86, 156, 214, 0.8);
            color: white;
        }
    )");

    // Create toolbar layout
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 0, 8, 0);
    toolbarLayout->setSpacing(8);

    // Add screenshot button
    QPushButton *screenshotBtn = new QPushButton("📸 Screenshot");
    screenshotBtn->setObjectName("screenshotButton");
    toolbarLayout->addWidget(screenshotBtn);
    
    // Connect screenshot button to capture function
    connect(screenshotBtn, &QPushButton::clicked, this, &DockSigint::captureWaterfallScreenshot);

    // Add spacer to push everything to the left
    toolbarLayout->addStretch();
    
    // Create tab widget
    QTabWidget *tabWidget = new QTabWidget();
    tabWidget->setTabPosition(QTabWidget::North);
    tabWidget->setDocumentMode(true);
    
    // Style the tab widget for better visibility
    tabWidget->setStyleSheet(R"(
        QTabWidget::pane { 
            border: none;
        }
        QTabBar::tab {
            background: #2d2d2d;
            color: #d4d4d4;
            padding: 8px 16px;
            border: none;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }
        QTabBar::tab:selected {
            background: #3d3d3d;
            color: #ffffff;
        }
        QTabBar::tab:hover {
            background: #353535;
        }
    )");
    
    // Create spectrum tab
    QWidget *spectrumTab = new QWidget();
    QVBoxLayout *spectrumLayout = new QVBoxLayout(spectrumTab);
    spectrumLayout->setContentsMargins(0, 0, 0, 0);
    spectrumLayout->addWidget(spectrumVisualizer);
    tabWidget->addTab(spectrumTab, "Spectrum Analysis");
    
    // Create waterfall tab with actual waterfall display
    QWidget *waterfallTab = new QWidget();
    QVBoxLayout *waterfallLayout = new QVBoxLayout(waterfallTab);
    waterfallLayout->setContentsMargins(0, 0, 0, 0);
    
    // Initialize waterfall display if not already done
    if (!waterfallDisplay) {
        waterfallDisplay = std::make_unique<WaterfallDisplay>(this);
        waterfallDisplay->setMinMax(-120.0f, -20.0f);  // Same range as spectrum
        waterfallDisplay->setTimeSpan(10.0f);  // 10 seconds of history
    }
    waterfallLayout->addWidget(waterfallDisplay.get());
    tabWidget->addTab(waterfallTab, "Waterfall");
    
    // Add widgets to splitter
    mainSplitter->addWidget(toolbar);  // Add toolbar first
    mainSplitter->addWidget(tabWidget);
    mainSplitter->addWidget(webView);
    
    // Set initial sizes - give chat area more space
    QList<int> sizes;
    sizes << 40 << 200 << 400;  // Toolbar: 40px, Visualization: 200px, Chat: 400px
    mainSplitter->setSizes(sizes);
    
    // Add splitter to main layout
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(ui->chatDisplay->layout());
    if (mainLayout) {
        // Clear any existing widgets
        while (QLayoutItem* item = mainLayout->takeAt(0)) {
            if (QWidget* widget = item->widget()) {
                widget->setParent(nullptr);
            }
            delete item;
        }
        mainLayout->addWidget(mainSplitter);
    }
    
    // Connect tab changed signal
    connect(tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        currentTab = (index == 0) ? "spectrum" : "waterfall";
        qDebug() << "Switched to tab:" << currentTab;
        if (currentTab == "spectrum") {
            spectrumVisualizer->setVisible(true);
            if (waterfallDisplay) waterfallDisplay->setVisible(false);
        } else {
            spectrumVisualizer->setVisible(false);
            if (waterfallDisplay) waterfallDisplay->setVisible(true);
        }
    });
    
    // Set minimum sizes to prevent areas from becoming too small
    tabWidget->setMinimumHeight(150);  // Minimum height for visualization
    webView->setMinimumHeight(100);    // Minimum height for chat
}

void DockSigint::moveVisualizerToTab()
{
    // This function is now deprecated
}

// JavaScript slot to handle tab changes
void DockSigint::onTabChanged(const QString &tabName)
{
    currentTab = tabName;
    
    // Handle visibility of widgets based on active tab
    if (tabName == "spectrum") {
        spectrumVisualizer->setVisible(true);
    } else {
        spectrumVisualizer->setVisible(false);
    }
}

void DockSigint::onNewFFTData(const std::vector<float>& fft_data, double center_freq, double bandwidth, double sample_rate)
{
    // Update spectrum visualizer
    spectrumVisualizer->updateData(fft_data, center_freq, bandwidth, sample_rate);
    
    // Update waterfall display if it exists and is visible
    if (waterfallDisplay && currentTab == "waterfall") {
        qDebug() << "Updating waterfall with" << fft_data.size() << "samples";
        qDebug() << "Center freq:" << center_freq << "Hz";
        qDebug() << "Bandwidth:" << bandwidth << "Hz";
        waterfallDisplay->updateData(fft_data, center_freq, bandwidth, sample_rate);
    }
}

QString DockSigint::getScreenshotPath() const
{
    // Create screenshots directory in the config folder
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gqrx/screenshots";
    
    // Create directory if it doesn't exist
    QDir dir(configDir);
    if (!dir.exists()) {
        qDebug() << "📁 Creating screenshots directory:" << configDir;
        if (!dir.mkpath(".")) {
            qDebug() << "❌ Failed to create screenshots directory";
            return QString();
        }
    }
    
    // Generate filename with timestamp and frequency
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    
    // Find the plotter widget to get the demod frequency
    QWidget* waterfallWidget = findWaterfallWidget();
    double freq = 0.0;
    if (waterfallWidget) {
        if (auto *plotter = qobject_cast<CPlotter*>(waterfallWidget)) {
            freq = plotter->getDemodCenterFreq();
        }
    }
    
    QString filepath = QString("%1/waterfall_%2_%3MHz.png")
            .arg(configDir)
            .arg(timestamp)
            .arg(freq / 1e6, 0, 'f', 3);
            
    qDebug() << "📸 Screenshot will be saved to:" << filepath;
    return filepath;
}

QWidget* DockSigint::findWaterfallWidget() const
{
    QWidget* mainWindow = qobject_cast<QWidget*>(parent());
    if (!mainWindow) return nullptr;
    
    // Get the main window
    while (mainWindow && !qobject_cast<MainWindow*>(mainWindow)) {
        mainWindow = mainWindow->parentWidget();
    }
    
    if (!mainWindow) return nullptr;
    
    // Find the waterfall widget by searching through children
    QList<QWidget*> widgets = mainWindow->findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        if (widget->objectName() == "plotter") {
            return widget;
        }
    }
    
    return nullptr;
}

void DockSigint::captureWaterfallScreenshot()
{
    qDebug() << "\n=== 📸 Capturing Waterfall Screenshot ===";
    
    // Find the waterfall widget
    QWidget* waterfallWidget = findWaterfallWidget();
    if (!waterfallWidget) {
        QString error = "Could not find waterfall widget";
        qDebug() << "❌ Error:" << error;
        appendMessage("❌ Error: " + error, false);
        return;
    }

    // Get the plotter
    auto *plotter = qobject_cast<CPlotter*>(waterfallWidget);
    if (!plotter) {
        QString error = "Could not access plotter functions";
        qDebug() << "❌ Error:" << error;
        appendMessage("❌ Error: " + error, false);
        return;
    }

    // Get signal parameters
    double demodFreq = plotter->getDemodCenterFreq();
    double sampleRate = plotter->getSampleRate();
    int filterLowCut, filterHighCut;
    plotter->getHiLowCutFrequencies(&filterLowCut, &filterHighCut);
    double filterBandwidth = (filterHighCut - filterLowCut);
    
    qDebug() << "📡 Signal Parameters:";
    qDebug() << "  - Demod Frequency:" << demodFreq << "Hz (" << demodFreq/1e6 << "MHz)";
    qDebug() << "  - Sample Rate:" << sampleRate << "Hz (" << sampleRate/1e6 << "MHz)";
    qDebug() << "  - Filter Bandwidth:" << filterBandwidth << "Hz (" << filterBandwidth/1e3 << "kHz)";
    qDebug() << "  - Filter Range:" << filterLowCut << "to" << filterHighCut << "Hz";
    
    // Set up capture parameters
    QRect widgetRect = waterfallWidget->rect();
    int centerX = plotter->xFromFreq(demodFreq);
    int sliceWidth = 100;  // Width of the slice to capture
    
    // Calculate actual frequency range of the slice
    double pixelsPerHz = plotter->width() / sampleRate;
    double captureWidthHz = sliceWidth / pixelsPerHz;
    
    qDebug() << "📊 Capture Parameters:";
    qDebug() << "  - Widget size:" << widgetRect.size();
    qDebug() << "  - Center X:" << centerX;
    qDebug() << "  - Slice width:" << sliceWidth << "pixels";
    qDebug() << "  - Capture bandwidth:" << captureWidthHz << "Hz (" << captureWidthHz/1e3 << "kHz)";
    
    // Create capture rectangle
    QRect captureRect(
        centerX - sliceWidth/2,
        0,
        sliceWidth,
        widgetRect.height()
    );
    
    // Capture the widget
    QPixmap screenshot = waterfallWidget->grab(captureRect);
    
    // Save to temp file
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    if (tempFile.open()) {
        QString filepath = tempFile.fileName();
        if (screenshot.save(&tempFile, "PNG")) {
            tempFile.close();
            
            // Read the image data
            QFile imageFile(filepath);
            if (imageFile.open(QIODevice::ReadOnly)) {
                QByteArray imageData = imageFile.readAll();
                imageFile.close();
                
                // Create detailed analysis prompt
                QString analysisPrompt = QString(
                    "Please analyze this waterfall signal data from GQRX:\n\n"
                    "📡 Signal Parameters:\n"
                    "- Center Frequency: %1 MHz\n"
                    "- Filter Bandwidth: %2 kHz\n"
                    "- Sample Rate: %3 MHz\n\n"
                    "Please analyze this signal and tell me:\n"
                    "1. The likely signal type(s)\n"
                    "2. Any modulation characteristics you can identify\n"
                    "3. Potential sources or applications\n"
                    "4. Signal quality assessment\n\n"
                    "If you're unsure about the precise signal type, please provide several likely possibilities. "
                    "Include any other relevant observations about the signal pattern, strength, or unique characteristics."
                ).arg(demodFreq / 1e6, 0, 'f', 6)
                 .arg(filterBandwidth / 1e3, 0, 'f', 2)
                 .arg(sampleRate / 1e6, 0, 'f', 3);

                qDebug() << "\n📝 Analysis Prompt:";
                qDebug().noquote() << analysisPrompt;
                qDebug() << "";

                // Send to Claude
                appendMessage("🔍 Analyzing signal...", false);
                sendToClaude(analysisPrompt, imageData);
            } else {
                QString error = "Could not read captured image";
                qDebug() << "❌ Error:" << error;
                appendMessage("❌ Error: " + error, false);
            }
            
            // Clean up
            QFile::remove(filepath);
        } else {
            QString error = "Could not save screenshot";
            qDebug() << "❌ Error:" << error;
            appendMessage("❌ Error: " + error, false);
        }
    } else {
        QString error = "Could not create temporary file";
        qDebug() << "❌ Error:" << error;
        appendMessage("❌ Error: " + error, false);
    }
}

void DockSigint::switchToLastActiveChat()
{
    qDebug() << "\n=== 🔄 Switching to Last Active Chat ===";
    qDebug() << "  - Target chat ID:" << currentChatId;
    
    // Find if this chat exists
    bool found = false;
    for (const auto &chat : chatList) {
        if (chat.id == currentChatId) {
            found = true;
            break;
        }
    }
    
    if (found) {
        qDebug() << "  - Chat found, performing switch";
        // Switch to the last active chat
        switchToChat(currentChatId);
        
        // Update the selector to show the correct chat
        int index = ui->chatSelector->findData(currentChatId);
        if (index != -1) {
            qDebug() << "  - Updating selector to index:" << index;
            ui->chatSelector->blockSignals(true);
            ui->chatSelector->setCurrentIndex(index);
            ui->chatSelector->blockSignals(false);
        } else {
            qDebug() << "  ⚠️ Chat index not found in selector!";
        }
    } else {
        qDebug() << "  ⚠️ Chat" << currentChatId << "not found in chat list!";
    }
    qDebug() << "=================================\n";
}

QString DockSigint::getBaseHtml()
{
    return QString(R"HTML(<!DOCTYPE html>
<html>
<head>
<style>
html, body {
    margin: 0;
    padding: 0;
    height: 100%;
    background: #1e1e1e;
    color: #d4d4d4;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
}

#chat-container {
    padding: 16px;
    height: 100%;
    overflow-y: auto;
    scroll-behavior: smooth;
    display: flex;
    flex-direction: column;
}

#messages {
    flex-grow: 1;
    min-height: min-content;
}

.message {
    margin: 16px 0;
    opacity: 0;
    transform: translateY(20px);
    animation: messageIn 0.3s ease-out forwards;
}

@keyframes messageIn {
    to {
        opacity: 1;
        transform: translateY(0);
    }
}

.message-content {
    padding: 16px;
    border-radius: 8px;
    line-height: 1.5;
    position: relative;
    overflow: hidden;
}

.user-message .message-content {
    background: #2d2d2d;
    border: 1px solid #3d3d3d;
    box-shadow: 0 2px 8px rgba(0,0,0,0.1);
}

.assistant-message .message-content {
    background: #1e1e1e;
}

.sender {
    font-weight: 500;
    margin-bottom: 8px;
}

.user-message .sender {
    color: #4ec9b0;
}

.assistant-message .sender {
    color: #569cd6;
}

.copy-button {
    position: absolute;
    top: 8px;
    right: 8px;
    padding: 4px 8px;
    background: #3d3d3d;
    border: none;
    border-radius: 4px;
    color: #569cd6;
    cursor: pointer;
    opacity: 0.8;
    transition: all 0.2s ease;
    font-size: 14px;
}

.message-content:hover .copy-button {
    opacity: 1;
}

.copy-button:hover {
    background-color: rgba(61, 61, 61, 0.8);
}
</style>

<script>
function copyMessage(element) {
    const text = element.parentElement.querySelector('.text').innerText;
    if (navigator.clipboard) {
        navigator.clipboard.writeText(text).then(() => {
            const button = element;
            button.innerHTML = '✓';
            button.style.background = '#4ec9b0';
            button.style.color = '#ffffff';
            setTimeout(() => {
                button.innerHTML = '📋';
                button.style.background = '#3d3d3d';
                button.style.color = '#569cd6';
            }, 1000);
        }).catch(err => {
            console.error('Failed to copy:', err);
        });
    }
}

function scrollToBottom() {
    const container = document.getElementById('chat-container');
    if (container) container.scrollTop = container.scrollHeight;
}

document.addEventListener('DOMContentLoaded', function() {
    scrollToBottom();
});
</script>
</head>
<body>
<div id="chat-container">
    <div id="messages"></div>
</div>
</body>
</html>)HTML");
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