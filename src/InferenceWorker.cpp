#include "InferenceWorker.h"

#include <opencv2/opencv.hpp>

#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <string>
#include <vector>

namespace {

struct DetBox
{
    int cls;
    float conf;
    cv::Rect box;             // 原图坐标
    cv::Rect box640;          // 640 letterbox 坐标（掩码裁剪用）
    std::vector<float> coeffs; // 分割掩码系数（仅分割模型）
};

struct DetectResult
{
    std::vector<DetBox> dets;
    cv::Mat protos;           // 掩码原型 [1,nm,160,160]（仅分割模型），否则为空
    float scale = 1.0f;
    int padX = 0;
    int padY = 0;
};

cv::Scalar clsColor(int id)
{
    static const cv::Scalar palette[] = {
        cv::Scalar(0, 0, 255), cv::Scalar(0, 255, 0), cv::Scalar(255, 0, 0),
        cv::Scalar(0, 255, 255), cv::Scalar(255, 255, 0), cv::Scalar(255, 0, 255),
        cv::Scalar(128, 0, 128), cv::Scalar(0, 128, 255),
    };
    return palette[id % 8];
}

cv::Mat letterbox(const cv::Mat &src, int size, float &scale, int &padX, int &padY)
{
    scale = std::min((float)size / src.cols, (float)size / src.rows);
    const int nw = (int)(src.cols * scale);
    const int nh = (int)(src.rows * scale);
    cv::Mat resized;
    cv::resize(src, resized, cv::Size(nw, nh));
    padX = (size - nw) / 2;
    padY = (size - nh) / 2;
    cv::Mat out(size, size, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(out(cv::Rect(padX, padY, nw, nh)));
    return out;
}

// 解析 ONNX 输出并做 NMS。兼容：
//   经典检测 [1, 4+nc, N]            （yolov8/v11/v12 等）
//   经典分割 [1, 4+nc+nm, N] + 原型   （yolov8-seg 等，第二个输出为 [1,nm,160,160]）
//   端到端   [1, K, 6(+nm)]           （YOLO26/RT-DETR 风格，含掩码时列数 6+nm）
DetectResult detect(cv::dnn::Net &net, const cv::Mat &img, float confTh, float nmsTh,
                    std::string *diag = nullptr)
{
    DetectResult res;
    const int size = 640;
    res.scale = std::min((float)size / img.cols, (float)size / img.rows);
    const int nw = (int)(img.cols * res.scale);
    const int nh = (int)(img.rows * res.scale);
    res.padX = (size - nw) / 2;
    res.padY = (size - nh) / 2;

    const cv::Mat lb = letterbox(img, size, res.scale, res.padX, res.padY);
    cv::Mat blob = cv::dnn::blobFromImage(lb, 1 / 255.0, cv::Size(size, size), cv::Scalar(), true, false);
    net.setInput(blob);
    std::vector<cv::Mat> outs;
    net.forward(outs, net.getUnconnectedOutLayersNames());

    if (outs.empty() || outs[0].dims != 3) {
        if (diag)
            *diag = "输出张量维度异常（期望 3 维 [1,C,N]）";
        return res;
    }

    const cv::Mat output = outs[0];
    const int d1 = output.size[1];
    const int d2 = output.size[2];

    // 分割模型：第二个输出为掩码原型，通道数即掩码系数数量 nm
    int nm = 0;
    if (outs.size() >= 2 && outs[1].dims == 4)
        nm = outs[1].size[1];

    // ---- 端到端（NMS-free）格式：[1, K, 6(+nm)]，每行 (x1,y1,x2,y2,conf,cls[,coeffs]) ----
    if ((d2 == 6 || (nm > 0 && d2 == 6 + nm)) && d1 >= 6) {
        cv::Mat m = output.reshape(1, d1); // K x (6+nm)
        for (int i = 0; i < d1; ++i) {
            const float *r = m.ptr<float>(i);
            const float conf = r[4];
            if (conf < confTh || conf > 1.0f)
                continue;
            const int cls = (int)(r[5] + 0.5f);
            const float x1 = (r[0] - res.padX) / res.scale;
            const float y1 = (r[1] - res.padY) / res.scale;
            const float x2 = (r[2] - res.padX) / res.scale;
            const float y2 = (r[3] - res.padY) / res.scale;
            if (x2 <= x1 || y2 <= y1)
                continue;
            DetBox d;
            d.cls = cls;
            d.conf = conf;
            d.box = cv::Rect(cvRound(x1), cvRound(y1), cvRound(x2 - x1), cvRound(y2 - y1));
            d.box640 = cv::Rect(cvRound(r[0]), cvRound(r[1]), cvRound(r[2] - r[0]), cvRound(r[3] - r[1]));
            d.box640 &= cv::Rect(0, 0, size, size);
            if (nm > 0)
                d.coeffs.assign(r + 6, r + 6 + nm);
            res.dets.push_back(d);
        }
        if (nm > 0)
            res.protos = outs[1];
        return res; // 端到端输出已按置信度取 top-K，无需 NMS
    }

    // ---- 经典格式：[1, 4+nc+nm, N]（或转置 [1, N, 4+nc+nm]）----
    cv::Mat work;
    if (d1 < d2) {
        work = output.reshape(1, d1); // (C, N)
        cv::transpose(work, work);    // (N, C)
    } else {
        work = output.reshape(1, d1); // 已是 (N, C) 行优先
    }
    work = work.clone(); // 确保连续内存
    const int channels = work.cols;
    const int num = work.rows;
    const int nc = channels - 4 - nm;
    if (nc <= 0) {
        if (diag)
            *diag = "输出通道数异常（channels=" + std::to_string(channels) + "），请确认 ONNX 是目标检测/分割模型";
        return res;
    }
    const float *data = (const float *)work.data;

    for (int i = 0; i < num; ++i) {
        const float *row = data + (size_t)i * channels;
        const float cx = row[0], cy = row[1], w = row[2], h = row[3];
        float best = -1;
        int bestCls = -1;
        for (int c = 0; c < nc; ++c) {
            if (row[4 + c] > best) {
                best = row[4 + c];
                bestCls = c;
            }
        }
        if (best < confTh)
            continue;
        DetBox d;
        d.cls = bestCls;
        d.conf = best;
        const float x1 = (cx - w / 2 - res.padX) / res.scale;
        const float y1 = (cy - h / 2 - res.padY) / res.scale;
        const float x2 = (cx + w / 2 - res.padX) / res.scale;
        const float y2 = (cy + h / 2 - res.padY) / res.scale;
        d.box = cv::Rect(cvRound(x1), cvRound(y1), cvRound(x2 - x1), cvRound(y2 - y1));
        d.box640 = cv::Rect(cvRound(cx - w / 2), cvRound(cy - h / 2), cvRound(w), cvRound(h));
        d.box640 &= cv::Rect(0, 0, size, size);
        if (nm > 0)
            d.coeffs.assign(row + 4 + nc, row + 4 + nc + nm);
        res.dets.push_back(d);
    }

    std::vector<cv::Rect> boxes;
    std::vector<float> confs;
    for (const DetBox &d : res.dets) {
        boxes.push_back(d.box);
        confs.push_back(d.conf);
    }
    std::vector<int> idx;
    cv::dnn::NMSBoxes(boxes, confs, confTh, nmsTh, idx);
    std::vector<DetBox> kept;
    for (int i : idx)
        kept.push_back(res.dets[i]);
    res.dets = std::move(kept);
    if (nm > 0)
        res.protos = outs[1];
    return res;
}

// 计算单个目标的掩码轮廓（原图坐标，已按原图尺寸裁剪）。
// 与 ultralytics process_mask(upsample=True) 一致：
//   mask = (coeffs @ protos) -> 160x160 -> 双线性放大到 640 -> logits>0 二值化 -> 按框裁剪
std::vector<std::vector<cv::Point>> maskContours(const DetBox &d, const cv::Mat &protos,
                                                 float scale, int padX, int padY,
                                                 const cv::Size &imgSize)
{
    std::vector<std::vector<cv::Point>> contours;
    const int nm = (int)d.coeffs.size();
    if (nm <= 0 || d.box640.width <= 0 || d.box640.height <= 0 || protos.empty())
        return contours;

    cv::Mat pm = protos.reshape(1, nm);              // nm x 25600
    cv::Mat coeffsRow(1, nm, CV_32F, (void *)d.coeffs.data());
    cv::Mat logits1 = coeffsRow * pm;                // 1 x 25600
    cv::Mat logits160 = logits1.reshape(1, 160);     // 160 x 160（logits）
    cv::Mat up640;
    cv::resize(logits160, up640, cv::Size(640, 640), 0, 0, cv::INTER_LINEAR);
    cv::Mat bin640;
    cv::threshold(up640, bin640, 0.0, 1.0, cv::THRESH_BINARY); // logits > 0
    bin640.convertTo(bin640, CV_8UC1);
    cv::Mat maskBox = bin640(d.box640).clone();

    // 640 空间 -> 原图坐标
    const int ox1 = cvRound((d.box640.x - padX) / scale);
    const int oy1 = cvRound((d.box640.y - padY) / scale);
    const int ox2 = cvRound((d.box640.x + d.box640.width - padX) / scale);
    const int oy2 = cvRound((d.box640.y + d.box640.height - padY) / scale);
    cv::Rect oBox(ox1, oy1, std::max(1, ox2 - ox1), std::max(1, oy2 - oy1));
    oBox &= cv::Rect(0, 0, imgSize.width, imgSize.height);
    if (oBox.width <= 0 || oBox.height <= 0)
        return contours;

    cv::Mat binOrig;
    cv::resize(maskBox, binOrig, oBox.size(), 0, 0, cv::INTER_NEAREST);
    binOrig.convertTo(binOrig, CV_8UC1);

    cv::findContours(binOrig, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    for (auto &c : contours)
        for (auto &pt : c) {
            pt.x += oBox.x;
            pt.y += oBox.y;
        }
    return contours;
}

// 在图上绘制单个目标的掩码轮廓（半透明填充 + 描边）
void drawMask(cv::Mat &img, const DetBox &d, const cv::Mat &protos, float scale, int padX, int padY)
{
    const auto contours = maskContours(d, protos, scale, padX, padY, cv::Size(img.cols, img.rows));
    if (contours.empty())
        return;
    const cv::Scalar color = clsColor(d.cls);
    cv::Mat overlay;
    img.copyTo(overlay);
    cv::fillPoly(overlay, contours, color);
    cv::addWeighted(overlay, 0.35, img, 0.65, 0, img);
    cv::polylines(img, contours, true, color, 2); // 轮廓描边加粗
}

// 绘制检测结果。掩码轮廓用 OpenCV 绘制，检测框与中文标签用 QPainter（支持中文）。
cv::Mat drawDet(const cv::Mat &img, const DetectResult &res, const QStringList &classes)
{
    cv::Mat out = img.clone();

    // 1) 分割掩码轮廓
    if (!res.protos.empty()) {
        for (const DetBox &d : res.dets) {
            if (!d.coeffs.empty())
                drawMask(out, d, res.protos, res.scale, res.padX, res.padY);
        }
    }

    // 2) 检测框 + 中文标签
    if (res.dets.empty())
        return out;

    cv::Mat rgb;
    cv::cvtColor(out, rgb, cv::COLOR_BGR2RGB);
    QImage qi(rgb.data, rgb.cols, rgb.rows, (int)rgb.step, QImage::Format_RGB888);

    QPainter p(&qi);
    p.setRenderHint(QPainter::Antialiasing, true);
    QFont f = p.font();
    f.setPixelSize(18);
    f.setBold(true);
    p.setFont(f);

    for (const DetBox &d : res.dets) {
        const cv::Scalar c = clsColor(d.cls);
        const QColor color((int)c[2], (int)c[1], (int)c[0]); // BGR -> RGB

        p.setPen(QPen(color, 3)); // 检测框加粗
        p.drawRect(d.box.x, d.box.y, d.box.width, d.box.height);

        QString label = (d.cls >= 0 && d.cls < classes.size()) ? classes[d.cls]
                                                               : QString::number(d.cls);
        label += QString(" %1").arg(d.conf, 0, 'f', 2);

        const QRect tr = p.fontMetrics().boundingRect(label);
        const int ty = std::max(0, d.box.y - tr.height() - 4);
        const QRect bg(d.box.x, ty, tr.width() + 8, tr.height() + 6);
        p.fillRect(bg, QColor(0, 0, 0, 150));
        p.setPen(Qt::white);
        p.drawText(bg.adjusted(4, 3, -4, -3), Qt::AlignLeft | Qt::AlignVCenter, label);
    }
    p.end();

    cv::cvtColor(rgb, out, cv::COLOR_RGB2BGR);
    return out;
}

// BGR cv::Mat -> QImage(BGR888)，必须拷贝像素数据
QImage cvMatToQImage(const cv::Mat &m)
{
    if (m.empty())
        return QImage();
    QImage img(m.data, m.cols, m.rows, (int)m.step, QImage::Format_BGR888);
    return img.copy();
}

// 检测结果 -> JSON 数组（供界面结果表格显示）
QJsonArray detsToJson(const std::vector<DetBox> &dets)
{
    QJsonArray arr;
    for (const DetBox &d : dets) {
        QJsonObject o;
        o["cls"] = d.cls;
        o["conf"] = double(d.conf);
        o["x"] = d.box.x;
        o["y"] = d.box.y;
        o["w"] = d.box.width;
        o["h"] = d.box.height;
        arr.append(o);
    }
    return arr;
}

// QImage(BGR888) -> BGR cv::Mat，必须拷贝像素数据
cv::Mat qImageToCvMat(const QImage &img)
{
    if (img.isNull())
        return cv::Mat();
    cv::Mat m(img.height(), img.width(), CV_8UC3, (void *)img.constBits(), (size_t)img.bytesPerLine());
    return m.clone();
}

} // namespace

InferenceWorker::InferenceWorker(QObject *parent)
    : QObject(parent)
{
}

void InferenceWorker::loadModel(const QString &onnxPath)
{
    m_modelReady = false;
    try {
        cv::dnn::Net net = cv::dnn::readNetFromONNX(onnxPath.toLocal8Bit().constData());
        if (net.empty()) {
            emit modelLoaded(onnxPath, false, "ONNX 模型加载失败，请检查文件路径与文件是否有效。");
            return;
        }
        m_net = net;
        m_modelPath = onnxPath;
        m_modelReady = true;
        emit modelLoaded(onnxPath, true, QString());
    } catch (const cv::Exception &e) {
        emit modelLoaded(onnxPath, false, QString("ONNX 模型加载异常：%1").arg(QString::fromUtf8(e.what())));
    } catch (const std::exception &e) {
        emit modelLoaded(onnxPath, false, QString("模型加载异常：%1").arg(QString::fromUtf8(e.what())));
    }
}

void InferenceWorker::inferImage(const QString &imgPath, float confTh, const QStringList &classes)
{
    if (!m_modelReady) {
        emit logLine("模型未就绪，无法推理。");
        emit imageDone(QFileInfo(imgPath).fileName(), QImage(), 0, "模型未加载", QJsonArray());
        return;
    }
    const cv::Mat img = cv::imread(imgPath.toLocal8Bit().constData());
    if (img.empty()) {
        emit logLine("图片读取失败：" + imgPath);
        emit imageDone(QFileInfo(imgPath).fileName(), QImage(), 0, "图片读取失败", QJsonArray());
        return;
    }
    std::string diag;
    const auto res = detect(m_net, img, confTh, 0.45f, &diag);
    const cv::Mat out = drawDet(img, res, classes);
    if (!res.protos.empty()) {
        if (!diag.empty())
            diag += " ";
        diag += "分割模型：已输出缺陷轮廓";
    }
    emit imageDone(QFileInfo(imgPath).fileName(), cvMatToQImage(out), (int)res.dets.size(),
                   QString::fromStdString(diag), detsToJson(res.dets));
}

void InferenceWorker::inferFolder(const QString &dir, const QString &outDir, float confTh,
                                  const QStringList &classes)
{
    if (!m_modelReady) {
        emit logLine("模型未就绪，无法推理。");
        emit folderDone(0, 0, outDir, "模型未加载");
        return;
    }
    m_abort.store(false);
    QDir().mkpath(outDir);
    const QDir d(dir);
    const QStringList filters = {"*.jpg", "*.jpeg", "*.png", "*.bmp", "*.webp"};
    const QFileInfoList files = d.entryInfoList(filters, QDir::Files, QDir::Name);
    const int total = files.size();
    int processed = 0;
    std::string diag;
    bool firstSent = false;
    bool isSeg = false;
    for (const QFileInfo &fi : files) {
        if (m_abort.load())
            break;
        cv::Mat img = cv::imread(fi.absoluteFilePath().toLocal8Bit().constData());
        if (img.empty())
            continue;
        const auto res = detect(m_net, img, confTh, 0.45f, &diag);
        if (!res.protos.empty())
            isSeg = true;
        const cv::Mat out = drawDet(img, res, classes);
        cv::imwrite((outDir + "/" + fi.fileName()).toLocal8Bit().constData(), out);
        ++processed;
        emit folderProgress(processed, total);
        if (!firstSent) {
            firstSent = true;
            emit imageDone(QString(), cvMatToQImage(out), (int)res.dets.size(), QString(),
                           detsToJson(res.dets));
        }
    }
    if (isSeg) {
        if (!diag.empty())
            diag += " ";
        diag += "分割模型：已输出缺陷轮廓";
    }
    emit folderDone(processed, total, outDir, QString::fromStdString(diag));
}

void InferenceWorker::inferFrame(const QImage &frame, float confTh, const QStringList &classes)
{
    if (!m_modelReady || frame.isNull()) {
        emit frameDone(QImage(), 0);
        return;
    }
    const cv::Mat bgr = qImageToCvMat(frame);
    const auto res = detect(m_net, bgr, confTh, 0.45f);
    const cv::Mat out = drawDet(bgr, res, classes);
    emit frameDone(cvMatToQImage(out), (int)res.dets.size());
}

// 摄像头：采集与推理都在工作线程执行，界面线程只接收 frameDone 回调，不阻塞
void InferenceWorker::startCamera(float confTh, const QStringList &classes)
{
    if (m_cameraOn)
        return;
    if (!m_modelReady) {
        emit cameraOpened(false, "模型未加载，无法打开摄像头。");
        return;
    }
    m_cap.open(0);
    if (!m_cap.isOpened()) {
        emit cameraOpened(false, "无法打开摄像头（可能被占用或没有摄像头设备）。");
        return;
    }
    m_cameraOn = true;
    m_camConf = confTh;
    m_camClasses = classes;
    m_camTimer = new QTimer();
    m_camTimer->setInterval(30); // ~33fps 采集；检测耗时较长时实际帧率自动降低
    connect(m_camTimer, &QTimer::timeout, this, [this]() { cameraTick(); });
    m_camTimer->start();
    emit cameraOpened(true, QString());
}

void InferenceWorker::stopCamera()
{
    if (!m_cameraOn && !m_camTimer)
        return;
    m_cameraOn = false;
    if (m_camTimer) {
        m_camTimer->stop();
        delete m_camTimer;
        m_camTimer = nullptr;
    }
    if (m_cap.isOpened())
        m_cap.release();
    emit cameraClosed();
}

void InferenceWorker::cameraTick()
{
    if (!m_cameraOn)
        return;
    cv::Mat frame;
    if (!m_cap.read(frame) || frame.empty())
        return;
    const auto res = detect(m_net, frame, m_camConf, 0.45f);
    const cv::Mat out = drawDet(frame, res, m_camClasses);
    emit frameDone(cvMatToQImage(out), (int)res.dets.size());
}

// AI 辅助预标注：告一段落推理并转成 YOLO 标注
//   检测模型 -> 矩形框；分割模型 -> 掩码轮廓多边形（approxPolyDP 简化）
void InferenceWorker::prelabelImage(const QString &imgPath, float confTh)
{
    if (!m_modelReady) {
        emit prelabelDone(Annotation(), "模型未加载");
        return;
    }
    const cv::Mat img = cv::imread(imgPath.toLocal8Bit().constData());
    if (img.empty()) {
        emit prelabelDone(Annotation(), "图片读取失败：" + imgPath);
        return;
    }

    const auto res = detect(m_net, img, confTh, 0.45f);
    Annotation labels;
    if (res.protos.empty()) {
        for (const DetBox &d : res.dets) {
            labels.boxes.append(LabelBox::fromPixelRect(
                d.cls, QRectF(d.box.x, d.box.y, d.box.width, d.box.height),
                img.cols, img.rows));
        }
    } else {
        for (const DetBox &d : res.dets) {
            for (const auto &c : maskContours(d, res.protos, res.scale, res.padX, res.padY,
                                              cv::Size(img.cols, img.rows))) {
                std::vector<cv::Point> approx;
                cv::approxPolyDP(c, approx, 2.0, true);
                if (approx.size() < 3)
                    continue;
                QPolygonF qpoly;
                for (const auto &pt : approx)
                    qpoly << QPointF(pt.x, pt.y);
                labels.polygons.append(LabelPolygon::fromPixelPoly(d.cls, qpoly, img.cols, img.rows));
            }
        }
    }
    emit prelabelDone(labels, QString());
}

void InferenceWorker::abort()
{
    m_abort.store(true);
}
