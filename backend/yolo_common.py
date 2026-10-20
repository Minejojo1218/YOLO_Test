# -*- coding: utf-8 -*-
"""YoloStudio 后端公共工具：统一通过 stdout 输出 JSON 供 Qt 端解析。"""
import json
import sys


def emit(obj):
    """向 stdout 输出一行 JSON（供 Qt 端逐行解析）。"""
    sys.stdout.write(json.dumps(obj, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def log(line):
    """输出一行普通日志。"""
    sys.stdout.write(str(line) + "\n")
    sys.stdout.flush()


def resolve_device(device):
    """把界面传入的 device 归一化。"""
    d = (device or "").strip().lower()
    if d in ("", "gpu", "auto"):
        return "0"
    return device


def fmt_duration(seconds):
    m = int(seconds // 60)
    s = int(seconds % 60)
    return f"{m}m{s:02d}s"
