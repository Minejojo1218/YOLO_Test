# -*- coding: utf-8 -*-
"""环境检测：检查 Python / PyTorch / CUDA / ultralytics 是否就绪。"""
import sys

from yolo_common import emit


def main():
    info = {
        "type": "result",
        "status": "success",
        "env": True,
        "python": sys.version.split()[0],
        "torch": "-",
        "cuda": False,
        "gpu": "-",
        "ultralytics": "-",
    }

    try:
        import torch

        info["torch"] = torch.__version__
        info["cuda"] = bool(torch.cuda.is_available())
        if info["cuda"]:
            info["gpu"] = torch.cuda.get_device_name(0)
    except Exception as e:
        info["status"] = "error"
        info["error"] = f"PyTorch 不可用: {e}（请先运行 scripts/setup_env.bat）"
        emit(info)
        return

    try:
        import ultralytics

        info["ultralytics"] = ultralytics.__version__
    except Exception as e:
        info["status"] = "error"
        info["error"] = f"ultralytics 不可用: {e}（请先运行 scripts/setup_env.bat）"
        emit(info)
        return

    emit(info)


if __name__ == "__main__":
    main()
