# -*- coding: utf-8 -*-
"""导出 YOLO 权重为 ONNX（动态 shape，供 C#/C++ 复用）。"""
import argparse
import os
import sys
import traceback

from yolo_common import emit, log

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--weights", required=True, help="best.pt 路径")
    parser.add_argument("--out", required=True, help="输出 onnx 路径")
    parser.add_argument("--opset", type=int, default=12)
    parser.add_argument("--simplify", action="store_true", help="用 onnxslim 简化模型计算图")
    args = parser.parse_args()

    try:
        from ultralytics import YOLO
    except Exception as e:
        emit({"type": "result", "status": "error", "error": f"导入 ultralytics 失败: {e}"})
        sys.exit(1)

    try:
        model = YOLO(args.weights)
        # 1) 用固定 shape 导出（dynamic=False），OpenCV DNN 才能正确解析 YOLOv8/v11 的 Concat 层。
        # 2) 端到端模型（YOLO26/RT-DETR 风格）默认导出的是含 TopK/Gather 的 [1,K,6] 图：
        #    OpenCV DNN 4.10 无法解析 TopK 节点，且输出格式与本程序 C++ 解析器
        #    （经典 [1,4+nc,N]）不兼容，会导致 GUI 推理完全看不到框。
        #    因此导出前关闭 end2end 分支，恢复为经典 NMS 前格式。
        if getattr(getattr(model, "model", None), "end2end", False):
            model.model.end2end = False
            log("检测到 end2end 模型（YOLO26/RT-DETR 风格）：已切换为经典 NMS 格式导出，以兼容 OpenCV DNN 推理。")
        exported = str(model.export(format="onnx", dynamic=False, imgsz=640,
                                    opset=args.opset, simplify=bool(args.simplify)))
        if exported:
            # 若目标路径与默认导出路径不同，再移动；否则直接用默认导出结果
            src = os.path.normpath(os.path.abspath(exported))
            target = os.path.normpath(os.path.abspath(args.out))
            if src != target:
                os.makedirs(os.path.dirname(target), exist_ok=True)
                if os.path.exists(target):
                    os.remove(target)
                os.replace(src, target)
                exported = target
            emit({"type": "result", "status": "success", "onnx": exported})
        else:
            emit({"type": "result", "status": "error", "error": "导出返回空路径"})
    except Exception:
        emit({"type": "result", "status": "error", "error": traceback.format_exc()})
        sys.exit(1)


if __name__ == "__main__":
    main()
