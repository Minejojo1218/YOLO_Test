#pragma once

#include <QVector>
#include <QWidget>

// 训练曲线面板：上半部分损失曲线，下半部分 mAP 曲线（无 Q_OBJECT，纯绘制）
class TrainingCurveWidget : public QWidget
{
public:
    explicit TrainingCurveWidget(QWidget *parent = nullptr);

    void clear();
    void addPoint(int epoch, double boxLoss, double clsLoss, double dflLoss,
                  double map50, double map5095);
    bool isEmpty() const { return m_epoch.isEmpty(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Series
    {
        QString name;
        QColor color;
        QVector<double> data;
    };
    void drawChart(QPainter &p, const QRectF &rect, const QString &title,
                   const QVector<Series> &series, bool markBest);

    QVector<int> m_epoch;
    QVector<double> m_box, m_cls, m_dfl, m_map50, m_map5095;
};
