#include "AnnotationView.h"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QLineF>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QWheelEvent>
#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>

// ---------- BoxItem ----------

BoxItem::BoxItem(const QRectF &rect, int classId, const QString &name, const QColor &color)
    : QGraphicsRectItem(rect)
    , m_classId(classId)
    , m_name(name)
    , m_color(color)
{
    setFlags(QGraphicsItem::ItemIsFocusable);
}

void BoxItem::setClassInfo(int classId, const QString &name, const QColor &color)
{
    m_classId = classId;
    m_name = name;
    m_color = color;
    update();
}

void BoxItem::setSelected(bool s)
{
    m_selected = s;
    update();
}

void BoxItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    QPen pen(m_color, m_selected ? 4.0 : 3.0);
    if (m_selected)
        pen.setStyle(Qt::DashLine);
    painter->setPen(pen);

    QColor fill = m_color;
    fill.setAlpha(m_selected ? 110 : 70);
    painter->setBrush(fill);
    painter->drawRect(rect());

    // 类别名标签
    painter->setPen(Qt::white);
    QFont f = painter->font();
    f.setPointSizeF(qMax(9.0, rect().height() * 0.08));
    painter->setFont(f);
    const QString text = QString("%1").arg(m_name.isEmpty() ? QString::number(m_classId) : m_name);
    painter->drawText(rect().topLeft() + QPointF(4, 16), text);
}

// ---------- PolygonItem ----------

PolygonItem::PolygonItem(const QPolygonF &pts, int classId, const QString &name, const QColor &color)
    : QGraphicsPolygonItem(pts)
    , m_classId(classId)
    , m_name(name)
    , m_color(color)
    , m_pts(pts)
{
    setFlags(QGraphicsItem::ItemIsFocusable);
}

void PolygonItem::setClassInfo(int classId, const QString &name, const QColor &color)
{
    m_classId = classId;
    m_name = name;
    m_color = color;
    update();
}

void PolygonItem::setSelected(bool s)
{
    m_selected = s;
    update();
}

void PolygonItem::setPixelPoints(const QPolygonF &pts)
{
    m_pts = pts;
    setPolygon(pts);
    update();
}

int PolygonItem::vertexAt(const QPointF &scenePos, double tol) const
{
    for (int i = 0; i < m_pts.size(); ++i) {
        if (QLineF(m_pts[i], scenePos).length() <= tol)
            return i;
    }
    return -1;
}

void PolygonItem::moveVertex(int index, const QPointF &pos)
{
    if (index < 0 || index >= m_pts.size())
        return;
    m_pts[index] = pos;
    setPolygon(m_pts);
    update();
}

void PolygonItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    QPen pen(m_color, m_selected ? 4.5 : 3.0);
    if (m_selected)
        pen.setStyle(Qt::DashLine);
    painter->setPen(pen);

    QColor fill = m_color;
    fill.setAlpha(m_selected ? 120 : 80);
    painter->setBrush(fill);
    painter->drawPolygon(m_pts);

    // 类别名标签
    painter->setPen(Qt::white);
    QFont f = painter->font();
    f.setPointSizeF(qMax(9.0, m_pts.boundingRect().height() * 0.06));
    painter->setFont(f);
    painter->drawText(m_pts.boundingRect().topLeft() + QPointF(4, 16),
                      m_name.isEmpty() ? QString::number(m_classId) : m_name);

    // 选中时绘制顶点手柄
    if (m_selected) {
        painter->setPen(QPen(m_color, 2.0));
        painter->setBrush(Qt::white);
        const double s = 5.0;
        for (const QPointF &v : m_pts)
            painter->drawRect(QRectF(v.x() - s, v.y() - s, s * 2, s * 2));
    }
}

// ---------- AnnotationView ----------

// 点到线段的距离（自由绘制轨迹简化的辅助函数）
static double ptSegDist(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 < 1e-9)
        return QLineF(p, a).length();
    double t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2;
    t = qBound(0.0, t, 1.0);
    return QLineF(p, a + ab * t).length();
}

// Douglas-Peucker 折线简化
static void dpReduce(const QVector<QPointF> &pts, int first, int last, double eps,
                     QVector<QPointF> &out)
{
    double maxD = 0;
    int idx = -1;
    for (int i = first + 1; i < last; ++i) {
        const double d = ptSegDist(pts[i], pts[first], pts[last]);
        if (d > maxD) {
            maxD = d;
            idx = i;
        }
    }
    if (maxD > eps && idx != -1) {
        dpReduce(pts, first, idx, eps, out);
        out << pts[idx];
        dpReduce(pts, idx, last, eps, out);
    }
}

AnnotationView::AnnotationView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing, false);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setDragMode(QGraphicsView::NoDrag);
    setMouseTracking(true);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void AnnotationView::setImage(const QImage &img)
{
    m_scene->clear();
    m_pixmapItem = nullptr;
    m_boxItems.clear();
    m_polyItems.clear();
    m_selected = nullptr;
    m_selectedPoly = nullptr;
    m_rubber = nullptr;
    m_polyRubber = nullptr;
    m_polyDrawing = false;
    m_pendingPts.clear();
    m_freehandRubber = nullptr; // m_scene->clear() 已删除该图元
    m_freehandDrawing = false;
    m_freehandPts.clear();
    m_undo.clear();
    m_redo.clear();
    m_committed = Annotation();

    m_imageSize = img.size();
    m_scene->setSceneRect(0, 0, m_imageSize.width(), m_imageSize.height());
    if (!img.isNull())
        m_pixmapItem = m_scene->addPixmap(QPixmap::fromImage(img));
    fitToImage();
}

void AnnotationView::setCurrentClassId(int id)
{
    if (id < 0)
        id = 0;
    m_currentClassId = id;
}

void AnnotationView::setAnnotationMode(AnnoMode mode)
{
    m_mode = mode;
    cancelPolyDraw();
    cancelFreehand();
}

void AnnotationView::rebuildItems(const Annotation &ann)
{
    for (BoxItem *b : m_boxItems)
        m_scene->removeItem(b);
    qDeleteAll(m_boxItems);
    m_boxItems.clear();
    for (PolygonItem *p : m_polyItems)
        m_scene->removeItem(p);
    qDeleteAll(m_polyItems);
    m_polyItems.clear();
    m_selected = nullptr;
    m_selectedPoly = nullptr;
    cancelPolyDraw();
    cancelFreehand();

    const int w = m_imageSize.width();
    const int h = m_imageSize.height();

    for (const LabelBox &b : ann.boxes) {
        const QRectF r = b.toPixelRect(w, h);
        const QString name = (b.classId >= 0 && b.classId < m_classes.size())
                                 ? m_classes[b.classId] : QString();
        BoxItem *item = new BoxItem(r, b.classId, name, classColor(b.classId));
        item->setZValue(10);
        m_scene->addItem(item);
        m_boxItems.append(item);
    }
    for (const LabelPolygon &p : ann.polygons) {
        const QPolygonF pts = p.toPixelPoly(w, h);
        const QString name = (p.classId >= 0 && p.classId < m_classes.size())
                                 ? m_classes[p.classId] : QString();
        PolygonItem *item = new PolygonItem(pts, p.classId, name, classColor(p.classId));
        item->setZValue(10);
        m_scene->addItem(item);
        m_polyItems.append(item);
    }
}

void AnnotationView::setAnnotations(const Annotation &ann)
{
    // 载入新图：重置撤销历史
    m_undo.clear();
    m_redo.clear();
    m_committed = ann;
    rebuildItems(ann);
}

void AnnotationView::commitHistory()
{
    m_undo.append(m_committed);
    m_committed = annotations();
    m_redo.clear();
    if (m_undo.size() > 100)
        m_undo.removeFirst();
}

void AnnotationView::undo()
{
    if (m_undo.isEmpty())
        return;
    m_redo.append(m_committed);
    const Annotation state = m_undo.takeLast();
    m_committed = state;
    rebuildItems(state);
    emit annotationChanged();
}

void AnnotationView::redo()
{
    if (m_redo.isEmpty())
        return;
    m_undo.append(m_committed);
    const Annotation state = m_redo.takeLast();
    m_committed = state;
    rebuildItems(state);
    emit annotationChanged();
}

void AnnotationView::appendAnnotations(const Annotation &ann)
{
    if (ann.isEmpty())
        return;
    const int w = m_imageSize.width();
    const int h = m_imageSize.height();
    for (const LabelBox &b : ann.boxes) {
        const QRectF r = b.toPixelRect(w, h);
        const QString name = (b.classId >= 0 && b.classId < m_classes.size())
                                 ? m_classes[b.classId] : QString();
        BoxItem *item = new BoxItem(r, b.classId, name, classColor(b.classId));
        item->setZValue(10);
        m_scene->addItem(item);
        m_boxItems.append(item);
    }
    for (const LabelPolygon &p : ann.polygons) {
        const QPolygonF pts = p.toPixelPoly(w, h);
        const QString name = (p.classId >= 0 && p.classId < m_classes.size())
                                 ? m_classes[p.classId] : QString();
        PolygonItem *item = new PolygonItem(pts, p.classId, name, classColor(p.classId));
        item->setZValue(10);
        m_scene->addItem(item);
        m_polyItems.append(item);
    }
    commitHistory();
    emit annotationChanged();
}

Annotation AnnotationView::annotations() const
{
    Annotation out;
    const int w = m_imageSize.width();
    const int h = m_imageSize.height();
    for (const BoxItem *b : m_boxItems) {
        out.boxes.append(LabelBox::fromPixelRect(b->classId(), b->rect(), w, h));
    }
    for (const PolygonItem *p : m_polyItems) {
        out.polygons.append(LabelPolygon::fromPixelPoly(p->classId(), p->pixelPoints(), w, h));
    }
    return out;
}

void AnnotationView::clearAnnotations()
{
    setAnnotations(Annotation());
}

void AnnotationView::fitToImage()
{
    if (m_pixmapItem)
        fitInView(m_pixmapItem, Qt::KeepAspectRatio);
}

QColor AnnotationView::classColor(int id) const
{
    return QColor::fromHsv((id * 137) % 360, 200, 255);
}

QPointF AnnotationView::clampToImage(const QPointF &p) const
{
    return QPointF(qBound(0.0, p.x(), (double)m_imageSize.width()),
                   qBound(0.0, p.y(), (double)m_imageSize.height()));
}

BoxItem *AnnotationView::boxAt(const QPointF &scenePos) const
{
    for (int i = m_boxItems.size() - 1; i >= 0; --i) {
        BoxItem *b = m_boxItems[i];
        if (b->sceneBoundingRect().contains(scenePos))
            return b;
    }
    return nullptr;
}

PolygonItem *AnnotationView::polyAt(const QPointF &scenePos) const
{
    for (int i = m_polyItems.size() - 1; i >= 0; --i) {
        if (m_polyItems[i]->polygon().containsPoint(scenePos, Qt::OddEvenFill))
            return m_polyItems[i];
    }
    return nullptr;
}

void AnnotationView::selectBox(BoxItem *box)
{
    if (m_selected == box)
        return;
    if (m_selected)
        m_selected->setSelected(false);
    m_selected = box;
    if (m_selected)
        m_selected->setSelected(true);
    selectPoly(nullptr); // 框与多边形互斥选中
}

void AnnotationView::selectPoly(PolygonItem *poly)
{
    if (m_selectedPoly == poly)
        return;
    if (m_selectedPoly)
        m_selectedPoly->setSelected(false);
    m_selectedPoly = poly;
    if (m_selectedPoly)
        m_selectedPoly->setSelected(true);
    selectBox(nullptr); // 框与多边形互斥选中
}

void AnnotationView::addBox(const QRectF &pixelRect)
{
    if (pixelRect.width() < 2 || pixelRect.height() < 2)
        return;
    QRectF r = pixelRect.normalized().intersected(m_scene->sceneRect());
    if (r.isEmpty())
        return;

    const QString name = (m_currentClassId >= 0 && m_currentClassId < m_classes.size())
                             ? m_classes[m_currentClassId] : QString();
    BoxItem *item = new BoxItem(r, m_currentClassId, name, classColor(m_currentClassId));
    item->setZValue(10); // 标注始终在图片之上
    m_scene->addItem(item);
    m_boxItems.append(item);
    selectBox(item);
    commitHistory();
    emit annotationChanged();
}

void AnnotationView::removeBox(BoxItem *box)
{
    if (!box)
        return;
    m_boxItems.removeOne(box);
    if (m_selected == box)
        m_selected = nullptr;
    m_scene->removeItem(box);
    delete box;
    commitHistory();
    emit annotationChanged();
}

void AnnotationView::changeBoxClass(BoxItem *box, int classId)
{
    if (!box)
        return;
    const QString name = (classId >= 0 && classId < m_classes.size()) ? m_classes[classId] : QString();
    box->setClassInfo(classId, name, classColor(classId));
    commitHistory();
    emit annotationChanged();
}

void AnnotationView::addPolygon(const QPolygonF &pts)
{
    if (pts.size() < 3)
        return;
    QPolygonF clamped;
    for (const QPointF &p : pts)
        clamped << clampToImage(p);
    if (clamped.boundingRect().width() < 1 || clamped.boundingRect().height() < 1)
        return;

    const QString name = (m_currentClassId >= 0 && m_currentClassId < m_classes.size())
                             ? m_classes[m_currentClassId] : QString();
    PolygonItem *item = new PolygonItem(clamped, m_currentClassId, name, classColor(m_currentClassId));
    item->setZValue(10); // 标注始终在图片之上
    m_scene->addItem(item);
    m_polyItems.append(item);
    selectPoly(item);
    commitHistory();
    emit annotationChanged();
}

void AnnotationView::removePolygon(PolygonItem *poly)
{
    if (!poly)
        return;
    m_polyItems.removeOne(poly);
    if (m_selectedPoly == poly)
        m_selectedPoly = nullptr;
    m_scene->removeItem(poly);
    delete poly;
    commitHistory();
    emit annotationChanged();
}

void AnnotationView::changePolyClass(PolygonItem *poly, int classId)
{
    if (!poly)
        return;
    const QString name = (classId >= 0 && classId < m_classes.size()) ? m_classes[classId] : QString();
    poly->setClassInfo(classId, name, classColor(classId));
    commitHistory();
    emit annotationChanged();
}

void AnnotationView::closePolygon()
{
    m_polyDrawing = false;
    // 去除过近的相邻点（双击会多出一个几乎重叠的点）
    QPolygonF pts;
    for (const QPointF &p : m_pendingPts) {
        if (pts.isEmpty() || QLineF(pts.last(), p).length() > 2.0)
            pts << p;
    }
    m_pendingPts.clear();
    if (m_polyRubber) {
        m_scene->removeItem(m_polyRubber);
        delete m_polyRubber;
        m_polyRubber = nullptr;
    }
    if (pts.size() >= 3)
        addPolygon(pts);
}

void AnnotationView::cancelPolyDraw()
{
    m_polyDrawing = false;
    m_pendingPts.clear();
    if (m_polyRubber) {
        m_scene->removeItem(m_polyRubber);
        delete m_polyRubber;
        m_polyRubber = nullptr;
    }
}

void AnnotationView::updatePolyRubber(const QPointF &cursor)
{
    if (!m_polyRubber)
        return;
    QPainterPath path;
    if (!m_pendingPts.isEmpty()) {
        path.moveTo(m_pendingPts.first());
        for (int i = 1; i < m_pendingPts.size(); ++i)
            path.lineTo(m_pendingPts[i]);
        path.lineTo(cursor); // 到鼠标的预览线
    }
    m_polyRubber->setPath(path);
}

// ---------- 自由绘制（画笔） ----------

void AnnotationView::startFreehand(const QPointF &scenePos)
{
    cancelPolyDraw();
    m_freehandDrawing = true;
    m_freehandPts.clear();
    m_freehandPts << clampToImage(scenePos);
    m_freehandRubber = new QGraphicsPathItem();
    m_freehandRubber->setPen(QPen(Qt::green, 2.0, Qt::SolidLine));
    m_freehandRubber->setZValue(1);
    m_scene->addItem(m_freehandRubber);
    updateFreehandRubber(scenePos);
}

void AnnotationView::updateFreehand(const QPointF &scenePos)
{
    if (!m_freehandDrawing)
        return;
    const QPointF p = clampToImage(scenePos);
    // 轨迹点过密会拖慢简化与标签体积，距离 <2px 的相邻点合并
    if (m_freehandPts.isEmpty() || QLineF(m_freehandPts.last(), p).length() > 2.0)
        m_freehandPts << p;
    updateFreehandRubber(p);
}

void AnnotationView::finishFreehand()
{
    m_freehandDrawing = false;
    if (m_freehandRubber) {
        m_scene->removeItem(m_freehandRubber);
        delete m_freehandRubber;
        m_freehandRubber = nullptr;
    }
    const QPolygonF poly = simplifyFreehand(m_freehandPts);
    m_freehandPts.clear();
    if (poly.size() >= 3 && poly.boundingRect().width() > 2
            && poly.boundingRect().height() > 2)
        addPolygon(poly); // 自由绘制的轨迹直接转成可编辑的多边形标注
}

void AnnotationView::cancelFreehand()
{
    m_freehandDrawing = false;
    m_freehandPts.clear();
    if (m_freehandRubber) {
        m_scene->removeItem(m_freehandRubber);
        delete m_freehandRubber;
        m_freehandRubber = nullptr;
    }
}

void AnnotationView::updateFreehandRubber(const QPointF &cursor)
{
    if (!m_freehandRubber)
        return;
    QPainterPath path;
    if (!m_freehandPts.isEmpty()) {
        path.moveTo(m_freehandPts.first());
        for (int i = 1; i < m_freehandPts.size(); ++i)
            path.lineTo(m_freehandPts[i]);
        if (QLineF(m_freehandPts.last(), cursor).length() > 2.0)
            path.lineTo(cursor);
    }
    m_freehandRubber->setPath(path);
}

// 自由绘制轨迹 -> 多边形（Douglas-Peucker 简化，控制点数 ≤ 200）
QPolygonF AnnotationView::simplifyFreehand(const QPolygonF &pts) const
{
    QVector<QPointF> clean;
    for (const QPointF &p : pts) {
        if (clean.isEmpty() || QLineF(clean.last(), p).length() > 2.0)
            clean << p;
    }
    if (clean.size() < 3)
        return QPolygonF();

    double eps = 2.5; // 简化容差（图像像素）
    QVector<QPointF> out;
    for (int iter = 0; iter < 8; ++iter) {
        out.clear();
        out << clean.first();
        dpReduce(clean, 0, clean.size() - 1, eps, out);
        out << clean.last();
        if (out.size() <= 200)
            break;
        eps *= 2.0; // 点太多就加大容差再简化
    }
    QPolygonF poly;
    for (const QPointF &p : out)
        poly << p;
    return poly;
}

void AnnotationView::mousePressEvent(QMouseEvent *event)
{
    if (!m_pixmapItem) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    const QPointF scenePos = mapToScene(event->pos());

    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_panStart = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() == Qt::LeftButton && m_mode == AnnoMode::Freehand) {
        startFreehand(scenePos);
        return;
    }

    if (event->button() == Qt::LeftButton && m_mode == AnnoMode::Polygon) {
        // 拖动选中多边形的顶点
        if (m_selectedPoly) {
            const double tol = 10.0 / transform().m11();
            const int vi = m_selectedPoly->vertexAt(scenePos, tol);
            if (vi >= 0) {
                m_vertexDrag = true;
                m_vertexPoly = m_selectedPoly;
                m_vertexIndex = vi;
                return;
            }
        }
        // 点中已有多边形：选中并整体移动
        PolygonItem *hit = polyAt(scenePos);
        if (hit) {
            selectPoly(hit);
            m_polyMoving = true;
            m_movingPoly = hit;
            m_polyMoveLast = scenePos;
            return;
        }
        // 正在绘制：继续加点
        if (m_polyDrawing) {
            m_pendingPts << clampToImage(scenePos);
            updatePolyRubber(scenePos);
            return;
        }
        // 空白处：开始新多边形
        selectBox(nullptr);
        selectPoly(nullptr);
        m_polyDrawing = true;
        m_pendingPts.clear();
        m_pendingPts << clampToImage(scenePos);
        m_polyRubber = new QGraphicsPathItem();
        m_polyRubber->setPen(QPen(Qt::green, 1.5, Qt::DashLine));
        m_polyRubber->setZValue(1);
        m_scene->addItem(m_polyRubber);
        updatePolyRubber(scenePos);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        BoxItem *hit = boxAt(scenePos);
        if (hit) {
            // 选中并开始移动
            selectBox(hit);
            m_moving = true;
            m_movingBox = hit;
            m_moveOffset = hit->rect().topLeft() - scenePos;
        } else {
            // 开始绘制新框
            m_drawing = true;
            m_drawStart = scenePos;
            selectBox(nullptr);
            m_rubber = new QGraphicsRectItem(QRectF(m_drawStart, m_drawStart));
            QPen pen(Qt::green, 1.5, Qt::DashLine);
            m_rubber->setPen(pen);
            m_rubber->setBrush(QColor(0, 255, 0, 30));
            m_scene->addItem(m_rubber);
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void AnnotationView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        const QPoint delta = event->pos() - m_panStart;
        m_panStart = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        return;
    }

    if (m_mode == AnnoMode::Freehand && m_freehandDrawing) {
        const QPointF scenePos = mapToScene(event->pos());
        updateFreehand(scenePos);
        return;
    }

    if (m_mode == AnnoMode::Polygon) {
        const QPointF scenePos = mapToScene(event->pos());
        if (m_polyDrawing && m_polyRubber) {
            updatePolyRubber(scenePos);
            return;
        }
        if (m_vertexDrag && m_vertexPoly) {
            m_vertexPoly->moveVertex(m_vertexIndex, clampToImage(scenePos));
            return;
        }
        if (m_polyMoving && m_movingPoly) {
            const QPointF d = scenePos - m_polyMoveLast;
            m_polyMoveLast = scenePos;
            QPolygonF pts = m_movingPoly->pixelPoints();
            pts.translate(d.x(), d.y());
            m_movingPoly->setPixelPoints(pts);
            return;
        }
    }

    if (m_drawing && m_rubber) {
        const QPointF scenePos = mapToScene(event->pos());
        m_rubber->setRect(QRectF(m_drawStart, scenePos).normalized());
        return;
    }

    if (m_moving && m_movingBox) {
        const QPointF scenePos = mapToScene(event->pos());
        QRectF nr = m_movingBox->rect();
        nr.moveTopLeft(scenePos + m_moveOffset);
        m_movingBox->setRect(nr);
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void AnnotationView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        return;
    }

    if (event->button() == Qt::LeftButton && m_mode == AnnoMode::Freehand
            && m_freehandDrawing) {
        finishFreehand();
        return;
    }

    if (event->button() == Qt::LeftButton && m_mode == AnnoMode::Polygon) {
        if (m_vertexDrag) {
            m_vertexDrag = false;
            m_vertexPoly = nullptr;
            m_vertexIndex = -1;
            commitHistory();
            emit annotationChanged();
            return;
        }
        if (m_polyMoving) {
            m_polyMoving = false;
            m_movingPoly = nullptr;
            commitHistory();
            emit annotationChanged();
            return;
        }
    }

    if (event->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;
        if (m_rubber) {
            const QRectF r = m_rubber->rect();
            m_scene->removeItem(m_rubber);
            delete m_rubber;
            m_rubber = nullptr;
            addBox(r);
        }
        return;
    }

    if (event->button() == Qt::LeftButton && m_moving) {
        m_moving = false;
        m_movingBox = nullptr;
        commitHistory();
        emit annotationChanged();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void AnnotationView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_mode == AnnoMode::Polygon && m_polyDrawing && event->button() == Qt::LeftButton) {
        closePolygon();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void AnnotationView::wheelEvent(QWheelEvent *event)
{
    if (!m_pixmapItem) {
        QGraphicsView::wheelEvent(event);
        return;
    }
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    scale(factor, factor);
}

void AnnotationView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (m_selected) {
            removeBox(m_selected);
            return;
        }
        if (m_selectedPoly) {
            removePolygon(m_selectedPoly);
            return;
        }
    }
    if (event->key() == Qt::Key_Escape) {
        if (m_freehandDrawing) {
            cancelFreehand();
            return;
        }
        if (m_polyDrawing) {
            cancelPolyDraw();
            return;
        }
        if (m_drawing) {
            m_drawing = false;
            if (m_rubber) {
                m_scene->removeItem(m_rubber);
                delete m_rubber;
                m_rubber = nullptr;
            }
            return;
        }
        selectBox(nullptr);
        selectPoly(nullptr);
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            && m_mode == AnnoMode::Polygon && m_polyDrawing) {
        closePolygon();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void AnnotationView::contextMenuEvent(QContextMenuEvent *event)
{
    const QPointF scenePos = mapToScene(event->pos());

    // 绘制中右键 = 闭合多边形
    if (m_polyDrawing) {
        closePolygon();
        return;
    }

    if (m_mode == AnnoMode::Polygon) {
        PolygonItem *poly = polyAt(scenePos);
        if (poly) {
            selectPoly(poly);
            QMenu menu(this);
            QMenu *classMenu = menu.addMenu("设置类别");
            for (int i = 0; i < m_classes.size(); ++i) {
                QAction *a = classMenu->addAction(m_classes[i]);
                const int cid = i;
                connect(a, &QAction::triggered, this, [this, poly, cid]() {
                    changePolyClass(poly, cid);
                });
            }
            menu.addSeparator();
            QAction *del = menu.addAction("删除多边形");
            connect(del, &QAction::triggered, this, [this, poly]() {
                removePolygon(poly);
            });
            menu.exec(event->globalPos());
            return;
        }
    }

    BoxItem *hit = boxAt(scenePos);
    if (!hit) {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    selectBox(hit);
    QMenu menu(this);

    QMenu *classMenu = menu.addMenu("设置类别");
    for (int i = 0; i < m_classes.size(); ++i) {
        QAction *a = classMenu->addAction(m_classes[i]);
        const int cid = i;
        connect(a, &QAction::triggered, this, [this, hit, cid]() {
            changeBoxClass(hit, cid);
        });
    }

    menu.addSeparator();
    QAction *del = menu.addAction("删除框");
    connect(del, &QAction::triggered, this, [this, hit]() {
        removeBox(hit);
    });

    menu.exec(event->globalPos());
}
