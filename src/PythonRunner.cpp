#include "PythonRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

PythonRunner::PythonRunner(QObject *parent)
    : QObject(parent)
    , m_proc(new QProcess(this))
{
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &PythonRunner::onReadyReadStdout);
    connect(m_proc, &QProcess::readyReadStandardError, this, &PythonRunner::onReadyReadStderr);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PythonRunner::onFinished);
}

PythonRunner::~PythonRunner()
{
    // 先断开信号，避免析构期间 kill 触发 finished 回调到已析构对象
    disconnect(m_proc, nullptr, this, nullptr);
    if (m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        m_proc->waitForFinished(2000);
    }
}

// 从可执行文件目录向上查找 runtime\Scripts\python.exe
QString PythonRunner::findPythonExe()
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        const QString candidate = dir.filePath("runtime/Scripts/python.exe");
        if (QFileInfo::exists(candidate))
            return QDir::cleanPath(candidate);
        if (!dir.cdUp())
            break;
    }
    return "python"; // 回退到 PATH
}

// 从可执行文件目录向上查找 backend\train.py
QString PythonRunner::findBackendDir()
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        if (QFileInfo::exists(dir.filePath("backend/train.py")))
            return QDir::cleanPath(dir.filePath("backend"));
        if (!dir.cdUp())
            break;
    }
    return QString();
}

QString PythonRunner::pythonPath() const
{
    if (!m_pythonPath.isEmpty())
        return m_pythonPath;
    return findPythonExe();
}

QString PythonRunner::backendDir() const
{
    if (!m_backendDir.isEmpty())
        return m_backendDir;
    return findBackendDir();
}

void PythonRunner::run(const QString &script, const QStringList &args)
{
    if (m_proc->state() != QProcess::NotRunning)
        return;

    const QString bd = backendDir();
    const QString scriptPath = bd.isEmpty() ? script : bd + "/" + script;

    QStringList fullArgs;
    fullArgs << scriptPath << args;

    m_stdoutBuf.clear();
    m_proc->start(pythonPath(), fullArgs);
}

bool PythonRunner::isRunning() const
{
    return m_proc->state() != QProcess::NotRunning;
}

void PythonRunner::stop()
{
    if (m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        m_proc->waitForFinished(2000);
    }
}

void PythonRunner::onReadyReadStdout()
{
    const QByteArray data = m_proc->readAllStandardOutput();
    m_stdoutBuf += QString::fromUtf8(data);
    int idx;
    while ((idx = m_stdoutBuf.indexOf('\n')) >= 0) {
        const QString line = m_stdoutBuf.left(idx).trimmed();
        m_stdoutBuf.remove(0, idx + 1);
        if (!line.isEmpty())
            handleLine(line);
    }
}

void PythonRunner::onReadyReadStderr()
{
    const QByteArray data = m_proc->readAllStandardError();
    const QString text = QString::fromUtf8(data);
    for (const QString &line : text.split('\n')) {
        if (!line.trimmed().isEmpty())
            emit logLine(line.trimmed());
    }
}

void PythonRunner::onFinished(int code, QProcess::ExitStatus status)
{
    // 处理剩余缓冲
    if (!m_stdoutBuf.trimmed().isEmpty())
        handleLine(m_stdoutBuf.trimmed());
    m_stdoutBuf.clear();
    emit finished(code, status);
}

void PythonRunner::handleLine(const QString &line)
{
    // 尝试解析为 JSON；失败则当作普通日志
    const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        const QString type = obj.value("type").toString();
        if (type == "progress") {
            emit progress(obj);
            return;
        }
        if (type == "result") {
            emit result(obj);
            return;
        }
    }
    emit logLine(line);
}
