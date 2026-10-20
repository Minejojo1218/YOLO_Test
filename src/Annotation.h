#pragma once

#include <QList>
#include <QMetaType>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>

// 单个标注框，采用 YOLO 归一化坐标（中心点 + 宽高，均为 0~1 的小数）
struct LabelBox
{
    int   classId = 0;
    float x = 0.0f;   // 中心点 x
    float y = 0.0f;   // 中心点 y
    float w = 0.0f;   // 宽度
    float h = 0.0f;   // 高度

    // 转为像素坐标矩形（左上角 + 宽高）
    QRectF toPixelRect(int imgW, int imgH) const
    {
        const float px = x * imgW;
        const float py = y * imgH;
        const float pw = w * imgW;
        const float ph = h * imgH;
        return QRectF(px - pw / 2.0f, py - ph / 2.0f, pw, ph);
    }

    // 从像素坐标矩形构建归一化框
    static LabelBox fromPixelRect(int classId, const QRectF &r, int imgW, int imgH)
    {
        LabelBox b;
        b.classId = classId;
        const float cx = (float)(r.x() + r.width()  / 2.0);
        const float cy = (float)(r.y() + r.height() / 2.0);
        b.x = cx / imgW;
        b.y = cy / imgH;
        b.w = (float)r.width()  / imgW;
        b.h = (float)r.height() / imgH;
        return b;
    }
};

// 单个多边形标注（YOLO-seg 归一化：class_id x1 y1 x2 y2 ... xn yn，坐标 0~1）
struct LabelPolygon
{
    int classId = 0;
    QVector<QPointF> points;   // 归一化顶点，按顺序连接

    // 转为像素坐标多边形（钳制在图像范围内）
    QPolygonF toPixelPoly(int imgW, int imgH) const
    {
        QPolygonF p;
        for (const QPointF &pt : points)
            p << QPointF(qBound(0.0, pt.x() * imgW, (double)imgW),
                         qBound(0.0, pt.y() * imgH, (double)imgH));
        return p;
    }

    // 从像素坐标多边形构建归一化多边形
    static LabelPolygon fromPixelPoly(int classId, const QPolygonF &p, int imgW, int imgH)
    {
        LabelPolygon poly;
        poly.classId = classId;
        for (const QPointF &pt : p)
            poly.points << QPointF(qBound(0.0, pt.x() / imgW, 1.0),
                                   qBound(0.0, pt.y() / imgH, 1.0));
        return poly;
    }
};

// 一张图上的全部标注（检测框 + 多边形）
struct Annotation
{
    QList<LabelBox> boxes;
    QList<LabelPolygon> polygons;

    bool isEmpty() const { return boxes.isEmpty() && polygons.isEmpty(); }
};

// 允许 Annotation 跨线程通过信号槽传递（AI 预标注在工作线程完成）
Q_DECLARE_METATYPE(Annotation)
