#pragma once

// 推理工作线程：ONNX 模型加载与所有推理（单张/文件夹/摄像头/AI 预标注）都在独立线程执行，
// 避免阻塞 GUI 界面。界面线程通过信号槽（队列连接）向本对象提交任务、接收结果。

#include "Annotation.h"

#include <QImage>
#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>

#include <opencv2/dnn.hpp>
#include <opencv2/videoio.hpp>

class QTimer;

class InferenceWorker : public QObject
{
    Q_OBJECT
public:
    explicit InferenceWorker(QObject *parent = nullptr);

public slots:
    // 在工作线程加载 ONNX 模型（成功/失败通过 modelLoaded 信号返回）
    void loadModel(const QString &onnxPath);
    // 单张图片推理
    void inferImage(const QString &imgPath, float confTh, const QStringList &classes);
    // 文件夹批量推理，结果图写入 outDir，进度用 folderProgress/folderDone 上报
    void inferFolder(const QString &dir, const QString &outDir, float confTh,
                     const QStringList &classes);
    // 摄像头单帧推理（GUI 每帧调用，结果用 frameDone 返回）
    void inferFrame(const QImage &frame, float confTh, const QStringList &classes);
    // 摄像头：在工作线程内采集+推理（回调 frameDone），不阻塞界面线程
    void startCamera(float confTh, const QStringList &classes);
    void stopCamera();
    // AI 辅助预标注：对图片推理并转换成 YOLO 标注（检测→框，分割→多边形轮廓）
    void prelabelImage(const QString &imgPath, float confTh);
    // 请求中止当前长任务（原子置位，文件夹循环会尽快退出）
    void abort();

signals:
    void logLine(const QString &line);
    void modelLoaded(const QString &path, bool ok, const QString &error);
    void imageDone(const QString &tag, const QImage &result, int count, const QString &diag,
                   const QJsonArray &dets);
    void folderProgress(int done, int total);
    void folderDone(int processed, int total, const QString &outDir, const QString &diag);
    void frameDone(const QImage &result, int count);
    void cameraOpened(bool ok, const QString &error);
    void cameraClosed();
    // AI 预标注结果（labels 为转换后的 YOLO 标注，error 为空表示成功）
    void prelabelDone(const Annotation &labels, const QString &error);

private:
    void cameraTick();

    cv::dnn::Net m_net;          // 仅在本线程使用，不跨线程
    bool m_modelReady = false;   // 模型是否已成功加载
    QString m_modelPath;
    std::atomic<bool> m_abort{false};

    // 摄像头（仅工作线程访问）
    cv::VideoCapture m_cap;
    QTimer *m_camTimer = nullptr;
    bool m_cameraOn = false;
    float m_camConf = 0.25f;
    QStringList m_camClasses;
};
