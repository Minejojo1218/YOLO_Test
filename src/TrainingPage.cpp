#include "TrainingPage.h"
#include "Database.h"
#include "ProjectManager.h"
#include "PythonRunner.h"
#include "TrainConfig.h"
#include "TrainingCurveWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

TrainingPage::TrainingPage(ProjectManager *project, Database *db, QWidget *parent)
    : QWidget(parent)
    , m_project(project)
    , m_db(db)
    , m_runner(new PythonRunner(this))
{
    // ---------- 参数表单 ----------
    m_model = new QComboBox(this);
    m_model->addItems({"yolov8n.pt", "yolov8s.pt", "yolov8m.pt", "yolov8l.pt", "yolov8x.pt",
                       "yolov11n.pt", "yolov11s.pt", "yolov11m.pt", "yolov11l.pt", "yolov11x.pt",
                       "yolov12n.pt", "yolov12s.pt", "yolov12m.pt", "yolov12l.pt", "yolov12x.pt",
                       "yolo26n.pt", "yolo26s.pt", "yolo26m.pt", "yolo26l.pt", "yolo26x.pt",
                       "yolov8n-seg.pt", "yolov8s-seg.pt", "yolov11n-seg.pt", "yolov11s-seg.pt",
                       "yolo26n-seg.pt", "yolo26s-seg.pt"});
    m_model->setEditable(true);

    m_epochs = new QSpinBox(this);
    m_epochs->setRange(1, 10000);
    m_epochs->setValue(100);

    m_batch = new QSpinBox(this);
    m_batch->setRange(1, 512);
    m_batch->setValue(16);

    m_imgsz = new QSpinBox(this);
    m_imgsz->setRange(128, 2560);
    m_imgsz->setValue(640);

    m_device = new QComboBox(this);
    m_device->addItems({"0", "cpu"});
    m_device->setEditable(true);

    m_optimizer = new QComboBox(this);
    m_optimizer->addItems({"auto", "SGD", "Adam", "AdamW"});

    m_lr0 = new QDoubleSpinBox(this);
    m_lr0->setRange(0.0001, 1.0);
    m_lr0->setDecimals(4);
    m_lr0->setValue(0.01);

    m_workers = new QSpinBox(this);
    m_workers->setRange(0, 64);
    m_workers->setValue(8);

    m_amp = new QCheckBox("启用 AMP 混合精度", this);
    m_amp->setChecked(true);

    m_negatives = new QCheckBox("把未标注图作为负样本参与训练", this);
    m_negatives->setChecked(false);

    m_mosaic = new QDoubleSpinBox(this);
    m_mosaic->setRange(0.0, 1.0);
    m_mosaic->setSingleStep(0.1);
    m_mosaic->setDecimals(1);
    m_mosaic->setValue(1.0);

    m_fliplr = new QDoubleSpinBox(this);
    m_fliplr->setRange(0.0, 1.0);
    m_fliplr->setSingleStep(0.1);
    m_fliplr->setDecimals(1);
    m_fliplr->setValue(0.5);

    m_degrees = new QDoubleSpinBox(this);
    m_degrees->setRange(0.0, 180.0);
    m_degrees->setSingleStep(5.0);
    m_degrees->setDecimals(1);
    m_degrees->setValue(0.0);

    m_patience = new QSpinBox(this);
    m_patience->setRange(0, 5000);
    m_patience->setValue(50);
    m_patience->setToolTip("早停：连续多少轮无提升就停止（0=关闭）");

    m_warmup = new QDoubleSpinBox(this);
    m_warmup->setRange(0.0, 20.0);
    m_warmup->setDecimals(1);
    m_warmup->setValue(3.0);
    m_warmup->setToolTip("学习率预热轮数");

    m_mixup = new QDoubleSpinBox(this);
    m_mixup->setRange(0.0, 1.0);
    m_mixup->setSingleStep(0.05);
    m_mixup->setDecimals(2);
    m_mixup->setValue(0.0);
    m_mixup->setToolTip("MixUp 混样增强概率");

    // ---- 数据增强（参考 YOLO 开源） ----
    auto makeAugSpin = [this](double lo, double hi, double step, int decimals, double val) {
        QDoubleSpinBox *s = new QDoubleSpinBox(this);
        s->setRange(lo, hi);
        s->setSingleStep(step);
        s->setDecimals(decimals);
        s->setValue(val);
        return s;
    };
    m_hsvH = makeAugSpin(0.0, 1.0, 0.005, 3, 0.015);
    m_hsvS = makeAugSpin(0.0, 1.0, 0.05, 2, 0.7);
    m_hsvV = makeAugSpin(0.0, 1.0, 0.05, 2, 0.4);
    m_translate = makeAugSpin(0.0, 1.0, 0.05, 2, 0.1);
    m_scale = makeAugSpin(0.0, 1.0, 0.05, 2, 0.5);
    m_shear = makeAugSpin(0.0, 45.0, 1.0, 1, 0.0);
    m_flipud = makeAugSpin(0.0, 1.0, 0.1, 1, 0.0);
    m_copyPaste = makeAugSpin(0.0, 1.0, 0.05, 2, 0.0);
    m_lrf = makeAugSpin(0.0001, 1.0, 0.001, 4, 0.01);
    m_momentum = makeAugSpin(0.0, 0.99, 0.01, 3, 0.937);
    m_weightDecay = makeAugSpin(0.0, 0.01, 0.0001, 4, 0.0005);

    QGroupBox *paramBox = new QGroupBox("训练参数", this);
    QFormLayout *form = new QFormLayout(paramBox);
    form->addRow("模型", m_model);
    form->addRow("Epochs", m_epochs);
    form->addRow("Batch", m_batch);
    form->addRow("图像尺寸", m_imgsz);
    form->addRow("设备 (0=GPU / cpu / 0,1=多卡)", m_device);
    form->addRow("优化器", m_optimizer);
    form->addRow("初始学习率", m_lr0);
    form->addRow("Workers", m_workers);
    form->addRow("早停 patience", m_patience);
    form->addRow("warmup epochs", m_warmup);
    form->addRow("lrf 最终学习率因子", m_lrf);
    form->addRow("momentum", m_momentum);
    form->addRow("weight decay", m_weightDecay);
    form->addRow("", m_amp);
    form->addRow("", m_negatives);

    QGroupBox *augBox = new QGroupBox("数据增强", this);
    QFormLayout *augForm = new QFormLayout(augBox);
    augForm->addRow("mosaic (0=关)", m_mosaic);
    augForm->addRow("水平翻转 fliplr", m_fliplr);
    augForm->addRow("垂直翻转 flipud", m_flipud);
    augForm->addRow("旋转角度 degrees", m_degrees);
    augForm->addRow("HSV 色调 hsv_h", m_hsvH);
    augForm->addRow("HSV 饱和 hsv_s", m_hsvS);
    augForm->addRow("HSV 亮度 hsv_v", m_hsvV);
    augForm->addRow("平移 translate", m_translate);
    augForm->addRow("缩放 scale", m_scale);
    augForm->addRow("剪切 shear", m_shear);
    augForm->addRow("MixUp (0=关)", m_mixup);
    augForm->addRow("Copy-Paste (分割)", m_copyPaste);

    QHBoxLayout *boxLay = new QHBoxLayout();
    boxLay->addWidget(paramBox, 1);
    boxLay->addWidget(augBox, 1);

    // ---------- 按钮 ----------
    m_btnTrain = new QPushButton("开始训练", this);
    m_btnStop = new QPushButton("停止", this);
    m_btnStop->setEnabled(false);
    m_btnExport = new QPushButton("导出 ONNX", this);
    m_btnExport->setEnabled(false);
    m_btnPick = new QPushButton("选择权重导出...", this);
    m_btnResume = new QPushButton("断点续训...", this);
    m_btnEnv = new QPushButton("环境检测", this);
    m_btnLog = new QPushButton("导出日志", this);

    QHBoxLayout *btnLay = new QHBoxLayout();
    btnLay->addWidget(m_btnTrain);
    btnLay->addWidget(m_btnStop);
    btnLay->addWidget(m_btnResume);
    btnLay->addWidget(m_btnExport);
    btnLay->addWidget(m_btnPick);
    btnLay->addWidget(m_btnEnv);
    btnLay->addWidget(m_btnLog);
    btnLay->addStretch(1);

    // ---------- 进度 + 日志/曲线 ----------
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(m_log, "训练日志");
    m_curve = new TrainingCurveWidget(this);
    m_tabs->addTab(m_curve, "训练曲线");

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->addLayout(boxLay);
    lay->addLayout(btnLay);
    lay->addWidget(m_progress);
    lay->addWidget(m_tabs, 1);

    connect(m_btnTrain, &QPushButton::clicked, this, &TrainingPage::onStartTrain);
    connect(m_btnStop, &QPushButton::clicked, this, &TrainingPage::onStopTrain);
    connect(m_btnExport, &QPushButton::clicked, this, &TrainingPage::onExportOnnx);
    connect(m_btnPick, &QPushButton::clicked, this, &TrainingPage::onPickWeights);
    connect(m_btnResume, &QPushButton::clicked, this, &TrainingPage::onResumeTrain);
    connect(m_btnEnv, &QPushButton::clicked, this, &TrainingPage::onCheckEnv);
    connect(m_btnLog, &QPushButton::clicked, this, [this]() {
        const QString p = QFileDialog::getSaveFileName(this, "导出训练日志",
                                                       m_project ? m_project->rootDir() + "/train_log.txt" : QString(),
                                                       "文本 (*.txt)");
        if (p.isEmpty())
            return;
        QFile f(p);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            f.write(m_log->toPlainText().toUtf8());
            appendLog("日志已导出：" + p);
        }
    });

    connect(m_runner, &PythonRunner::logLine, this, &TrainingPage::onRunnerLog);
    connect(m_runner, &PythonRunner::progress, this, &TrainingPage::onRunnerProgress);
    connect(m_runner, &PythonRunner::result, this, &TrainingPage::onRunnerResult);
    connect(m_runner, &PythonRunner::finished, this, &TrainingPage::onRunnerFinished);
}

void TrainingPage::refresh()
{
    if (m_project && m_project->isValid())
        appendLog("当前项目：" + m_project->rootDir());
    else
        appendLog("提示：请先在「数据标注」页新建/打开项目。");
}

QString TrainingPage::ensureDataYaml()
{
    if (!m_project || !m_project->isValid())
        return QString();
    // 每次训练前按当前设置重新生成（含负样本开关）
    m_project->generateDataYaml(0.2, m_negatives->isChecked());
    return m_project->dataYamlPath();
}

void TrainingPage::onStartTrain()
{
    if (m_runner->isRunning())
        return;

    const QString data = ensureDataYaml();
    if (data.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先在「数据标注」页新建/打开项目。");
        return;
    }
    if (m_project->labeledCount() < 1) {
        QMessageBox::warning(this, "提示", "没有已标注的图片，请先完成标注。");
        return;
    }

    TrainConfig cfg;
    cfg.model = m_model->currentText().trimmed();
    cfg.data = data;
    cfg.project = m_project->rootDir();
    cfg.name = "train_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    cfg.epochs = m_epochs->value();
    cfg.batch = m_batch->value();
    cfg.imgsz = m_imgsz->value();
    cfg.device = m_device->currentText().trimmed();
    cfg.optimizer = m_optimizer->currentText();
    cfg.lr0 = m_lr0->value();
    cfg.workers = m_workers->value();
    cfg.amp = m_amp->isChecked();
    cfg.includeNegatives = m_negatives->isChecked();
    cfg.mosaic = m_mosaic->value();
    cfg.fliplr = m_fliplr->value();
    cfg.degrees = m_degrees->value();
    cfg.patience = m_patience->value();
    cfg.warmup = m_warmup->value();
    cfg.mixup = m_mixup->value();
    cfg.hsvH = m_hsvH->value();
    cfg.hsvS = m_hsvS->value();
    cfg.hsvV = m_hsvV->value();
    cfg.translate = m_translate->value();
    cfg.scale = m_scale->value();
    cfg.shear = m_shear->value();
    cfg.flipud = m_flipud->value();
    cfg.copyPaste = m_copyPaste->value();
    cfg.lrf = m_lrf->value();
    cfg.momentum = m_momentum->value();
    cfg.weightDecay = m_weightDecay->value();
    cfg.resume = false;
    cfg.resumePath.clear();

    const QString configPath = m_project->rootDir() + "/train_config.json";
    QFile f(configPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "失败", "无法写入训练配置。");
        return;
    }
    f.write(QJsonDocument(cfg.toJson()).toJson(QJsonDocument::Indented));

    m_currentEpoch = 0;
    m_totalEpochs = cfg.epochs;
    m_progress->setValue(0);
    m_log->clear();
    m_curve->clear();
    m_tabs->setCurrentIndex(1); // 切到训练曲线
    appendLog("==== 开始训练 ====");
    appendLog("Python: " + m_runner->pythonPath());
    appendLog("Backend: " + m_runner->backendDir());
    appendLog("模型: " + cfg.model + "  epochs=" + QString::number(cfg.epochs)
              + "  device=" + cfg.device);

    m_btnTrain->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_btnExport->setEnabled(false);
    m_runner->run("train.py", QStringList() << "--config" << configPath);
}

void TrainingPage::onStopTrain()
{
    if (m_runner->isRunning()) {
        m_runner->stop();
        appendLog("已请求停止训练。");
    }
}

void TrainingPage::onExportOnnx()
{
    if (m_runner->isRunning())
        return;
    if (m_lastBestPt.isEmpty()) {
        QMessageBox::information(this, "提示", "请先训练，或点击「选择权重导出」。");
        return;
    }

    // 导出选项对话框（opset / 简化模型）
    QDialog dlg(this);
    dlg.setWindowTitle("导出 ONNX");
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();
    QComboBox *opset = new QComboBox(&dlg);
    opset->addItems({"12", "13", "17"});
    opset->setToolTip("OpenCV DNN 4.10 建议 12");
    QCheckBox *simplify = new QCheckBox("简化计算图（onnxslim，OpenCV 兼容性更好）", &dlg);
    simplify->setChecked(true);
    form->addRow("算子集 opset", opset);
    form->addRow("", simplify);
    lay->addWidget(new QLabel("将导出固定 640×640 输入的 ONNX（OpenCV DNN 推理用）。", &dlg));
    lay->addLayout(form);
    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;

    appendLog("==== 导出 ONNX ====");
    appendLog(QString("opset=%1  simplify=%2").arg(opset->currentText())
                  .arg(simplify->isChecked() ? "是" : "否"));
    const QString outPath = m_lastBestPt.left(m_lastBestPt.lastIndexOf('.')) + ".onnx";
    QStringList args;
    args << "--weights" << m_lastBestPt << "--out" << outPath
         << "--opset" << opset->currentText();
    if (simplify->isChecked())
        args << "--simplify";
    m_runner->run("export.py", args);
}

void TrainingPage::onPickWeights()
{
    const QString pt = QFileDialog::getOpenFileName(this, "选择 YOLO 权重", m_project->rootDir(),
                                                    "PyTorch 权重 (*.pt)");
    if (pt.isEmpty())
        return;
    m_lastBestPt = pt;
    m_btnExport->setEnabled(true);
    appendLog("已选择权重：" + pt);
    onExportOnnx();
}

// 断点续训：选择 last.pt，恢复优化器/超参/学习率进度继续训练
void TrainingPage::onResumeTrain()
{
    if (m_runner->isRunning())
        return;
    if (!m_project || !m_project->isValid()) {
        QMessageBox::warning(this, "提示", "请先在「数据标注」页打开项目。");
        return;
    }
    const QString pt = QFileDialog::getOpenFileName(this, "选择要续训的 last.pt",
                                                    m_project->rootDir() + "/runs",
                                                    "PyTorch 权重 (*.pt)");
    if (pt.isEmpty())
        return;

    TrainConfig cfg;
    cfg.model = m_model->currentText().trimmed();
    cfg.data = ensureDataYaml();
    cfg.project = m_project->rootDir();
    cfg.name = "train_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    cfg.epochs = m_epochs->value();
    cfg.batch = m_batch->value();
    cfg.imgsz = m_imgsz->value();
    cfg.device = m_device->currentText().trimmed();
    cfg.optimizer = m_optimizer->currentText();
    cfg.lr0 = m_lr0->value();
    cfg.workers = m_workers->value();
    cfg.amp = m_amp->isChecked();
    cfg.includeNegatives = m_negatives->isChecked();
    cfg.mosaic = m_mosaic->value();
    cfg.fliplr = m_fliplr->value();
    cfg.degrees = m_degrees->value();
    cfg.patience = m_patience->value();
    cfg.warmup = m_warmup->value();
    cfg.mixup = m_mixup->value();
    cfg.hsvH = m_hsvH->value();
    cfg.hsvS = m_hsvS->value();
    cfg.hsvV = m_hsvV->value();
    cfg.translate = m_translate->value();
    cfg.scale = m_scale->value();
    cfg.shear = m_shear->value();
    cfg.flipud = m_flipud->value();
    cfg.copyPaste = m_copyPaste->value();
    cfg.lrf = m_lrf->value();
    cfg.momentum = m_momentum->value();
    cfg.weightDecay = m_weightDecay->value();
    cfg.resume = true;
    cfg.resumePath = pt;

    const QString configPath = m_project->rootDir() + "/train_resume_config.json";
    QFile f(configPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "失败", "无法写入训练配置。");
        return;
    }
    f.write(QJsonDocument(cfg.toJson()).toJson(QJsonDocument::Indented));

    m_log->clear();
    m_curve->clear();
    m_tabs->setCurrentIndex(1);
    appendLog("==== 断点续训 ====");
    appendLog("权重: " + pt);
    appendLog("(将从上次训练中断的 epoch 继续)");
    m_btnTrain->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_btnExport->setEnabled(false);
    m_runner->run("train.py", QStringList() << "--config" << configPath);
}

// 环境检测：检查 Python / PyTorch / CUDA / ultralytics 是否就绪
void TrainingPage::onCheckEnv()
{
    if (m_runner->isRunning())
        return;
    appendLog("==== 环境检测 ====");
    m_runner->run("check_env.py", QStringList());
}

void TrainingPage::appendLog(const QString &line)
{
    m_log->appendPlainText(line);
}

void TrainingPage::onRunnerLog(const QString &line)
{
    appendLog(line);
}

void TrainingPage::onRunnerProgress(const QJsonObject &info)
{
    const int epoch = info.value("epoch").toInt();
    const int epochs = info.value("epochs").toInt(m_totalEpochs);
    m_currentEpoch = epoch;
    m_totalEpochs = epochs;
    if (epochs > 0)
        m_progress->setValue(epoch * 100 / epochs);

    // 曲线数据（后端在 progress JSON 中带出损失与 mAP）
    if (m_curve) {
        m_curve->addPoint(epoch,
                          info.value("box_loss").toDouble(),
                          info.value("cls_loss").toDouble(),
                          info.value("dfl_loss").toDouble(),
                          info.value("map50").toDouble(),
                          info.value("map5095").toDouble());
    }

    QString msg = QString("Epoch %1/%2").arg(epoch).arg(epochs);
    if (info.contains("map50"))
        msg += QString("  mAP50=%1").arg(info.value("map50").toDouble(), 0, 'f', 4);
    if (info.contains("map5095"))
        msg += QString("  mAP50-95=%1").arg(info.value("map5095").toDouble(), 0, 'f', 4);
    if (info.contains("cls_loss"))
        msg += QString("  cls_loss=%1").arg(info.value("cls_loss").toDouble(), 0, 'f', 3);
    appendLog(msg);
}

void TrainingPage::onRunnerResult(const QJsonObject &info)
{
    m_progress->setValue(100);
    const bool ok = info.value("status").toString() == "success";

    if (info.contains("best_pt")) {
        // ---- 训练结果 ----
        appendLog(ok ? "==== 训练完成 ====" : "==== 训练失败 ====");
        TrainRecord rec;
        rec.model = info.value("model").toString();
        rec.epochs = info.value("epochs").toInt();
        rec.precision = info.value("precision").toDouble();
        rec.recall = info.value("recall").toDouble();
        rec.map50 = info.value("map50").toDouble();
        rec.map5095 = info.value("map5095").toDouble();
        rec.duration = info.value("duration").toString();
        rec.resultModel = info.value("best_pt").toString();
        rec.status = ok ? "成功" : "失败";
        rec.createdAt = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

        m_lastBestPt = rec.resultModel;
        m_btnExport->setEnabled(!rec.resultModel.isEmpty());

        if (m_db) {
            if (!m_db->isOpen())
                m_db->open();
            m_db->addRecord(rec);
        }

        appendLog("best.pt: " + rec.resultModel);
        if (rec.map50 > 0)
            appendLog(QString("mAP50=%1  mAP50-95=%2  P=%3  R=%4")
                          .arg(rec.map50, 0, 'f', 4)
                          .arg(rec.map5095, 0, 'f', 4)
                          .arg(rec.precision, 0, 'f', 4)
                          .arg(rec.recall, 0, 'f', 4));
    } else if (info.contains("onnx")) {
        // ---- 导出结果 ----
        appendLog(ok ? "==== 导出完成 ====" : "==== 导出失败 ====");
        if (ok)
            appendLog("ONNX: " + info.value("onnx").toString());
        else
            appendLog(info.value("error").toString());
    } else if (info.contains("env")) {
        // ---- 环境检测结果 ----
        if (!ok) {
            appendLog("==== 环境检测失败 ====");
            appendLog(info.value("error").toString());
            return;
        }
        appendLog(QString("Python: %1").arg(info.value("python").toString()));
        appendLog(QString("PyTorch: %1").arg(info.value("torch").toString()));
        appendLog(QString("ultralytics: %1").arg(info.value("ultralytics").toString()));
        const bool cuda = info.value("cuda").toBool();
        appendLog(QString("CUDA: %1").arg(cuda ? "可用" : "不可用（将使用 CPU）"));
        if (cuda)
            appendLog("GPU: " + info.value("gpu").toString());
        appendLog("== 环境检测完成 ==");
    } else {
        // ---- 其他（验证等）----
        appendLog(ok ? "==== 评估完成 ====" : "==== 评估失败 ====");
        if (!ok && info.contains("error"))
            appendLog(info.value("error").toString());
    }
}

void TrainingPage::onRunnerFinished(int code, QProcess::ExitStatus status)
{
    m_btnTrain->setEnabled(true);
    m_btnStop->setEnabled(false);
    appendLog(QString("进程结束，退出码 %1 (%2)")
                  .arg(code)
                  .arg(status == QProcess::CrashExit ? "崩溃" : "正常"));
}
