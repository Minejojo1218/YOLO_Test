#pragma once

#include <QJsonObject>
#include <QProcess>
#include <QWidget>

class ProjectManager;
class Database;
class PythonRunner;
class TrainingCurveWidget;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTabWidget;

// 模型训练 + 导出 ONNX 页
class TrainingPage : public QWidget
{
    Q_OBJECT
public:
    explicit TrainingPage(ProjectManager *project, Database *db, QWidget *parent = nullptr);

    void refresh();

private slots:
    void onStartTrain();
    void onStopTrain();
    void onExportOnnx();
    void onPickWeights();
    void onResumeTrain();
    void onCheckEnv();
    void onRunnerLog(const QString &line);
    void onRunnerProgress(const QJsonObject &info);
    void onRunnerResult(const QJsonObject &info);
    void onRunnerFinished(int code, QProcess::ExitStatus status);

private:
    void appendLog(const QString &line);
    QString ensureDataYaml();

    ProjectManager *m_project;
    Database *m_db;
    PythonRunner *m_runner;

    QComboBox *m_model = nullptr;
    QSpinBox *m_epochs = nullptr;
    QSpinBox *m_batch = nullptr;
    QSpinBox *m_imgsz = nullptr;
    QComboBox *m_device = nullptr;
    QComboBox *m_optimizer = nullptr;
    QDoubleSpinBox *m_lr0 = nullptr;
    QSpinBox *m_workers = nullptr;
    QCheckBox *m_amp = nullptr;
    QCheckBox *m_negatives = nullptr;
    QDoubleSpinBox *m_mosaic = nullptr;
    QDoubleSpinBox *m_fliplr = nullptr;
    QDoubleSpinBox *m_degrees = nullptr;
    QSpinBox *m_patience = nullptr;
    QDoubleSpinBox *m_warmup = nullptr;
    QDoubleSpinBox *m_mixup = nullptr;
    // 颜色 / 几何增强 + 优化器细节
    QDoubleSpinBox *m_hsvH = nullptr;
    QDoubleSpinBox *m_hsvS = nullptr;
    QDoubleSpinBox *m_hsvV = nullptr;
    QDoubleSpinBox *m_translate = nullptr;
    QDoubleSpinBox *m_scale = nullptr;
    QDoubleSpinBox *m_shear = nullptr;
    QDoubleSpinBox *m_flipud = nullptr;
    QDoubleSpinBox *m_copyPaste = nullptr;
    QDoubleSpinBox *m_lrf = nullptr;
    QDoubleSpinBox *m_momentum = nullptr;
    QDoubleSpinBox *m_weightDecay = nullptr;

    QPushButton *m_btnTrain = nullptr;
    QPushButton *m_btnStop = nullptr;
    QPushButton *m_btnExport = nullptr;
    QPushButton *m_btnPick = nullptr;
    QPushButton *m_btnResume = nullptr;
    QPushButton *m_btnEnv = nullptr;
    QPushButton *m_btnLog = nullptr;
    QProgressBar *m_progress = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QTabWidget *m_tabs = nullptr;
    TrainingCurveWidget *m_curve = nullptr;

    QString m_lastBestPt;   // 最近一次训练得到的 best.pt
    int m_currentEpoch = 0;
    int m_totalEpochs = 0;
};
