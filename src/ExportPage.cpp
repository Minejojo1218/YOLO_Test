#include "ExportPage.h"
#include "ProjectManager.h"
#include "PythonRunner.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

ExportPage::ExportPage(ProjectManager *project, QWidget *parent)
    : QWidget(parent)
    , m_project(project)
    , m_runner(new PythonRunner(this))
{
    QGroupBox *box = new QGroupBox("导出配置", this);
    QFormLayout *form = new QFormLayout(box);

    QHBoxLayout *wRow = new QHBoxLayout();
    m_weights = new QLineEdit(box);
    m_weights->setPlaceholderText("选择训练得到的 best.pt / last.pt");
    QPushButton *bWeights = new QPushButton("浏览", box);
    wRow->addWidget(m_weights, 1);
    wRow->addWidget(bWeights);
    form->addRow("源模型 (.pt)", wRow);

    QHBoxLayout *oRow = new QHBoxLayout();
    m_out = new QLineEdit(box);
    m_out->setReadOnly(true);
    QPushButton *bOut = new QPushButton("浏览", box);
    oRow->addWidget(m_out, 1);
    oRow->addWidget(bOut);
    form->addRow("输出 ONNX", oRow);

    m_opset = new QComboBox(box);
    m_opset->addItems({"12", "13", "17"});
    m_opset->setToolTip("OpenCV DNN 4.10 建议使用 opset 12");
    m_simplify = new QCheckBox("简化计算图（onnxslim）", box);
    m_simplify->setChecked(true);
    QHBoxLayout *o2 = new QHBoxLayout();
    o2->addWidget(new QLabel("opset"));
    o2->addWidget(m_opset);
    o2->addWidget(m_simplify);
    o2->addStretch(1);
    form->addRow("选项", o2);

    QLabel *tip = new QLabel(
        "说明：导出固定 640×640 输入的 ONNX，供 OpenCV DNN 推理使用。\n"
        "YOLO26 等端到端模型会自动转为经典 NMS 格式（兼容 OpenCV 4.10）。", box);
    tip->setWordWrap(true);
    form->addRow("", tip);

    m_btnExport = new QPushButton("开始导出", box);
    m_btnExport->setMinimumHeight(36);
    form->addRow("", m_btnExport);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(3000);

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->addWidget(box);
    lay->addWidget(new QLabel("导出日志"));
    lay->addWidget(m_log, 1);

    connect(bWeights, &QPushButton::clicked, this, &ExportPage::onBrowseWeights);
    connect(bOut, &QPushButton::clicked, this, [this]() {
        const QString p = QFileDialog::getSaveFileName(this, "选择导出路径",
                                                       m_out->text(), "ONNX 模型 (*.onnx)");
        if (!p.isEmpty())
            m_out->setText(p.endsWith(".onnx") ? p : p + ".onnx");
    });
    connect(m_btnExport, &QPushButton::clicked, this, &ExportPage::onExport);
    connect(m_runner, &PythonRunner::logLine, this, &ExportPage::onLog);
    connect(m_runner, &PythonRunner::result, this, &ExportPage::onResult);
    connect(m_runner, &PythonRunner::finished, this, &ExportPage::onFinished);
}

void ExportPage::refresh()
{
    if (!m_weights->text().isEmpty() && QFileInfo::exists(m_weights->text()))
        return;
    const QString latest = findLatestWeights();
    if (!latest.isEmpty()) {
        m_weights->setText(latest);
        syncOutPath();
        appendLog("已自动定位最近训练权重：" + latest);
    }
}

QString ExportPage::findLatestWeights() const
{
    if (!m_project || !m_project->isValid())
        return QString();
    QDirIterator it(m_project->rootDir() + "/runs", QStringList() << "best.pt",
                    QDir::Files, QDirIterator::Subdirectories);
    QString best;
    QDateTime newest;
    while (it.hasNext()) {
        const QString p = it.next();
        const QDateTime t = QFileInfo(p).lastModified();
        if (best.isEmpty() || t > newest) {
            best = p;
            newest = t;
        }
    }
    return best;
}

void ExportPage::syncOutPath()
{
    const QString w = m_weights->text().trimmed();
    if (!w.isEmpty())
        m_out->setText(w.left(w.lastIndexOf('.')) + ".onnx");
}

void ExportPage::onBrowseWeights()
{
    const QString p = QFileDialog::getOpenFileName(
        this, "选择 YOLO 权重",
        m_project && m_project->isValid() ? m_project->rootDir() : QString(),
        "PyTorch 权重 (*.pt)");
    if (p.isEmpty())
        return;
    m_weights->setText(p);
    syncOutPath();
}

void ExportPage::onExport()
{
    if (m_runner->isRunning())
        return;
    const QString w = m_weights->text().trimmed();
    if (w.isEmpty() || !QFileInfo::exists(w)) {
        QMessageBox::warning(this, "提示", "请选择有效的 .pt 权重文件。");
        return;
    }
    syncOutPath();

    appendLog("==== 开始导出 ====");
    appendLog("源模型: " + w);
    appendLog("opset=" + m_opset->currentText()
              + "  简化=" + (m_simplify->isChecked() ? "是" : "否"));
    QStringList args;
    args << "--weights" << w << "--out" << m_out->text()
         << "--opset" << m_opset->currentText();
    if (m_simplify->isChecked())
        args << "--simplify";
    m_btnExport->setEnabled(false);
    m_runner->run("export.py", args);
}

void ExportPage::appendLog(const QString &line)
{
    m_log->appendPlainText(line);
}

void ExportPage::onLog(const QString &line)
{
    appendLog(line);
}

void ExportPage::onResult(const QJsonObject &info)
{
    const bool ok = info.value("status").toString() == "success";
    if (ok) {
        appendLog("==== 导出完成 ====");
        appendLog("ONNX: " + info.value("onnx").toString());
    } else {
        appendLog("==== 导出失败 ====");
        appendLog(info.value("error").toString());
    }
}

void ExportPage::onFinished(int code, QProcess::ExitStatus status)
{
    m_btnExport->setEnabled(true);
    appendLog(QString("进程结束，退出码 %1 (%2)")
                  .arg(code)
                  .arg(status == QProcess::CrashExit ? "崩溃" : "正常"));
}
