#ifndef DOCKSIGINT_H
#define DOCKSIGINT_H

#include "spectrum_capture.h"
#include "spectrum_visualizer.h"
#include "waterfall_display.h"
#include "../applications/gqrx/receiver.h"
#include <QDockWidget>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebChannel>
#include <QSqlDatabase>
#include <QThread>
#include <QDateTime>
#include <QProcess>
#include <memory>
#include <functional>

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
    QProcess *pythonProcess;
    bool analyzeTuningRequest(const QString &message);
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
    void saveSetting(const QString &key, const QString &value);
    void loadSetting(const QString &key);

signals:
    void messageSaved(qint64 id);
    void historyLoaded(const QVector<QPair<QString, QString>> &messages);
    void chatsLoaded(const QVector<QPair<int, QString>> &chats);
    void chatCreated(int chatId, const QString &name);
    void error(const QString &message);
    void settingLoaded(const QString &key, const QString &value);

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
    explicit DockSigint(receiver *rx_ptr, QWidget *parent = nullptr);
    ~DockSigint();

    void saveSettings(QSettings *settings);
    void readSettings(QSettings *settings);

signals:
    void sendMessageToWorker(const QString &apiKey, const QString &model, const QJsonArray &messages);
    void saveMessageToDb(int chatId, const QString &role, const QString &content);
    void loadHistoryFromDb(int chatId);

public slots:
    void onReceiverDestroyed() { rx_ptr = nullptr; }

private slots:
    void onSendClicked();
    void onReturnPressed();
    void onWorkerMessageReceived(const QString &message);
    void onWorkerErrorOccurred(const QString &error);
    void onNewChatClicked();
    void onChatSelected(int index);
    void onChatsLoaded(const QVector<QPair<int, QString>> &chats);
    void onDspStateChanged(bool running);
    void onTabChanged(const QString &tabName);
    void onLessonSelected(int index);
    void onNewFFTData(const std::vector<float>& fft_data, double center_freq, double bandwidth, double sample_rate);
    void runWaterfallOptimizer();  // New slot for FFT optimization
    void startFMTransmission();   // New slot for FM transmission
    
    // Spectrum capture slots
    void onCaptureStarted(const SpectrumCapture::CaptureRange& range);
    void onCaptureComplete(const SpectrumCapture::CaptureResult& result);
    void onCaptureError(const std::string& error);
    void onCaptureProgress(int percent);
    void testSpectrumCapture();  // Test function

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

    // Screenshot functionality
    QString getScreenshotPath() const;
    void captureWaterfallScreenshot();
    QWidget* findWaterfallWidget() const;

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
    
    // Spectrum capture and visualization
    std::unique_ptr<SpectrumCapture> spectrumCapture;
    SpectrumVisualizer *spectrumVisualizer;
    std::unique_ptr<WaterfallDisplay> waterfallDisplay;  // Add waterfall display

    receiver *rx_ptr;
    bool dsp_running;  // Track DSP state locally

    // Tab management
    QString currentTab;
    QWidget *spectrumContainer;
    QWidget *waterfallContainer;
    
    void loadEnvironmentVariables();
    QString getBaseHtml();
    void initializeWebView();
    void updateChatView();
    void appendMessage(const QString &message, bool isUser = true);
    void appendMessageToView(const QString &message, bool isUser);
    void sendToClaude(const QString &message, std::function<void(const QString&)> callback = nullptr);
    void sendToClaude(const QString &message, const QByteArray &imageData, std::function<void(const QString&)> callback = nullptr);
    QString getDatabasePath();
    void loadChats();
    void createNewChat();
    void switchToChat(int chatId);
    void updateChatSelector();
    void updateLessonSelector();
    void clearChat();
    void setupTabSystem();
    void moveVisualizerToTab();

    // Add new member variables
    bool m_lastActiveChatLoaded;
    bool m_chatsLoaded;
    
    // Add new member function
    void switchToLastActiveChat();
};

#endif // DOCKSIGINT_H 