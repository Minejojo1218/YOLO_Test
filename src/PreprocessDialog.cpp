#include "PreprocessDialog.h"
#include "LabelIO.h"
#include "ProjectManager.h"

#include <opencv2/opencv.hpp>

#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

namespace {

QImage cvMatToQImage(const cv::Mat &m)
{
    if (m.empty())
        return QImage();
    if (m.channels() == 1) {
        cv::Mat bgr;
        cv::cvtColor(m, bgr, cv::COLOR_GRAY2BGR);
        QImage img(bgr.data, bgr.cols, bgr.rows, (int)bgr.step, QImage::Format_BGR888);
        return img.copy();
    }
    QImage img(m.data, m.cols, m.rows, (int)m.step, QImage::Format_BGR888);
    return img.copy();
}

} // namespace

PreprocessDialog::PreprocessDialog(ProjectManager *project, QWidget *parent)
    : QDialog(parent)
    , m_project(project)
{
    setWindowTitle("图像预处理");
    resize(1180, 720);

    // ---------- 图像列表 ----------
    m_imageList = new QListWidget(this);
    m_imageList->setFixedWidth(230);
    m_imageList->setAlternatingRowColors(true);

    // ---------- 预览 ----------
    m_origLabel = new QLabel("原始图像", this);
    m_origLabel->setAlignment(Qt::AlignCenter);
    m_origLabel->setMinimumSize(300, 240);
    m_origLabel->setStyleSheet("background: #14141e; color: #9a9ab0;");
    m_procLabel = new QLabel("处理后", this);
    m_procLabel->setAlignment(Qt::AlignCenter);
    m_procLabel->setMinimumSize(300, 240);
    m_procLabel->setStyleSheet("background: #14141e; color: #9a9ab0;");

    QHBoxLayout *previewLay = new QHBoxLayout();
    QVBoxLayout *origCol = new QVBoxLayout();
    origCol->addWidget(new QLabel("原始图像"));
    origCol->addWidget(m_origLabel, 1);
    QVBoxLayout *procCol = new QVBoxLayout();
    procCol->addWidget(new QLabel("处理后"));
    procCol->addWidget(m_procLabel, 1);
    previewLay->addLayout(origCol, 1);
    previewLay->addLayout(procCol, 1);

    // ---------- 步骤列表 ----------
    QGroupBox *stepBox = new QGroupBox("处理步骤", this);
    QVBoxLayout *stepLay = new QVBoxLayout(stepBox);
    m_stepList = new QListWidget(stepBox);
    m_stepList->setMaximumHeight(170);
    stepLay->addWidget(m_stepList);

    QHBoxLayout *stepBtns = new QHBoxLayout();
    QPushButton *bAdd = new QPushButton("添加", stepBox);
    QPushButton *bDel = new QPushButton("删除", stepBox);
    QPushButton *bUp = new QPushButton("上移", stepBox);
    QPushButton *bDown = new QPushButton("下移", stepBox);
    stepBtns->addWidget(bAdd);
    stepBtns->addWidget(bDel);
    stepBtns->addWidget(bUp);
    stepBtns->addWidget(bDown);
    stepBtns->addStretch(1);
    stepLay->addLayout(stepBtns);

    // 参数 + 添加步骤
    QHBoxLayout *paramLay = new QHBoxLayout();
    m_typeCombo = new QComboBox(stepBox);
    m_typeCombo->addItems({"镜像(水平)", "镜像(垂直)", "旋转90°", "旋转180°", "旋转270°",
                           "亮度调整", "对比度调整", "反色", "灰度", "高斯模糊", "锐化"});
    m_valueSpin = new QDoubleSpinBox(stepBox);
    m_valueSpin->setRange(-100, 100);
    m_valueSpin->setValue(0);
    paramLay->addWidget(new QLabel("类型"));
    paramLay->addWidget(m_typeCombo, 1);
    paramLay->addWidget(new QLabel("参数"));
    paramLay->addWidget(m_valueSpin);
    stepLay->addLayout(paramLay);

    // ---------- 右侧操作 ----------
    QGroupBox *opBox = new QGroupBox("操作", this);
    QVBoxLayout *opLay = new QVBoxLayout(opBox);
    QPushButton *bPreview = new QPushButton("预览效果", opBox);
    QPushButton *bReset = new QPushButton("重置步骤", opBox);
    bPreview->setMinimumHeight(34);
    opLay->addWidget(bPreview);
    opLay->addWidget(bReset);

    QHBoxLayout *outLay = new QHBoxLayout();
    QPushButton *bOut = new QPushButton("输出目录", opBox);
    m_outDir = new QLineEdit(opBox);
    m_outDir->setReadOnly(true);
    outLay->addWidget(bOut);
    outLay->addWidget(m_outDir, 1);
    opLay->addLayout(outLay);

    QPushButton *bApply = new QPushButton("应用（当前图）", opBox);
    QPushButton *bBatch = new QPushButton("批量应用（全部）", opBox);
    bApply->setMinimumHeight(34);
    bBatch->setMinimumHeight(34);
    opLay->addWidget(bApply);
    opLay->addWidget(bBatch);
    QLabel *hint = new QLabel(
        "说明：镜像/旋转会同步转换标签坐标；输出到独立目录（images/ + labels/），"
        "不会覆盖原项目。Pixel 类操作（亮度/反色等）标签不变。", opBox);
    hint->setWordWrap(true);
    opLay->addWidget(hint);
    opLay->addStretch(1);

    QPushButton *bClose = new QPushButton("关闭", opBox);
    opLay->addWidget(bClose);

    // ---------- 总布局 ----------
    QVBoxLayout *center = new QVBoxLayout();
    center->addLayout(previewLay, 2);
    center->addWidget(new QLabel("图像列表（共 0 张）"));
    center->addWidget(m_imageList, 1);

    QHBoxLayout *main = new QHBoxLayout();
    main->addLayout(center, 1);
    QVBoxLayout *right = new QVBoxLayout();
    right->addWidget(stepBox);
    right->addWidget(opBox, 1);
    main->addLayout(right);
    setLayout(main);

    // ---------- 初始化数据 ----------
    if (m_project && m_project->isValid()) {
        m_files = m_project->imageFiles();
        for (const QString &f : m_files)
            m_imageList->addItem(QFileInfo(f).fileName());
        if (!m_files.isEmpty())
            m_imageList->setCurrentRow(0);
        m_outDir->setText(m_project->rootDir() + "/preprocessed");
    }
    m_outDir->setText(m_outDir->text().isEmpty()
                          ? QDir::currentPath() + "/preprocessed" : m_outDir->text());

    // ---------- 信号 ----------
    connect(m_imageList, &QListWidget::currentRowChanged, this, &PreprocessDialog::onImageSelected);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) { updateParamsForType(idx); });
    connect(m_stepList, &QListWidget::currentRowChanged, this, &PreprocessDialog::onStepSelectionChanged);
    connect(bAdd, &QPushButton::clicked, this, &PreprocessDialog::onAddStep);
    connect(bDel, &QPushButton::clicked, this, &PreprocessDialog::onDeleteStep);
    connect(bUp, &QPushButton::clicked, this, &PreprocessDialog::onUpStep);
    connect(bDown, &QPushButton::clicked, this, &PreprocessDialog::onDownStep);
    connect(bPreview, &QPushButton::clicked, this, &PreprocessDialog::onPreview);
    connect(bReset, &QPushButton::clicked, this, &PreprocessDialog::onReset);
    connect(bApply, &QPushButton::clicked, this, &PreprocessDialog::onApply);
    connect(bBatch, &QPushButton::clicked, this, &PreprocessDialog::onApply);
    connect(bOut, &QPushButton::clicked, this, &PreprocessDialog::onBrowseOutput);
    connect(bClose, &QPushButton::clicked, this, &QDialog::accept);

    updateParamsForType(0);
}

// ---------- 步骤基础信息 ----------

QString PreprocessDialog::stepName(int type)
{
    static const char *names[] = {"镜像(水平)", "镜像(垂直)", "旋转90°", "旋转180°", "旋转270°",
                                  "亮度调整", "对比度调整", "反色", "灰度", "高斯模糊", "锐化"};
    return (type >= 0 && type < 11) ? QString::fromUtf8(names[type]) : QString("?");
}

QPair<double, double> PreprocessDialog::valueRange(int type)
{
    switch (type) {
    case 5: return {-100, 100};        // 亮度
    case 6: return {0.2, 3.0};         // 对比度
    case 9: return {3, 31};            // 模糊核
    default: return {0, 0};
    }
}

double PreprocessDialog::defaultValue(int type)
{
    switch (type) {
    case 5: return 20;
    case 6: return 1.2;
    case 9: return 5;
    default: return 0;
    }
}

bool PreprocessDialog::isGeometryStep(int type)
{
    return type >= 0 && type <= 4;   // 镜像 / 旋转
}

// ---------- 图像处理 ----------

cv::Mat PreprocessDialog::applySteps(const cv::Mat &img, const std::vector<Step> &steps)
{
    cv::Mat out = img.clone();
    for (const Step &s : steps) {
        switch (s.type) {
        case 0:
            cv::flip(out, out, 1);
            break;
        case 1:
            cv::flip(out, out, 0);
            break;
        case 2:
            cv::rotate(out, out, cv::ROTATE_90_CLOCKWISE);
            break;
        case 3:
            cv::rotate(out, out, cv::ROTATE_180);
            break;
        case 4:
            cv::rotate(out, out, cv::ROTATE_90_COUNTERCLOCKWISE);
            break;
        case 5: { // 亮度
            out.convertTo(out, -1, 1.0, s.value);
            break;
        }
        case 6: { // 对比度
            out.convertTo(out, -1, s.value, 0);
            break;
        }
        case 7:
            cv::bitwise_not(out, out);
            break;
        case 8: { // 灰度（转回 3 通道便于显示/保存）
            cv::Mat gray;
            cv::cvtColor(out, gray, cv::COLOR_BGR2GRAY);
            cv::cvtColor(gray, out, cv::COLOR_GRAY2BGR);
            break;
        }
        case 9: { // 高斯模糊
            int k = int(s.value) | 1;
            k = qBound(3, k, 31);
            cv::GaussianBlur(out, out, cv::Size(k, k), 0);
            break;
        }
        case 10: { // 锐化
            const cv::Mat kernel = (cv::Mat_<float>(3, 3) <<
                0, -1, 0,
                -1, 5, -1,
                0, -1, 0);
            cv::filter2D(out, out, out.depth(), kernel);
            break;
        }
        default:
            break;
        }
    }
    return out;
}

// 按几何步骤顺序转换标签坐标（归一化）
Annotation PreprocessDialog::transformLabels(const Annotation &ann) const
{
    Annotation out = ann;
    for (const Step &s : m_steps) {
        switch (s.type) {
        case 0: // 镜像水平：x -> 1-x
            for (LabelBox &b : out.boxes)
                b.x = 1.0f - b.x;
            for (LabelPolygon &p : out.polygons)
                for (QPointF &pt : p.points)
                    pt.setX(1.0 - pt.x());
            break;
        case 1: // 镜像垂直：y -> 1-y
            for (LabelBox &b : out.boxes)
                b.y = 1.0f - b.y;
            for (LabelPolygon &p : out.polygons)
                for (QPointF &pt : p.points)
                    pt.setY(1.0 - pt.y());
            break;
        case 2: // 旋转 90° 顺时针：(x,y) -> (1-y, x)，宽高互换
            for (LabelBox &b : out.boxes) {
                const float nx = 1.0f - b.y, ny = b.x;
                b.x = nx;
                b.y = ny;
                std::swap(b.w, b.h);
            }
            for (LabelPolygon &p : out.polygons)
                for (QPointF &pt : p.points) {
                    const double nx = 1.0 - pt.y(), ny = pt.x();
                    pt = QPointF(nx, ny);
                }
            break;
        case 3: // 旋转 180°：(x,y) -> (1-x, 1-y)
            for (LabelBox &b : out.boxes) {
                b.x = 1.0f - b.x;
                b.y = 1.0f - b.y;
            }
            for (LabelPolygon &p : out.polygons)
                for (QPointF &pt : p.points)
                    pt = QPointF(1.0 - pt.x(), 1.0 - pt.y());
            break;
        case 4: // 旋转 270°（逆时针 90°）：(x,y) -> (y, 1-x)，宽高互换
            for (LabelBox &b : out.boxes) {
                const float nx = b.y, ny = 1.0f - b.x;
                b.x = nx;
                b.y = ny;
                std::swap(b.w, b.h);
            }
            for (LabelPolygon &p : out.polygons)
                for (QPointF &pt : p.points) {
                    const double nx = pt.y(), ny = 1.0 - pt.x();
                    pt = QPointF(nx, ny);
                }
            break;
        default:
            break;
        }
    }
    return out;
}

// ---------- 界面逻辑 ----------

void PreprocessDialog::updateParamsForType(int type)
{
    const auto range = valueRange(type);
    const bool hasParam = range.second > range.first;
    m_valueSpin->setRange(range.first, range.second);
    m_valueSpin->setValue(defaultValue(type));
    m_valueSpin->setEnabled(hasParam);
    m_valueSpin->setDecimals(type == 6 ? 2 : 1);
    m_valueSpin->setSingleStep(type == 6 ? 0.1 : 1.0);
}

void PreprocessDialog::onImageSelected(int row)
{
    loadImage(row);
}

void PreprocessDialog::loadImage(int row)
{
    m_currentRow = row;
    m_original = cv::Mat();
    if (row < 0 || row >= m_files.size())
        return;
    m_original = cv::imread(m_files[row].toLocal8Bit().constData());
    showMat(m_origLabel, m_original);
    onPreview();
}

void PreprocessDialog::showMat(QLabel *label, const cv::Mat &m)
{
    if (m.empty()) {
        label->setText("—");
        return;
    }
    const QImage img = cvMatToQImage(m);
    label->setPixmap(QPixmap::fromImage(img).scaled(label->size() - QSize(8, 8),
                                                    Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void PreprocessDialog::onAddStep()
{
    Step s;
    s.type = m_typeCombo->currentIndex();
    s.value = m_valueSpin->value();
    m_steps.push_back(s);
    refreshStepList();
    onPreview();
}

void PreprocessDialog::onDeleteStep()
{
    const int row = m_stepList->currentRow();
    if (row < 0 || row >= (int)m_steps.size())
        return;
    m_steps.erase(m_steps.begin() + row);
    refreshStepList();
    onPreview();
}

void PreprocessDialog::onUpStep()
{
    const int row = m_stepList->currentRow();
    if (row <= 0 || row >= (int)m_steps.size())
        return;
    std::swap(m_steps[row], m_steps[row - 1]);
    refreshStepList();
    m_stepList->setCurrentRow(row - 1);
    onPreview();
}

void PreprocessDialog::onDownStep()
{
    const int row = m_stepList->currentRow();
    if (row < 0 || row + 1 >= (int)m_steps.size())
        return;
    std::swap(m_steps[row], m_steps[row + 1]);
    refreshStepList();
    m_stepList->setCurrentRow(row + 1);
    onPreview();
}

void PreprocessDialog::onStepSelectionChanged(int row)
{
    Q_UNUSED(row);
}

void PreprocessDialog::refreshStepList()
{
    m_stepList->clear();
    for (size_t i = 0; i < m_steps.size(); ++i) {
        const Step &s = m_steps[i];
        QString text = QString("%1. %2").arg(i + 1).arg(stepName(s.type));
        if (valueRange(s.type).second > valueRange(s.type).first)
            text += QString("（参数 %1）").arg(s.value, 0, 'f', 2);
        m_stepList->addItem(text);
    }
}

void PreprocessDialog::onPreview()
{
    if (m_original.empty())
        return;
    const cv::Mat proc = applySteps(m_original, m_steps);
    showMat(m_procLabel, proc);
}

void PreprocessDialog::onReset()
{
    m_steps.clear();
    refreshStepList();
    onPreview();
}

void PreprocessDialog::onBrowseOutput()
{
    const QString dir = QFileDialog::getExistingDirectory(this, "选择输出目录", m_outDir->text());
    if (!dir.isEmpty())
        m_outDir->setText(dir);
}

void PreprocessDialog::onApply()
{
    if (m_steps.empty()) {
        QMessageBox::information(this, "提示", "请先添加处理步骤。");
        return;
    }
    const QString outDir = m_outDir->text().trimmed();
    if (outDir.isEmpty()) {
        QMessageBox::warning(this, "提示", "请选择输出目录。");
        return;
    }

    // 判断是否批量模式（批量应用按钮与单图应用共用一个槽：用 sender 区分）
    const bool batch = (sender() == nullptr) ? false
        : (static_cast<QPushButton *>(sender())->text().contains("批量"));
    QList<int> targets;
    if (batch) {
        for (int i = 0; i < m_files.size(); ++i)
            targets.append(i);
    } else {
        targets.append(m_currentRow);
    }
    if (targets.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择图片。");
        return;
    }

    QDir().mkpath(outDir + "/images");
    QDir().mkpath(outDir + "/labels");
    int done = 0;
    for (int row : targets) {
        if (row < 0 || row >= m_files.size())
            continue;
        cv::Mat img = cv::imread(m_files[row].toLocal8Bit().constData());
        if (img.empty())
            continue;
        const cv::Mat proc = applySteps(img, m_steps);
        const QString base = QFileInfo(m_files[row]).completeBaseName();
        const QString ext = QFileInfo(m_files[row]).suffix();
        const QString outImg = QString("%1/images/%2.%3").arg(outDir, base, ext);
        cv::imwrite(outImg.toLocal8Bit().constData(), proc);

        // 同步标签（镜像/旋转会转换坐标）
        const QString lblPath = m_project ? m_project->labelPathFor(m_files[row]) : QString();
        if (!lblPath.isEmpty() && QFileInfo::exists(lblPath)) {
            const Annotation ann = LabelIO::readYoloTxt(lblPath);
            const Annotation out = transformLabels(ann);
            LabelIO::writeYoloTxt(QString("%1/labels/%2.txt").arg(outDir, base), out);
        }
        ++done;
    }

    QMessageBox::information(this, "完成",
        QString("已处理 %1 张图片（%2）。\n图像：%3/images\n标签：%3/labels\n\n"
                "可用「数据集 → 打开项目」直接打开该目录。")
            .arg(done)
            .arg(batch ? "批量" : "当前图")
            .arg(outDir));
}
