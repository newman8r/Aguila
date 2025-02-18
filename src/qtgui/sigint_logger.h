#pragma once

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

// Forward declaration of the implementation class
class SigintLogger {
public:
    static void initialize(const QString& logPath);
    static void debug(const QString& msg);
    static void info(const QString& msg);
    static void warning(const QString& msg);
    static void error(const QString& msg);
    static void cleanup();

private:
    QFile logFile;
    QTextStream* logStream;
    
    SigintLogger();
    ~SigintLogger();
    
    static SigintLogger& instance();
    void initializeImpl(const QString& logPath);
    void logImpl(const QString& level, const char* color, const QString& msg);
    void cleanupImpl();
}; 