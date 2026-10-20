#pragma once

#include <QJsonObject>
#include <QProcess>
#include <QWidget>

class ProjectManager;
class PythonRunner;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QPlainTextEdit;
class QPushButton;

// 模型导出页：选择权重 → ONNX 导出（opset / 简化）→ 日志
class ExportPage : public QWidget
{
    Q_OBJECT
public:
    explicit ExportPage(ProjectManager *project, QWidget *parent = nullptr);

    void refresh();

private slots:
    void onBrowseWeights();
    void onExport();
    void onLog(const QString &line);
    void onResult(const QJsonObject &info);
    void onFinished(int code, QProcess::ExitStatus status);

private:
    QString findLatestWeights() const;
    void appendLog(const QString &line);
    void syncOutPath();

    ProjectManager *m_project;
    PythonRunner *m_runner;

    QLineEdit *m_weights = nullptr;
    QLineEdit *m_out = nullptr;
    QComboBox *m_opset = nullptr;
    QCheckBox *m_simplify = nullptr;
    QPushButton *m_btnExport = nullptr;
    QPlainTextEdit *m_log = nullptr;
};
