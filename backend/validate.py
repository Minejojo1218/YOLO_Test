# -*- coding: utf-8 -*-
"""验证集评估：输出 mAP / precision / recall / F1 / 推理速度 / 模型大小 + 评估图表目录。"""
import argparse
import glob
import os
import sys
import traceback

from yolo_common import emit

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--weights", required=True)
    parser.add_argument("--data", required=True)
    args = parser.parse_args()

    try:
        from ultralytics import YOLO
    except Exception as e:
        emit({"type": "result", "status": "error", "error": f"导入 ultralytics 失败: {e}"})
        sys.exit(1)

    try:
        model = YOLO(args.weights)
        # 在权重同级目录生成评估图表（混淆矩阵 / PR / F1 曲线等）
        base = os.path.dirname(os.path.abspath(args.weights))
        m = model.val(data=args.data, verbose=False, plots=True,
                      project=base, name="val_plots")
        d = getattr(m, "results_dict", None) or {}
        p = float(d.get("metrics/precision(B)", 0.0))
        r = float(d.get("metrics/recall(B)", 0.0))
        f1 = (2.0 * p * r / (p + r)) if (p + r) > 0 else 0.0
        speed = getattr(m, "speed", None) or {}
        try:
            inference_ms = float(speed.get("inference", 0.0))
        except Exception:
            inference_ms = 0.0
        model_mb = 0.0
        try:
            if os.path.exists(args.weights):
                model_mb = os.path.getsize(args.weights) / (1024.0 * 1024.0)
        except Exception:
            pass
        # 找到本次生成的图表目录（ultralytics 可能自动加序号）
        plots_dir = ""
        try:
            cands = sorted(glob.glob(os.path.join(base, "val_plots*")), key=os.path.getmtime)
            if cands:
                plots_dir = cands[-1]
        except Exception:
            pass
        emit({
            "type": "result",
            "status": "success",
            "map50": float(d.get("metrics/mAP50(B)", 0.0)),
            "map5095": float(d.get("metrics/mAP50-95(B)", 0.0)),
            "precision": p,
            "recall": r,
            "f1": f1,
            "speed_ms": inference_ms,
            "model_mb": model_mb,
            "save_dir": plots_dir,
        })
    except Exception:
        emit({"type": "result", "status": "error", "error": traceback.format_exc()})
        sys.exit(1)


if __name__ == "__main__":
    main()
