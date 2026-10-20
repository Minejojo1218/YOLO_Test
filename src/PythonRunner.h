#pragma once

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

// 调用本机 Python 跑训练/导出/验证脚本，逐行解析脚本打印的 JSON 进度
class PythonRunner : public QObject
{
    Q_OBJECT
public:
    explicit PythonRunner(QObject *parent = nullptr);
    ~PythonRunner();

    // 手动覆盖自动探测结果（可为空表示自动）
    void setPythonPath(const QString &p) { m_pythonPath = p; }
    void setBackendDir(const QString &d) { m_backendDir = d; }

    // 解析 python.exe 与 backend 目录（自动向上探测，找不到则回退）
    QString pythonPath() const;
    QString backendDir() const;

    static QString findPythonExe();
    static QString findBackendDir();

    // 运行 backendDir 下的脚本，args 传给脚本
    void run(const QString &script, const QStringList &args);

    bool isRunning() const;
    void stop();

signals:
    void logLine(const QString &line);               // 原始 stdout 行
    void progress(const QJsonObject &info);          // {"type":"progress",...}
    void result(const QJsonObject &info);            // {"type":"result",...}
    void finished(int exitCode, QProcess::ExitStatus status);

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onFinished(int code, QProcess::ExitStatus status);

private:
    void handleLine(const QString &line);

    QProcess *m_proc = nullptr;
    QString m_pythonPath;
    QString m_backendDir;
    QString m_stdoutBuf;
};
