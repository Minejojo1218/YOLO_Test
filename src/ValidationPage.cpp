#include "ValidationPage.h"
#include "InferenceWorker.h"
#include "ProjectManager.h"
#include "PythonRunner.h"

#include <opencv2/opencv.hpp>

#include <QDir>
#include <QDirIterator>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QTableWidget>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>

ValidationPage::ValidationPage(ProjectManager *project, QWidget *parent)
    : QWidget(parent)
    , m_project(project)
    , m_runner(new PythonRunner(this))
{
    // ---------- 推理工作线程（模型加载、推理、摄像头采集全部在线程内，不阻塞界面） ----------
    m_workerThread = new QThread(this);
    m_worker = new InferenceWorker();
    m_worker->moveToThread(m_workerThread);
    connect(m_worker, &InferenceWorker::logLine, this, &ValidationPage::onRunnerLog);
    connect(m_worker, &InferenceWorker::modelLoaded, this, &ValidationPage::onWorkerModelLoaded);
    connect(m_worker, &InferenceWorker::imageDone, this, &ValidationPage::onWorkerImageDone);
    connect(m_worker, &InferenceWorker::folderProgress, this, &ValidationPage::onWorkerFolderProgress);
    connect(m_worker, &InferenceWorker::folderDone, this, &ValidationPage::onWorkerFolderDone);
    connect(m_worker, &InferenceWorker::frameDone, this, &ValidationPage::onWorkerFrameDone);
    connect(m_worker, &InferenceWorker::cameraOpened, this, &ValidationPage::onWorkerCameraOpened);
    connect(m_worker, &InferenceWorker::cameraClosed, this, &ValidationPage::onWorkerCameraClosed);
    m_workerThread->start();

    // ---------- 顶部控制 ----------
    m_modelPath = new QLineEdit(this);
    m_modelPath->setPlaceholderText("选择 ONNX 模型路径");
    QPushButton *bBrowse = new QPushButton("浏览", this);
    m_conf = new QDoubleSpinBox(this);
    m_conf->setRange(0.01, 1.0);
    m_conf->setSingleStep(0.05);
    m_conf->setValue(0.25);
    m_conf->setDecimals(2);

    QPushButton *bImg = new QPushButton("推理单张图片", this);
    QPushButton *bFolder = new QPushButton("推理整个文件夹", this);
    m_btnCamera = new QPushButton("打开摄像头", this);
    QPushButton *bVal = new QPushButton("验证集指标", this);
    m_btnPlots = new QPushButton("打开评估图表", this);
    m_btnLog = new QPushButton("导出日志", this);
    m_btnPrev = new QPushButton("◀ 上一张", this);
    m_btnNext = new QPushButton("下一张 ▶", this);
    m_btnPrev->setEnabled(false);
    m_btnNext->setEnabled(false);

    QHBoxLayout *ctl = new QHBoxLayout();
    ctl->addWidget(new QLabel("模型：", this));
    ctl->addWidget(m_modelPath, 1);
    ctl->addWidget(bBrowse);
    ctl->addWidget(new QLabel("置信度：", this));
    ctl->addWidget(m_conf);
    ctl->addWidget(bImg);
    ctl->addWidget(bFolder);
    ctl->addWidget(m_btnPrev);
    ctl->addWidget(m_btnNext);
    ctl->addWidget(m_btnCamera);
    ctl->addWidget(bVal);
    ctl->addWidget(m_btnPlots);
    ctl->addWidget(m_btnLog);

    // ---------- 图像显示 ----------
    m_imageLabel = new QLabel("推理结果将显示在这里", this);
    m_imageLabel->setObjectName("imageLabel");
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(400, 300);
    m_imageLabel->setScaledContents(false);

    // ---------- 检测结果表格 ----------
    m_detsTable = new QTableWidget(0, 7, this);
    m_detsTable->setHorizontalHeaderLabels({"序号", "类别", "置信度", "X1", "Y1", "X2", "Y2"});
    m_detsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_detsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_detsTable->verticalHeader()->setVisible(false);
    m_detsTable->horizontalHeader()->setStretchLastSection(true);
    m_detsTable->setMaximumHeight(150);
    m_detsTable->setAlternatingRowColors(true);

    // ---------- 日志 ----------
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(2000);

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->addLayout(ctl);
    lay->addWidget(m_imageLabel, 1);
    lay->addWidget(m_detsTable);
    lay->addWidget(m_log);

    connect(bBrowse, &QPushButton::clicked, this, &ValidationPage::onBrowseModel);
    connect(bImg, &QPushButton::clicked, this, &ValidationPage::onInferImage);
    connect(bFolder, &QPushButton::clicked, this, &ValidationPage::onInferFolder);
    connect(m_btnPrev, &QPushButton::clicked, this, &ValidationPage::onPrevResult);
    connect(m_btnNext, &QPushButton::clicked, this, &ValidationPage::onNextResult);
    connect(m_btnCamera, &QPushButton::clicked, this, &ValidationPage::onToggleCamera);
    connect(bVal, &QPushButton::clicked, this, &ValidationPage::onValidateMetrics);
    connect(m_btnPlots, &QPushButton::clicked, this, [this]() {
        if (m_lastPlotsDir.isEmpty()) {
            appendLog("还没有评估图表，请先跑「验证集指标」。");
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastPlotsDir));
    });
    connect(m_btnLog, &QPushButton::clicked, this, [this]() {
        const QString p = QFileDialog::getSaveFileName(this, "导出验证日志",
                                                       m_project ? m_project->rootDir() + "/val_log.txt" : QString(),
                                                       "文本 (*.txt)");
        if (p.isEmpty())
            return;
        QFile f(p);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            f.write(m_log->toPlainText().toUtf8());
            appendLog("日志已导出：" + p);
        }
    });

    connect(m_runner, &PythonRunner::logLine, this, &ValidationPage::onRunnerLog);
    connect(m_runner, &PythonRunner::result, this, &ValidationPage::onRunnerResult);
    connect(m_runner, &PythonRunner::finished, this, &ValidationPage::onRunnerFinished);
}

ValidationPage::~ValidationPage()
{
    if (m_workerThread) {
        // 先在工作线程内安全关闭摄像头（阻塞等待其处理完毕）
        if (m_cameraOn)
            QMetaObject::invokeMethod(m_worker, "stopCamera", Qt::BlockingQueuedConnection);
        m_worker->abort();              // 原子置位，长任务尽快退出（线程安全）
        m_workerThread->quit();
        m_workerThread->wait(3000);     // 等待当前任务结束
        delete m_worker;
        m_worker = nullptr;
    }
}

void ValidationPage::refresh()
{
    if (m_project && m_project->isValid()) {
        // 自动尝试定位最近导出的 onnx
        if (m_modelPath->text().isEmpty()) {
            const QString best = findBestPt();
            if (!best.isEmpty()) {
                const QString onnx = best.left(best.lastIndexOf('.')) + ".onnx";
                if (QFileInfo::exists(onnx))
                    m_modelPath->setText(onnx);
            }
        }
    }
}

void ValidationPage::appendLog(const QString &line)
{
    m_log->appendPlainText(line);
}

void ValidationPage::showImage(const QImage &img)
{
    if (img.isNull())
        return;
    QPixmap pm = QPixmap::fromImage(img).scaled(m_imageLabel->size() - QSize(8, 8),
                                                Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_imageLabel->setPixmap(pm);
}

// 确保模型已加载：已加载则返回 true；否则异步加载并返回 false（任务已挂起，加载完成自动继续）
bool ValidationPage::ensureModelRequested()
{
    const QString path = m_modelPath->text().trimmed();
    if (path.isEmpty()) {
        appendLog("请先选择 ONNX 模型（点击「浏览」选择 .onnx 文件）。");
        return false;
    }
    if (m_modelReady && m_loadedModelPath == path)
        return true;
    if (m_loadedModelPath != path) {
        m_modelReady = false;
        appendLog("正在加载模型：" + path);
        QMetaObject::invokeMethod(m_worker, "loadModel", Qt::QueuedConnection,
                                  Q_ARG(QString, path));
    }
    return false; // 加载进行中（m_loadedModelPath == path 且未就绪），等 modelLoaded 信号
}

void ValidationPage::runPendingTask()
{
    const PendingTask t = m_pendingTask;
    m_pendingTask = PendingTask::None;
    const QStringList classes = m_project ? m_project->classes() : QStringList();
    const float conf = (float)m_conf->value();
    switch (t) {
    case PendingTask::InferImage:
        QMetaObject::invokeMethod(m_worker, "inferImage", Qt::QueuedConnection,
                                  Q_ARG(QString, m_pendingArg1), Q_ARG(float, conf),
                                  Q_ARG(QStringList, classes));
        break;
    case PendingTask::InferFolder:
        QMetaObject::invokeMethod(m_worker, "inferFolder", Qt::QueuedConnection,
                                  Q_ARG(QString, m_pendingArg1), Q_ARG(QString, m_pendingArg2),
                                  Q_ARG(float, conf), Q_ARG(QStringList, classes));
        break;
    case PendingTask::OpenCamera:
        // 摄像头采集与推理都在工作线程执行，界面只接收 frameDone 回调
        QMetaObject::invokeMethod(m_worker, "startCamera", Qt::QueuedConnection,
                                  Q_ARG(float, conf), Q_ARG(QStringList, classes));
        break;
    default:
        break;
    }
}

void ValidationPage::onBrowseModel()
{
    const QString p = QFileDialog::getOpenFileName(this, "选择 ONNX 模型",
                                                   m_project ? m_project->rootDir() : QString(),
                                                   "ONNX 模型 (*.onnx)");
    if (p.isEmpty())
        return;
    m_modelPath->setText(p);
    m_loadedModelPath.clear(); // 下次推理时按新路径重新加载
    m_modelReady = false;
}

void ValidationPage::onInferImage()
{
    const QString imgPath = QFileDialog::getOpenFileName(this, "选择图片", QString(),
        "图片 (*.jpg *.jpeg *.png *.bmp *.webp)");
    if (imgPath.isEmpty())
        return;
    inferImagePath(imgPath);
}

// 单张推理（含上一张/下一张复用）
void ValidationPage::inferImagePath(const QString &path)
{
    m_pendingTask = PendingTask::InferImage;
    m_pendingArg1 = path;
    m_pendingArg2.clear();
    if (!ensureModelRequested())
        return;
    runPendingTask();
}

void ValidationPage::onInferFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, "选择图片文件夹", QString());
    if (dir.isEmpty())
        return;

    m_pendingTask = PendingTask::InferFolder;
    m_pendingArg1 = dir;
    m_pendingArg2 = dir + "/predict";
    m_lastProgressLogged = -1;
    // 记录文件夹内图片，供「上一张/下一张」浏览结果
    const QDir d(dir);
    const QStringList filters = {"*.jpg", "*.jpeg", "*.png", "*.bmp", "*.webp"};
    m_folderFiles.clear();
    for (const QFileInfo &fi : d.entryInfoList(filters, QDir::Files, QDir::Name))
        m_folderFiles.append(fi.absoluteFilePath());
    m_folderIndex = m_folderFiles.isEmpty() ? -1 : 0;
    m_btnPrev->setEnabled(!m_folderFiles.isEmpty());
    m_btnNext->setEnabled(!m_folderFiles.isEmpty());
    appendLog("文件夹推理开始：" + dir);
    appendLog("结果将保存到：" + m_pendingArg2);
    if (!ensureModelRequested())
        return;
    runPendingTask();
}

void ValidationPage::onPrevResult()
{
    if (m_folderFiles.isEmpty()) {
        appendLog("提示：先跑一次「推理整个文件夹」，才能用上一张/下一张浏览结果。");
        return;
    }
    m_folderIndex = qBound(0, m_folderIndex - 1, m_folderFiles.size() - 1);
    appendLog(QString("浏览结果：%1/%2").arg(m_folderIndex + 1).arg(m_folderFiles.size()));
    inferImagePath(m_folderFiles[m_folderIndex]);
}

void ValidationPage::onNextResult()
{
    if (m_folderFiles.isEmpty()) {
        appendLog("提示：先跑一次「推理整个文件夹」，才能用上一张/下一张浏览结果。");
        return;
    }
    m_folderIndex = qBound(0, m_folderIndex + 1, m_folderFiles.size() - 1);
    appendLog(QString("浏览结果：%1/%2").arg(m_folderIndex + 1).arg(m_folderFiles.size()));
    inferImagePath(m_folderFiles[m_folderIndex]);
}

void ValidationPage::onToggleCamera()
{
    if (m_cameraOn) {
        m_cameraOn = false;
        m_btnCamera->setText("打开摄像头");
        QMetaObject::invokeMethod(m_worker, "stopCamera", Qt::BlockingQueuedConnection);
        appendLog("摄像头已关闭。");
        return;
    }
    m_pendingTask = PendingTask::OpenCamera;
    if (!ensureModelRequested())
        return;
    runPendingTask();
}

// ---------- 推理工作线程回调 ----------

void ValidationPage::onWorkerModelLoaded(const QString &path, bool ok, const QString &error)
{
    if (ok) {
        m_modelReady = true;
        m_loadedModelPath = path;
        appendLog("模型加载完成：" + path);
        runPendingTask();
    } else {
        m_modelReady = false;
        m_pendingTask = PendingTask::None; // 清掉挂起任务，避免模型加载失败后残留
        appendLog("模型加载失败：" + error);
    }
}

void ValidationPage::onWorkerImageDone(const QString &tag, const QImage &result, int count,
                                       const QString &diag, const QJsonArray &dets)
{
    if (!result.isNull())
        showImage(result);
    fillDetTable(dets);
    if (!tag.isEmpty()) {
        QString msg = QString("图片 %1：检出 %2 个目标").arg(tag).arg(count);
        if (!diag.isEmpty())
            msg += QString("  [%1]").arg(diag);
        appendLog(msg);
    }
}

// 检测结果表格（类别 / 置信度 / 框坐标）
void ValidationPage::fillDetTable(const QJsonArray &dets)
{
    m_detsTable->setRowCount(dets.size());
    for (int i = 0; i < dets.size(); ++i) {
        const QJsonObject o = dets[i].toObject();
        const int cls = o.value("cls").toInt();
        QString clsName = QString::number(cls);
        if (m_project && m_project->isValid() && cls >= 0 && cls < m_project->classes().size())
            clsName = m_project->classes()[cls];
        const double x = o.value("x").toDouble();
        const double y = o.value("y").toDouble();
        const double w = o.value("w").toDouble();
        const double h = o.value("h").toDouble();

        auto setCell = [&](int col, const QString &text) {
            m_detsTable->setItem(i, col, new QTableWidgetItem(text));
        };
        setCell(0, QString::number(i + 1));
        setCell(1, clsName);
        setCell(2, QString::number(o.value("conf").toDouble(), 'f', 3));
        setCell(3, QString::number(x, 'f', 0));
        setCell(4, QString::number(y, 'f', 0));
        setCell(5, QString::number(x + w, 'f', 0));
        setCell(6, QString::number(y + h, 'f', 0));
    }
}

void ValidationPage::onWorkerFolderProgress(int done, int total)
{
    if (done == total || done - m_lastProgressLogged >= 10) {
        m_lastProgressLogged = done;
        appendLog(QString("已处理 %1/%2").arg(done).arg(total));
    }
}

void ValidationPage::onWorkerFolderDone(int processed, int total, const QString &outDir,
                                        const QString &diag)
{
    QString msg = QString("文件夹推理完成：%1/%2 张，结果保存在 %3")
                      .arg(processed).arg(total).arg(outDir);
    if (!diag.isEmpty())
        msg += QString("  [%1]").arg(diag);
    appendLog(msg);
}

void ValidationPage::onWorkerFrameDone(const QImage &result, int count)
{
    if (!result.isNull())
        showImage(result);
}

void ValidationPage::onWorkerCameraOpened(bool ok, const QString &error)
{
    if (ok) {
        m_cameraOn = true;
        m_btnCamera->setText("关闭摄像头");
        appendLog("摄像头已打开（采集与推理在工作线程执行，界面不会卡顿）。");
    } else {
        m_cameraOn = false;
        m_btnCamera->setText("打开摄像头");
        appendLog("无法打开摄像头：" + error);
    }
}

void ValidationPage::onWorkerCameraClosed()
{
    m_cameraOn = false;
    m_btnCamera->setText("打开摄像头");
}

// ---------- 验证集指标 ----------

QString ValidationPage::findBestPt()
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

void ValidationPage::onValidateMetrics()
{
    if (m_runner->isRunning())
        return;
    if (!m_project || !m_project->isValid()) {
        appendLog("请先打开项目。");
        return;
    }
    QString pt = findBestPt();
    if (pt.isEmpty()) {
        pt = QFileDialog::getOpenFileName(this, "选择权重 (.pt)", m_project->rootDir(),
                                          "PyTorch 权重 (*.pt)");
    }
    if (pt.isEmpty()) {
        appendLog("未找到权重文件。");
        return;
    }
    const QString data = m_project->dataYamlPath();
    if (!QFileInfo::exists(data))
        m_project->generateDataYaml(0.2);

    appendLog("==== 验证集指标 ====");
    appendLog("说明：用 data.yaml 的 val 划分对指定 .pt 权重做评估，输出 mAP50 / mAP50-95 / Precision / Recall。");
    appendLog("权重: " + pt);
    m_runner->run("validate.py", QStringList() << "--weights" << pt << "--data" << data);
}

void ValidationPage::onRunnerLog(const QString &line)
{
    appendLog(line);
}

void ValidationPage::onRunnerResult(const QJsonObject &info)
{
    const bool ok = info.value("status").toString() == "success";
    appendLog(ok ? "==== 评估完成 ====" : "==== 评估失败 ====");
    appendLog(QString("mAP50=%1  mAP50-95=%2  Precision=%3  Recall=%4")
                  .arg(info.value("map50").toDouble(), 0, 'f', 4)
                  .arg(info.value("map5095").toDouble(), 0, 'f', 4)
                  .arg(info.value("precision").toDouble(), 0, 'f', 4)
                  .arg(info.value("recall").toDouble(), 0, 'f', 4));
    if (ok && info.contains("f1"))
        appendLog(QString("F1=%1  推理速度=%2 ms/图  模型大小=%3 MB")
                      .arg(info.value("f1").toDouble(), 0, 'f', 4)
                      .arg(info.value("speed_ms").toDouble(), 0, 'f', 2)
                      .arg(info.value("model_mb").toDouble(), 0, 'f', 2));
    if (ok) {
        const QString sd = info.value("save_dir").toString();
        if (!sd.isEmpty()) {
            m_lastPlotsDir = sd;
            appendLog("评估图表已保存：" + sd);
            appendLog("（点「打开评估图表」查看混淆矩阵 / PR / F1 曲线）");
        }
    }
}

void ValidationPage::onRunnerFinished(int code, QProcess::ExitStatus status)
{
    appendLog(QString("进程结束，退出码 %1").arg(code));
}
