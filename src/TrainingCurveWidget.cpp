#include "TrainingCurveWidget.h"

#include <QColor>
#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>

TrainingCurveWidget::TrainingCurveWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(280);
}

void TrainingCurveWidget::clear()
{
    m_epoch.clear();
    m_box.clear();
    m_cls.clear();
    m_dfl.clear();
    m_map50.clear();
    m_map5095.clear();
    update();
}

void TrainingCurveWidget::addPoint(int epoch, double boxLoss, double clsLoss, double dflLoss,
                                   double map50, double map5095)
{
    m_epoch.append(epoch);
    m_box.append(boxLoss);
    m_cls.append(clsLoss);
    m_dfl.append(dflLoss);
    m_map50.append(map50);
    m_map5095.append(map5095);
    update();
}

void TrainingCurveWidget::drawChart(QPainter &p, const QRectF &rect, const QString &title,
                                    const QVector<Series> &series, bool markBest)
{
    // 背景与边框
    p.fillRect(rect, QColor(0x15, 0x15, 0x22));
    p.setPen(QColor(0x2b, 0x2b, 0x40));
    p.drawRect(rect);

    // 标题
    p.setPen(QColor(0xcf, 0xcf, 0xe0));
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(12);
    p.setFont(f);
    p.drawText(rect.adjusted(8, 4, -8, 0), Qt::AlignLeft | Qt::AlignTop, title);
    f.setBold(false);
    p.setFont(f);

    QRectF plot = rect.adjusted(46, 24, -14, -24);
    if (plot.width() <= 10 || plot.height() <= 10)
        return;

    // 数据范围
    double lo = 1e9, hi = -1e9;
    for (const Series &s : series) {
        for (double v : s.data) {
            lo = qMin(lo, v);
            hi = qMax(hi, v);
        }
    }
    if (lo > hi)
        return;
    if (hi - lo < 1e-6) {
        lo -= 1.0;
        hi += 1.0;
    }
    const double pad = (hi - lo) * 0.08;
    lo -= pad;
    hi += pad;

    const int n = m_epoch.size();
    const int x0 = m_epoch.first();
    const int x1 = qMax(m_epoch.last(), x0 + 1);

    auto xAt = [&](int ep) {
        return plot.left() + (ep - x0) * plot.width() / double(x1 - x0);
    };
    auto yAt = [&](double v) {
        return plot.bottom() - (v - lo) * plot.height() / (hi - lo);
    };

    // 网格 + 刻度
    p.setPen(QPen(QColor(0x26, 0x26, 0x38), 1, Qt::DashLine));
    const int ticks = 4;
    for (int i = 0; i <= ticks; ++i) {
        const double v = lo + (hi - lo) * i / ticks;
        const double y = yAt(v);
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(QColor(0x8a, 0x8a, 0xa8));
        p.drawText(QRectF(0, y - 8, plot.left() - 4, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(v, 'f', (hi - lo > 5 ? 1 : 2)));
        p.setPen(QPen(QColor(0x26, 0x26, 0x38), 1, Qt::DashLine));
    }
    // X 轴刻度
    for (int i = 0; i <= 4; ++i) {
        const int ep = x0 + (x1 - x0) * i / 4;
        const double x = xAt(ep);
        p.setPen(QColor(0x8a, 0x8a, 0xa8));
        p.drawText(QRectF(x - 20, plot.bottom() + 2, 40, 16), Qt::AlignCenter,
                   QString::number(ep));
    }

    // 曲线
    for (const Series &s : series) {
        if (s.data.size() < 2)
            continue;
        QPainterPath path;
        bool started = false;
        for (int i = 0; i < s.data.size() && i < n; ++i) {
            const QPointF pt(xAt(m_epoch[i]), yAt(s.data[i]));
            if (!started) {
                path.moveTo(pt);
                started = true;
            } else {
                path.lineTo(pt);
            }
        }
        p.setPen(QPen(s.color, 1.8));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

    // 最优标记（mAP 图）
    if (markBest && n > 0) {
        auto bestOf = [&](const QVector<double> &data) {
            int idx = 0;
            for (int i = 1; i < data.size(); ++i)
                if (data[i] > data[idx])
                    idx = i;
            return idx;
        };
        struct Best { QString label; QColor color; int idx; double val; };
        const QVector<Best> bests = {
            {"Best mAP50", QColor(0x8a, 0xb4, 0xf8), bestOf(m_map50), m_map50.value(bestOf(m_map50))},
            {"Best mAP50-95", QColor(0xe8, 0x79, 0xf9), bestOf(m_map5095), m_map5095.value(bestOf(m_map5095))},
        };
        for (const Best &b : bests) {
            if (b.idx < 0 || b.idx >= n)
                continue;
            const QPointF pt(xAt(m_epoch[b.idx]), yAt(b.val));
            p.setPen(QPen(b.color, 1.5));
            p.setBrush(Qt::white);
            QPolygonF star;
            const double r = 4.5;
            star << QPointF(pt.x(), pt.y() - r) << QPointF(pt.x() + r, pt.y())
                 << QPointF(pt.x(), pt.y() + r) << QPointF(pt.x() - r, pt.y());
            p.drawPolygon(star);
            const QString label = QString("%1 %2 @%3")
                                      .arg(b.label).arg(b.val, 0, 'f', 3).arg(m_epoch[b.idx]);
            p.setPen(b.color);
            p.drawText(QRectF(pt.x() + 6, pt.y() - 18, 150, 16), Qt::AlignLeft | Qt::AlignVCenter, label);
        }
    }

    // 图例（右上角）
    QFontMetrics fm(p.font());
    double legendX = plot.right() - 8;
    double legendY = plot.top() + 4;
    for (int i = series.size() - 1; i >= 0; --i) {
        const Series &s = series[i];
        if (s.data.isEmpty())
            continue;
        const QString txt = s.name;
        const int w = fm.horizontalAdvance(txt) + 26;
        legendX -= w;
        p.setPen(s.color);
        p.drawLine(QPointF(legendX, legendY + 7), QPointF(legendX + 14, legendY + 7));
        p.setPen(QColor(0xb8, 0xb8, 0xcc));
        p.drawText(QRectF(legendX + 16, legendY, w - 16, 14), Qt::AlignLeft | Qt::AlignVCenter, txt);
        legendX -= 6;
    }
}

void TrainingCurveWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(0x1b, 0x1b, 0x26));

    if (m_epoch.isEmpty()) {
        p.setPen(QColor(0x8a, 0x8a, 0xa8));
        p.drawText(rect(), Qt::AlignCenter, "开始训练后，这里会显示损失 / mAP 曲线");
        return;
    }

    const QRectF full = rect().adjusted(6, 6, -6, -6);
    const double gap = 10;
    const double h = (full.height() - gap) / 2.0;
    const QRectF lossRect(full.left(), full.top(), full.width(), h);
    const QRectF mapRect(full.left(), full.top() + h + gap, full.width(), h);

    QVector<Series> lossSeries;
    lossSeries.append({"box_loss", QColor(0x6a, 0xa9, 0xff), m_box});
    lossSeries.append({"cls_loss", QColor(0xff, 0x6b, 0x6b), m_cls});
    lossSeries.append({"dfl_loss", QColor(0xff, 0xd1, 0x66), m_dfl});

    QVector<Series> mapSeries;
    mapSeries.append({"mAP50", QColor(0x8a, 0xb4, 0xf8), m_map50});
    mapSeries.append({"mAP50-95", QColor(0xe8, 0x79, 0xf9), m_map5095});

    drawChart(p, lossRect, "训练损失曲线", lossSeries, false);
    drawChart(p, mapRect, "mAP 指标曲线", mapSeries, true);
}
