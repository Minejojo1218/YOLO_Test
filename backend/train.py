# -*- coding: utf-8 -*-
"""YOLO 训练脚本：读取配置 JSON，调用 ultralytics 训练，逐 epoch 输出进度 JSON。"""
import argparse
import json
import os
import sys
import time
import traceback

from yolo_common import emit, fmt_duration

# 确保能 import 到同目录的 yolo_common
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", required=True, help="训练配置 JSON 文件")
    args = parser.parse_args()

    try:
        with open(args.config, "r", encoding="utf-8") as f:
            cfg = json.load(f)
    except Exception as e:
        emit({"type": "result", "status": "error", "error": f"读取配置失败: {e}"})
        sys.exit(1)

    try:
        from ultralytics import YOLO
    except Exception as e:
        emit({"type": "result", "status": "error",
              "error": f"导入 ultralytics 失败，请先运行 scripts/setup_env.bat 配置环境: {e}"})
        sys.exit(1)

    model_name = cfg.get("model", "yolov8n.pt")
    epochs = int(cfg.get("epochs", 100))
    batch = int(cfg.get("batch", 16))
    imgsz = int(cfg.get("imgsz", 640))
    device = cfg.get("device", "0") or "0"
    optimizer = cfg.get("optimizer", "auto") or "auto"
    lr0 = float(cfg.get("lr0", 0.01))
    workers = int(cfg.get("workers", 8))
    amp = bool(cfg.get("amp", True))
    mosaic = float(cfg.get("mosaic", 1.0))
    fliplr = float(cfg.get("fliplr", 0.5))
    degrees = float(cfg.get("degrees", 0.0))
    patience = int(cfg.get("patience", 50))
    warmup = float(cfg.get("warmup", 3.0))
    mixup = float(cfg.get("mixup", 0.0))
    hsv_h = float(cfg.get("hsv_h", 0.015))
    hsv_s = float(cfg.get("hsv_s", 0.7))
    hsv_v = float(cfg.get("hsv_v", 0.4))
    translate = float(cfg.get("translate", 0.1))
    scale = float(cfg.get("scale", 0.5))
    shear = float(cfg.get("shear", 0.0))
    flipud = float(cfg.get("flipud", 0.0))
    copy_paste = float(cfg.get("copy_paste", 0.0))
    lrf = float(cfg.get("lrf", 0.01))
    momentum = float(cfg.get("momentum", 0.937))
    weight_decay = float(cfg.get("weight_decay", 0.0005))
    data = cfg.get("data", "")
    project = cfg.get("project", ".")
    name = cfg.get("name", "train")
    resume_path = (cfg.get("resume_path", "") or "").strip()
    resume = bool(cfg.get("resume", False)) or bool(resume_path)

    start = time.time()
    last = {}

    def on_epoch_end(trainer):
        m = getattr(trainer, "metrics", None) or {}
        last.update(m)
        epoch = getattr(trainer, "epoch", -1)
        total = int(getattr(trainer, "epochs", epochs))
        # 损失曲线：ultralytics 的 label_loss_items 把 tloss 拆成 box/cls/dfl/seg
        box_loss = cls_loss = dfl_loss = seg_loss = 0.0
        try:
            items = getattr(trainer, "label_loss_items", None)
            if items is not None and getattr(trainer, "tloss", None) is not None:
                losses = items(trainer.tloss, prefix="train")
                box_loss = float(losses.get("train/box_loss", 0.0))
                cls_loss = float(losses.get("train/cls_loss", 0.0))
                dfl_loss = float(losses.get("train/dfl_loss", 0.0))
                seg_loss = float(losses.get("train/seg_loss", 0.0))
        except Exception:
            pass
        emit({
            "type": "progress",
            "epoch": int(epoch) + 1,
            "epochs": total,
            "map50": float(m.get("metrics/mAP50(B)", 0.0)),
            "map5095": float(m.get("metrics/mAP50-95(B)", 0.0)),
            "precision": float(m.get("metrics/precision(B)", 0.0)),
            "recall": float(m.get("metrics/recall(B)", 0.0)),
            "box_loss": box_loss,
            "cls_loss": cls_loss,
            "dfl_loss": dfl_loss,
            "seg_loss": seg_loss,
        })

    try:
        if resume:
            # 断点续训：加载 last.pt 并恢复优化器/超参/学习率进度
            if not resume_path or not os.path.exists(resume_path):
                emit({"type": "result", "status": "error",
                      "error": f"断点续训失败：找不到权重文件 {resume_path}"})
                sys.exit(1)
            model = YOLO(resume_path)
            model.add_callback("on_train_epoch_end", on_epoch_end)
            results = model.train(resume=True)
        else:
            model = YOLO(model_name)
            model.add_callback("on_train_epoch_end", on_epoch_end)
            results = model.train(
                data=data,
                epochs=epochs,
                batch=batch,
                imgsz=imgsz,
                device=device,
                optimizer=optimizer,
                lr0=lr0,
                workers=workers,
                amp=amp,
                project=project,
                name=name,
                mosaic=mosaic,
                fliplr=fliplr,
                degrees=degrees,
                patience=patience,
                warmup_epochs=warmup,
                mixup=mixup,
                hsv_h=hsv_h,
                hsv_s=hsv_s,
                hsv_v=hsv_v,
                translate=translate,
                scale=scale,
                shear=shear,
                flipud=flipud,
                copy_paste=copy_paste,
                lrf=lrf,
                momentum=momentum,
                weight_decay=weight_decay,
                verbose=False,
                plots=True,
            )
    except Exception:
        emit({"type": "result", "status": "error", "error": traceback.format_exc()})
        sys.exit(1)

    duration = time.time() - start
    m = getattr(results, "metrics", None) or last
    save_dir = str(getattr(results, "save_dir", os.path.join(project, name)))
    best_pt = os.path.join(save_dir, "weights", "best.pt")

    emit({
        "type": "result",
        "status": "success",
        "model": model_name,
        "epochs": epochs,
        "map50": float(m.get("metrics/mAP50(B)", 0.0)),
        "map5095": float(m.get("metrics/mAP50-95(B)", 0.0)),
        "precision": float(m.get("metrics/precision(B)", 0.0)),
        "recall": float(m.get("metrics/recall(B)", 0.0)),
        "duration": fmt_duration(duration),
        "best_pt": best_pt,
    })


if __name__ == "__main__":
    main()
