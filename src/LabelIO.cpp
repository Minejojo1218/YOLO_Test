#include "LabelIO.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace LabelIO {

// 归一化数字输出（6 位小数，与 ultralytics 约定一致）
static QString num(double v)
{
    return QString::number(v, 'f', 6);
}

Annotation readYoloTxt(const QString &path)
{
    Annotation ann;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return ann;

    // 先整体读入并按行切分，判断该文件是「框格式」还是「多边形格式」。
    // ultralytics 不允许同一文件混用两种格式（混用会整条丢弃），所以按文件统一解析。
    QTextStream in(&f);
    static const QRegularExpression ws("\\s+");
    QList<QStringList> rows;
    bool hasPolygon = false;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        const QStringList parts = line.split(ws, Qt::SkipEmptyParts);
        if (parts.size() < 5)
            continue;
        if (parts.size() >= 7 && parts.size() % 2 == 1)
            hasPolygon = true; // 多边形行：class x1 y1 x2 y2 ...（至少 3 个点 → ≥7 且奇数）
        rows.append(parts);
    }

    for (const QStringList &parts : rows) {
        bool ok = true;
        const int classId = parts[0].toInt(&ok);
        if (!ok)
            continue;

        if (hasPolygon) {
            // 多边形格式：只解析多边形行（class x1 y1 ...，token 数为奇数 1+2k）
            // 跳过 5 个数字的旧框行与偶数个 token 的异常行
            if (parts.size() < 7 || parts.size() % 2 == 0)
                continue;
            LabelPolygon poly;
            poly.classId = classId;
            const int pointCount = (parts.size() - 1) / 2;
            bool valid = true;
            for (int i = 0; i < pointCount; ++i) {
                const float px = parts[1 + i * 2].toFloat(&ok);
                if (!ok) {
                    valid = false;
                    break;
                }
                const float py = parts[2 + i * 2].toFloat(&ok);
                if (!ok) {
                    valid = false;
                    break;
                }
                poly.points.append(QPointF(px, py));
            }
            if (valid && poly.points.size() >= 3)
                ann.polygons.append(poly);
        } else {
            // 框格式：class cx cy w h
            if (parts.size() != 5)
                continue;
            LabelBox b;
            b.classId = classId;
            b.x = parts[1].toFloat(&ok);
            b.y = parts[2].toFloat(&ok);
            b.w = parts[3].toFloat(&ok);
            b.h = parts[4].toFloat(&ok);
            if (ok)
                ann.boxes.append(b);
        }
    }
    return ann;
}

bool writeYoloTxt(const QString &path, const Annotation &ann)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;

    // ultralytics 不允许同一文件混用框与多边形格式，因此：
    // 只要该图含多边形，就把所有框也转成四角多边形，统一按多边形格式输出。
    const bool usePolygon = !ann.polygons.isEmpty();

    QTextStream out(&f);
    for (const LabelBox &b : ann.boxes) {
        if (!usePolygon) {
            out << b.classId << ' '
                << num(b.x) << ' '
                << num(b.y) << ' '
                << num(b.w) << ' '
                << num(b.h) << '\n';
        } else {
            // 框 -> 四角多边形（YOLO-seg 格式）
            const double cx = b.x, cy = b.y;
            const double hw = b.w / 2.0, hh = b.h / 2.0;
            out << b.classId
                << ' ' << num(cx - hw) << ' ' << num(cy - hh)
                << ' ' << num(cx + hw) << ' ' << num(cy - hh)
                << ' ' << num(cx + hw) << ' ' << num(cy + hh)
                << ' ' << num(cx - hw) << ' ' << num(cy + hh) << '\n';
        }
    }
    for (const LabelPolygon &p : ann.polygons) {
        out << p.classId;
        for (const QPointF &pt : p.points)
            out << ' ' << num(pt.x()) << ' ' << num(pt.y());
        out << '\n';
    }
    return true;
}

} // namespace LabelIO
