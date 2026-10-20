#pragma once

#include "Annotation.h"

#include <QString>

// YOLO 标注文件读写
namespace LabelIO {

// 读取 YOLO 标注文件。
//   框格式（5 个数）：classId cx cy w h
//   多边形格式（奇数个 ≥7）：classId x1 y1 x2 y2 ... xn yn
// 文件不存在返回空列表。
Annotation readYoloTxt(const QString &path);

// 写入 YOLO 标注文件（框与多边形均按各自格式输出）。返回是否成功。
bool writeYoloTxt(const QString &path, const Annotation &ann);

} // namespace LabelIO
