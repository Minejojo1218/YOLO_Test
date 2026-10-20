#pragma once

#include <QJsonObject>
#include <QString>

// 训练参数结构体，与 backend/train.py 的配置 JSON 一一对应
struct TrainConfig
{
    QString model = "yolov8n.pt";   // 预训练模型
    QString data;                   // data.yaml 绝对路径
    QString project;                // 项目目录（存放训练结果 runs/）
    QString name = "train";         // 本次训练运行名
    int     epochs = 100;
    int     batch = 16;
    int     imgsz = 640;
    QString device = "0";           // "cpu" 或 "0" / "0,1"（多 GPU）
    QString optimizer = "auto";     // auto / SGD / Adam / AdamW
    double  lr0 = 0.01;
    int     workers = 8;
    bool    resume = false;
    QString resumePath;             // 断点续训的 last.pt 路径（resume=true 时用）
    bool    amp = true;
    // 数据与增强
    bool    includeNegatives = false;   // 未标注图作为负样本参与训练
    double  mosaic = 1.0;               // mosaic 增强概率 (0~1)
    double  fliplr = 0.5;               // 水平翻转概率 (0~1)
    double  degrees = 0.0;              // 随机旋转角度 (0~180)
    int     patience = 50;              // 早停忍耐轮数（0=关闭）
    double  warmup = 3.0;               // warmup epoch 数
    double  mixup = 0.0;                // MixUp 增强概率 (0~1)
    // 颜色 / 几何增强（ultralytics 参数名）
    double  hsvH = 0.015;               // HSV 色调增强
    double  hsvS = 0.7;                 // HSV 饱和度增强
    double  hsvV = 0.4;                 // HSV 亮度增强
    double  translate = 0.1;            // 平移比例
    double  scale = 0.5;                // 缩放比例
    double  shear = 0.0;                // 剪切角度
    double  flipud = 0.0;               // 上下翻转概率
    double  copyPaste = 0.0;            // 复制粘贴增强概率（分割任务）
    // 优化器细节
    double  lrf = 0.01;                 // 最终学习率因子
    double  momentum = 0.937;           // SGD 动量
    double  weightDecay = 0.0005;       // 权重衰减

    QJsonObject toJson() const
    {
        QJsonObject o;
        o["model"] = model;
        o["data"] = data;
        o["project"] = project;
        o["name"] = name;
        o["epochs"] = epochs;
        o["batch"] = batch;
        o["imgsz"] = imgsz;
        o["device"] = device;
        o["optimizer"] = optimizer;
        o["lr0"] = lr0;
        o["workers"] = workers;
        o["resume"] = resume;
        o["resume_path"] = resumePath;
        o["amp"] = amp;
        o["include_negatives"] = includeNegatives;
        o["mosaic"] = mosaic;
        o["fliplr"] = fliplr;
        o["degrees"] = degrees;
        o["patience"] = patience;
        o["warmup"] = warmup;
        o["mixup"] = mixup;
        o["hsv_h"] = hsvH;
        o["hsv_s"] = hsvS;
        o["hsv_v"] = hsvV;
        o["translate"] = translate;
        o["scale"] = scale;
        o["shear"] = shear;
        o["flipud"] = flipud;
        o["copy_paste"] = copyPaste;
        o["lrf"] = lrf;
        o["momentum"] = momentum;
        o["weight_decay"] = weightDecay;
        return o;
    }

    static TrainConfig fromJson(const QJsonObject &o)
    {
        TrainConfig c;
        c.model = o.value("model").toString(c.model);
        c.data = o.value("data").toString();
        c.project = o.value("project").toString();
        c.name = o.value("name").toString(c.name);
        c.epochs = o.value("epochs").toInt(c.epochs);
        c.batch = o.value("batch").toInt(c.batch);
        c.imgsz = o.value("imgsz").toInt(c.imgsz);
        c.device = o.value("device").toString(c.device);
        c.optimizer = o.value("optimizer").toString(c.optimizer);
        c.lr0 = o.value("lr0").toDouble(c.lr0);
        c.workers = o.value("workers").toInt(c.workers);
        c.resume = o.value("resume").toBool(c.resume);
        c.resumePath = o.value("resume_path").toString();
        c.amp = o.value("amp").toBool(c.amp);
        c.includeNegatives = o.value("include_negatives").toBool(c.includeNegatives);
        c.mosaic = o.value("mosaic").toDouble(c.mosaic);
        c.fliplr = o.value("fliplr").toDouble(c.fliplr);
        c.degrees = o.value("degrees").toDouble(c.degrees);
        c.patience = o.value("patience").toInt(c.patience);
        c.warmup = o.value("warmup").toDouble(c.warmup);
        c.mixup = o.value("mixup").toDouble(c.mixup);
        c.hsvH = o.value("hsv_h").toDouble(c.hsvH);
        c.hsvS = o.value("hsv_s").toDouble(c.hsvS);
        c.hsvV = o.value("hsv_v").toDouble(c.hsvV);
        c.translate = o.value("translate").toDouble(c.translate);
        c.scale = o.value("scale").toDouble(c.scale);
        c.shear = o.value("shear").toDouble(c.shear);
        c.flipud = o.value("flipud").toDouble(c.flipud);
        c.copyPaste = o.value("copy_paste").toDouble(c.copyPaste);
        c.lrf = o.value("lrf").toDouble(c.lrf);
        c.momentum = o.value("momentum").toDouble(c.momentum);
        c.weightDecay = o.value("weight_decay").toDouble(c.weightDecay);
        return c;
    }
};
