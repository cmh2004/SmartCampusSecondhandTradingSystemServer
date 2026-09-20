#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QMutex>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

class Logger : public QObject
{
    Q_OBJECT
public:
    enum LogLevel { Debug = 0, Info, Warning, Critical };
    Q_ENUM(LogLevel)

    static Logger* instance();
    void initialize(const QString& logDir = "logs",
                    LogLevel minLevel = Info,
                    bool outputToConsole = true,
                    int maxFileSizeMB = 10,
                    int maxBackupFiles = 5);
    void setLogLevel(LogLevel level) { m_minLevel = level; }

private:
    explicit Logger(QObject* parent = nullptr);
    ~Logger();
    static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);
    void writeLog(QtMsgType type, const QString& msg, const QString& file, int line);
    void rotateIfNeeded();

    static Logger* m_instance;
    QMutex m_mutex;
    QFile m_logFile;
    QTextStream m_stream;
    LogLevel m_minLevel;
    bool m_outputToConsole;
    int m_maxFileSize;
    int m_maxBackup;
    QString m_currentLogFileName;
};

#endif // LOGGER_H
