#pragma once

#include "Annotation.h"

#include <QColor>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsView>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QSize>
#include <QStringList>

class QPainter;
class QStyleOptionGraphicsItem;

// 标注模式：框标注 / 多边形标注（点选闭合）/ 自由绘制（画笔随手描，松手自动简化成多边形）
enum class AnnoMode { Box, Polygon, Freehand };

// 单个标注框的图形项
class BoxItem : public QGraphicsRectItem
{
public:
    BoxItem(const QRectF &rect, int classId, const QString &name, const QColor &color);

    void setClassInfo(int classId, const QString &name, const QColor &color);
    int classId() const { return m_classId; }
    void setSelected(bool s);
    bool isSelected() const { return m_selected; }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    int m_classId;
    QString m_name;
    QColor m_color;
    bool m_selected = false;
};

// 单个多边形标注的图形项（分割掩码多边形）
class PolygonItem : public QGraphicsPolygonItem
{
public:
    PolygonItem(const QPolygonF &pts, int classId, const QString &name, const QColor &color);

    void setClassInfo(int classId, const QString &name, const QColor &color);
    int classId() const { return m_classId; }
    void setSelected(bool s);
    bool isSelected() const { return m_selected; }

    void setPixelPoints(const QPolygonF &pts);
    QPolygonF pixelPoints() const { return m_pts; }

    // 命中检测：返回离 scenePos 在 tol 像素内的顶点下标，否则 -1
    int vertexAt(const QPointF &scenePos, double tol) const;
    void moveVertex(int index, const QPointF &pos);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    int m_classId;
    QString m_name;
    QColor m_color;
    bool m_selected = false;
    QPolygonF m_pts;   // 像素坐标（场景坐标）
};

// 标注画布：框标注（拖框/移动/删除）+ 多边形标注（点选闭合/拖顶点/移动/删除）
class AnnotationView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit AnnotationView(QWidget *parent = nullptr);

    void setImage(const QImage &img);
    bool hasImage() const { return !m_imageSize.isNull(); }
    QSize imageSize() const { return m_imageSize; }

    void setClasses(const QStringList &classes) { m_classes = classes; }
    void setCurrentClassId(int id);
    int currentClassId() const { return m_currentClassId; }

    // 标注模式切换
    void setAnnotationMode(AnnoMode mode);
    AnnoMode annotationMode() const { return m_mode; }

    void setAnnotations(const Annotation &ann);
    Annotation annotations() const;
    void clearAnnotations();
    // AI 预标注：把新标注追加到当前图（自动记录撤销点）
    void appendAnnotations(const Annotation &ann);

    // 撤销 / 重做
    void undo();
    void redo();
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    void fitToImage();
    QColor classColor(int id) const;

signals:
    void annotationChanged();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    BoxItem *boxAt(const QPointF &scenePos) const;
    void selectBox(BoxItem *box);
    void addBox(const QRectF &pixelRect);
    void removeBox(BoxItem *box);
    void changeBoxClass(BoxItem *box, int classId);

    PolygonItem *polyAt(const QPointF &scenePos) const;
    void selectPoly(PolygonItem *poly);
    void addPolygon(const QPolygonF &pts);
    void removePolygon(PolygonItem *poly);
    void changePolyClass(PolygonItem *poly, int classId);
    void closePolygon();
    void cancelPolyDraw();
    void updatePolyRubber(const QPointF &cursor);

    // 自由绘制
    void startFreehand(const QPointF &scenePos);
    void updateFreehand(const QPointF &scenePos);
    void finishFreehand();
    void cancelFreehand();
    void updateFreehandRubber(const QPointF &cursor);
    QPolygonF simplifyFreehand(const QPolygonF &pts) const;

    // 撤销/重做
    void commitHistory();                       // 记录一次变更
    void rebuildItems(const Annotation &ann);   // 按标注重建图元（不改历史）

    QPointF clampToImage(const QPointF &p) const;

    QGraphicsScene *m_scene = nullptr;
    QGraphicsPixmapItem *m_pixmapItem = nullptr;
    QSize m_imageSize;
    QStringList m_classes;
    int m_currentClassId = 0;

    QList<BoxItem *> m_boxItems;
    QList<PolygonItem *> m_polyItems;

    // 绘制状态（框）
    bool m_drawing = false;
    QPointF m_drawStart;
    QGraphicsRectItem *m_rubber = nullptr;

    // 移动状态（框）
    bool m_moving = false;
    BoxItem *m_movingBox = nullptr;
    QPointF m_moveOffset;

    BoxItem *m_selected = nullptr;

    // 平移状态
    bool m_panning = false;
    QPoint m_panStart;

    // ---- 多边形/自由绘制模式 ----
    AnnoMode m_mode = AnnoMode::Box;
    PolygonItem *m_selectedPoly = nullptr;
    bool m_polyDrawing = false;
    QPolygonF m_pendingPts;                     // 场景（像素）坐标
    QGraphicsPathItem *m_polyRubber = nullptr;
    bool m_vertexDrag = false;
    PolygonItem *m_vertexPoly = nullptr;
    int m_vertexIndex = -1;
    bool m_polyMoving = false;
    PolygonItem *m_movingPoly = nullptr;
    QPointF m_polyMoveLast;

    // ---- 自由绘制状态 ----
    bool m_freehandDrawing = false;
    QPolygonF m_freehandPts;                    // 画笔轨迹（场景像素坐标）
    QGraphicsPathItem *m_freehandRubber = nullptr;

    // ---- 撤销 / 重做 ----
    QList<Annotation> m_undo;
    QList<Annotation> m_redo;
    Annotation m_committed;                     // 最近一次已提交状态
};
