#pragma once

#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QWidget>

class ProjectManager;
class PythonRunner;
class InferenceWorker;
class QLabel;
class QLineEdit;
class QDoubleSpinBox;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QThread;

// 模型验证页：ONNX 单张/文件夹/摄像头推理（全部在工作线程执行，不卡界面）+ validate.py 指标
class ValidationPage : public QWidget
{
    Q_OBJECT
public:
    explicit ValidationPage(ProjectManager *project, QWidget *parent = nullptr);
    ~ValidationPage();

    void refresh();

private slots:
    void onBrowseModel();
    void onInferImage();
    void onInferFolder();
    void onToggleCamera();
    void onPrevResult();
    void onNextResult();
    void onValidateMetrics();
    void onRunnerLog(const QString &line);
    void onRunnerResult(const QJsonObject &info);
    void onRunnerFinished(int code, QProcess::ExitStatus status);

    // 推理工作线程回调
    void onWorkerModelLoaded(const QString &path, bool ok, const QString &error);
    void onWorkerImageDone(const QString &tag, const QImage &result, int count,
                           const QString &diag, const QJsonArray &dets);
    void onWorkerFolderProgress(int done, int total);
    void onWorkerFolderDone(int processed, int total, const QString &outDir,
                            const QString &diag);
    void onWorkerFrameDone(const QImage &result, int count);
    void onWorkerCameraOpened(bool ok, const QString &error);
    void onWorkerCameraClosed();

private:
    void appendLog(const QString &line);
    void showImage(const QImage &img);
    void fillDetTable(const QJsonArray &dets);
    void inferImagePath(const QString &path);
    QString findBestPt();
    bool ensureModelRequested();   // 确保模型已加载；未加载则异步加载并挂起当前任务
    void runPendingTask();         // 模型就绪后执行挂起的任务

    ProjectManager *m_project;
    PythonRunner *m_runner;

    QLineEdit *m_modelPath = nullptr;
    QDoubleSpinBox *m_conf = nullptr;
    QLabel *m_imageLabel = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QPushButton *m_btnCamera = nullptr;
    QPushButton *m_btnPlots = nullptr;
    QPushButton *m_btnLog = nullptr;
    QPushButton *m_btnPrev = nullptr;
    QPushButton *m_btnNext = nullptr;
    QTableWidget *m_detsTable = nullptr;
    QString m_lastPlotsDir;          // 最近一次评估图表目录

    // 文件夹推理结果导航
    QStringList m_folderFiles;
    int m_folderIndex = -1;

    // 推理工作线程：模型加载、推理、摄像头采集都在该线程执行
    QThread *m_workerThread = nullptr;
    InferenceWorker *m_worker = nullptr;
    bool m_modelReady = false;       // 工作线程侧模型是否已加载成功
    QString m_loadedModelPath;

    // 挂起的推理任务（模型加载完成后自动继续）
    enum class PendingTask { None, InferImage, InferFolder, OpenCamera };
    PendingTask m_pendingTask = PendingTask::None;
    QString m_pendingArg1;           // 图片路径 / 文件夹路径
    QString m_pendingArg2;           // 输出文件夹（文件夹推理）

    bool m_cameraOn = false;
    int m_lastProgressLogged = -1;   // 文件夹进度日志节流
};
