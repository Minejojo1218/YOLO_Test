#include "AnnotationPage.h"
#include "AnnotationView.h"
#include "InferenceWorker.h"
#include "LabelIO.h"
#include "PreprocessDialog.h"
#include "ProjectManager.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QTableWidget>
#include <QThread>
#include <QToolBar>
#include <QVBoxLayout>

#include <opencv2/opencv.hpp>

AnnotationPage::AnnotationPage(ProjectManager *project, QWidget *parent)
    : QWidget(parent)
    , m_project(project)
{
    // 允许 Annotation 跨线程传递（AI 预标注在工作线程完成）
    qRegisterMetaType<Annotation>("Annotation");

    // ---------- AI 预标注工作线程 ----------
    m_preThread = new QThread(this);
    m_preWorker = new InferenceWorker();
    m_preWorker->moveToThread(m_preThread);
    connect(m_preWorker, &InferenceWorker::modelLoaded, this, &AnnotationPage::onPrelabelModelLoaded);
    connect(m_preWorker, &InferenceWorker::prelabelDone, this, &AnnotationPage::onPrelabelDone);
    m_preThread->start();

    // ---------- 顶部工具栏 ----------
    QToolBar *tb = new QToolBar(this);
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextOnly);
    QAction *aNew = tb->addAction("新建项目");
    QAction *aOpen = tb->addAction("打开项目");
    QAction *aImport = tb->addAction("导入图片");
    tb->addSeparator();
    QAction *aPrev = tb->addAction("上一张 (A)");
    QAction *aNext = tb->addAction("下一张 (D)");
    QAction *aSave = tb->addAction("保存 (Ctrl+S)");
    tb->addSeparator();
    QAction *aBoxMode = tb->addAction("框标注");
    QAction *aPolyMode = tb->addAction("多边形标注");
    QAction *aFreeMode = tb->addAction("自由绘制");
    tb->addSeparator();
    QAction *aPrelabel = tb->addAction("AI 预标注");
    tb->addSeparator();
    QAction *aPre = tb->addAction("图像预处理");
    QAction *aClean = tb->addAction("数据清洗");
    QAction *aYaml = tb->addAction("生成 data.yaml");
    aPre->setToolTip("图像预处理：镜像 / 旋转 / 亮度 / 对比度 / 反色 / 灰度 / 模糊 / 锐化（批量，输出到独立目录）");
    aClean->setToolTip("批量检查标注状态，可删除异常图片");
    aYaml->setToolTip("生成 data.yaml 训练划分");
    connect(aNew, &QAction::triggered, this, &AnnotationPage::onNewProject);
    connect(aOpen, &QAction::triggered, this, &AnnotationPage::onOpenProject);
    connect(aImport, &QAction::triggered, this, &AnnotationPage::onImportImages);
    connect(aPrev, &QAction::triggered, this, &AnnotationPage::onPrevImage);
    connect(aNext, &QAction::triggered, this, &AnnotationPage::onNextImage);
    connect(aSave, &QAction::triggered, this, &AnnotationPage::onSaveCurrent);
    connect(aPrelabel, &QAction::triggered, this, &AnnotationPage::onPrelabel);
    connect(aYaml, &QAction::triggered, this, &AnnotationPage::onGenerateDataYaml);
    connect(aClean, &QAction::triggered, this, &AnnotationPage::onCleanupDataset);
    connect(aPre, &QAction::triggered, this, [this]() {
        if (!m_project || !m_project->isValid()) {
            QMessageBox::information(this, "提示", "请先新建或打开项目。");
            return;
        }
        PreprocessDialog dlg(m_project, this);
        dlg.exec();
    });

    // 标注模式切换（框 / 多边形 / 自由绘制，互斥）
    aBoxMode->setCheckable(true);
    aBoxMode->setChecked(true);
    aPolyMode->setCheckable(true);
    aFreeMode->setCheckable(true);
    auto switchMode = [this](QAction *self, QAction *other1, QAction *other2, AnnoMode mode,
                             const QString &hint) {
        self->setChecked(true);
        other1->setChecked(false);
        other2->setChecked(false);
        m_view->setAnnotationMode(mode);
        m_status->setText(hint);
    };
    connect(aBoxMode, &QAction::triggered, this, [this, aBoxMode, aPolyMode, aFreeMode, switchMode]() {
        switchMode(aBoxMode, aPolyMode, aFreeMode, AnnoMode::Box,
                   "框标注：拖拽画框；点击框可移动，右键改类别/删除");
    });
    connect(aPolyMode, &QAction::triggered, this, [this, aBoxMode, aPolyMode, aFreeMode, switchMode]() {
        switchMode(aPolyMode, aBoxMode, aFreeMode, AnnoMode::Polygon,
                   "多边形标注：单击加点，双击/右键/回车闭合，Esc 取消；拖动顶点或整体可编辑");
    });
    connect(aFreeMode, &QAction::triggered, this, [this, aBoxMode, aPolyMode, aFreeMode, switchMode]() {
        switchMode(aFreeMode, aBoxMode, aPolyMode, AnnoMode::Freehand,
                   "自由绘制：按住左键沿缺陷边缘描一圈，松手自动简化成多边形；可再拖顶点微调");
    });

    // ---------- 左侧图片列表 ----------
    m_imageList = new QListWidget(this);
    m_imageList->setFixedWidth(230);
    m_imageList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_imageList, &QListWidget::currentRowChanged, this, &AnnotationPage::onImageSelected);
    connect(m_imageList, &QListWidget::customContextMenuRequested, this, &AnnotationPage::onImageListMenu);

    // ---------- 中间画布 ----------
    m_view = new AnnotationView(this);
    m_view->setMinimumSize(400, 300);
    m_view->setFocusPolicy(Qt::StrongFocus);

    m_imageName = new QLabel("未加载图片", this);
    m_status = new QLabel("", this);

    QWidget *center = new QWidget(this);
    QVBoxLayout *centerLay = new QVBoxLayout(center);
    centerLay->setContentsMargins(0, 0, 0, 0);
    centerLay->addWidget(m_view, 1);
    QHBoxLayout *statusLay = new QHBoxLayout();
    statusLay->addWidget(m_imageName);
    statusLay->addStretch(1);
    statusLay->addWidget(m_status);
    centerLay->addLayout(statusLay);

    // ---------- 右侧类别管理 ----------
    QWidget *right = new QWidget(this);
    right->setFixedWidth(200);
    QVBoxLayout *rightLay = new QVBoxLayout(right);
    rightLay->addWidget(new QLabel("类别（点击选择当前绘制类别）"));
    m_classList = new QListWidget(right);
    m_classList->setSelectionMode(QAbstractItemView::SingleSelection);
    rightLay->addWidget(m_classList, 1);
    connect(m_classList, &QListWidget::currentRowChanged, this, &AnnotationPage::onCurrentClassChanged);

    QPushButton *bAdd = new QPushButton("添加类别", right);
    QPushButton *bRename = new QPushButton("重命名", right);
    QPushButton *bRemove = new QPushButton("删除类别", right);
    rightLay->addWidget(bAdd);
    rightLay->addWidget(bRename);
    rightLay->addWidget(bRemove);
    connect(bAdd, &QPushButton::clicked, this, &AnnotationPage::onAddClass);
    connect(bRename, &QPushButton::clicked, this, &AnnotationPage::onRenameClass);
    connect(bRemove, &QPushButton::clicked, this, &AnnotationPage::onRemoveClass);

    // ---------- 整体布局 ----------
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_imageList);
    splitter->addWidget(center);
    splitter->addWidget(right);
    splitter->setStretchFactor(1, 1);

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(tb);
    lay->addWidget(splitter, 1);

    // 快捷键
    auto *sPrev = new QShortcut(QKeySequence("A"), this);
    connect(sPrev, &QShortcut::activated, this, &AnnotationPage::onPrevImage);
    auto *sNext = new QShortcut(QKeySequence("D"), this);
    connect(sNext, &QShortcut::activated, this, &AnnotationPage::onNextImage);
    auto *sSave = new QShortcut(QKeySequence("Ctrl+S"), this);
    connect(sSave, &QShortcut::activated, this, &AnnotationPage::onSaveCurrent);
    // 撤销 / 重做 / 下一张（Tab）
    auto *sUndo = new QShortcut(QKeySequence::Undo, this);
    connect(sUndo, &QShortcut::activated, this, [this]() { m_view->undo(); });
    auto *sRedo = new QShortcut(QKeySequence::Redo, this);
    connect(sRedo, &QShortcut::activated, this, [this]() { m_view->redo(); });
    auto *sTab = new QShortcut(QKeySequence(Qt::Key_Tab), this);
    connect(sTab, &QShortcut::activated, this, &AnnotationPage::onNextImage);
    for (int i = 1; i <= 9; ++i) {
        auto *sc = new QShortcut(QKeySequence(QString::number(i)), this);
        const int idx = i - 1;
        connect(sc, &QShortcut::activated, this, [this, idx]() {
            if (idx < m_classList->count()) {
                m_classList->setCurrentRow(idx);
                onCurrentClassChanged(idx);
            }
        });
    }

    // 画布改动标记
    connect(m_view, &AnnotationView::annotationChanged, this, [this]() {
        m_status->setText("● 已修改，切换图片或按 Ctrl+S 保存");
    });

    updateStatus();
}

void AnnotationPage::openCleanupDialog()
{
    onCleanupDataset();
}

void AnnotationPage::refresh()
{
    // 重载图片列表与类别
    const bool hasProject = m_project && m_project->isValid();
    m_imageList->clear();
    m_currentIndex = -1;
    m_view->setImage(QImage());
    if (hasProject) {
        const QStringList imgs = m_project->imageFiles();
        for (const QString &img : imgs) {
            const QString name = QFileInfo(img).fileName();
            QListWidgetItem *item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, img);
            if (m_project->isLabeled(img))
                item->setText(name + "  ✓");
            m_imageList->addItem(item);
        }
    }
    updateClassWidgets();
    updateStatus();
}

void AnnotationPage::onNewProject()
{
    QSettings s;
    const QString lastDir = s.value("lastProjectDir").toString();
    const QString dir = QFileDialog::getExistingDirectory(this, "选择项目存放目录",
                                                          lastDir.isEmpty() ? m_project->rootDir() : lastDir);
    if (dir.isEmpty())
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, "新建项目", "项目名称：", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty())
        return;
    if (m_project->createProject(dir, name)) {
        s.setValue("lastProjectDir", m_project->rootDir());
        refresh();
    }
}

void AnnotationPage::onOpenProject()
{
    QSettings s;
    const QString lastDir = s.value("lastProjectDir").toString();
    const QString dir = QFileDialog::getExistingDirectory(this, "选择项目目录",
                                                          lastDir.isEmpty() ? m_project->rootDir() : lastDir);
    if (dir.isEmpty())
        return;
    if (m_project->openProject(dir)) {
        s.setValue("lastProjectDir", m_project->rootDir());
        refresh();
    } else {
        QMessageBox::warning(this, "打开失败", "所选目录不是有效的项目目录（需包含 images 子目录）。");
    }
}

void AnnotationPage::onImportImages()
{
    if (!m_project->isValid()) {
        QMessageBox::information(this, "提示", "请先新建或打开项目。");
        return;
    }
    const QStringList files = QFileDialog::getOpenFileNames(
        this, "选择图片",
        QString(),
        "图片 (*.jpg *.jpeg *.png *.bmp *.webp *.tif *.tiff)");
    if (files.isEmpty())
        return;
    const int n = m_project->importImages(files);
    refresh();
    QMessageBox::information(this, "导入完成", QString("成功导入 %1 张图片。").arg(n));
}

void AnnotationPage::onImageSelected(int row)
{
    if (row < 0)
        return;
    saveCurrentImage();
    loadImage(row);
}

void AnnotationPage::onPrevImage()
{
    if (m_imageList->count() == 0)
        return;
    const int next = (m_currentIndex - 1 + m_imageList->count()) % m_imageList->count();
    m_imageList->setCurrentRow(next);
    onImageSelected(next);
}

void AnnotationPage::onNextImage()
{
    if (m_imageList->count() == 0)
        return;
    const int next = (m_currentIndex + 1) % m_imageList->count();
    m_imageList->setCurrentRow(next);
    onImageSelected(next);
}

void AnnotationPage::onSaveCurrent()
{
    saveCurrentImage();
    m_status->setText("已保存");
}

void AnnotationPage::saveCurrentImage()
{
    if (!m_project || !m_project->isValid() || m_currentIndex < 0)
        return;
    if (m_currentIndex >= m_project->imageFiles().size())
        return;
    const QString imgPath = m_project->imageFiles()[m_currentIndex];
    const QString labelPath = m_project->labelPathFor(imgPath);
    const Annotation ann = m_view->annotations();
    LabelIO::writeYoloTxt(labelPath, ann);

    // 更新列表标记
    QListWidgetItem *item = m_imageList->item(m_currentIndex);
    if (item) {
        const QString base = QFileInfo(imgPath).fileName();
        item->setText(ann.isEmpty() ? base : base + "  ✓");
    }
}

void AnnotationPage::loadImage(int index)
{
    if (!m_project || !m_project->isValid())
        return;
    const QStringList imgs = m_project->imageFiles();
    if (index < 0 || index >= imgs.size())
        return;
    m_currentIndex = index;
    const QString imgPath = imgs[index];
    const QImage img(imgPath);
    if (img.isNull()) {
        m_status->setText("图片加载失败");
        return;
    }
    m_view->setClasses(m_project->classes());
    m_view->setImage(img);
    m_view->setAnnotations(LabelIO::readYoloTxt(m_project->labelPathFor(imgPath)));
    m_imageName->setText(QFileInfo(imgPath).fileName());
    updateStatus();
}

void AnnotationPage::updateClassWidgets()
{
    const QStringList classes = m_project ? m_project->classes() : QStringList();
    const int cur = m_classList->currentRow();
    m_classList->blockSignals(true);
    m_classList->clear();
    for (const QString &c : classes)
        m_classList->addItem(c);
    if (cur >= 0 && cur < m_classList->count())
        m_classList->setCurrentRow(cur);
    else if (m_classList->count() > 0)
        m_classList->setCurrentRow(0);
    m_classList->blockSignals(false);
    m_view->setClasses(classes);
    if (m_classList->currentRow() >= 0)
        m_view->setCurrentClassId(m_classList->currentRow());
}

void AnnotationPage::onCurrentClassChanged(int row)
{
    if (row < 0)
        return;
    m_view->setCurrentClassId(row);
}

void AnnotationPage::onAddClass()
{
    if (!m_project->isValid()) {
        QMessageBox::information(this, "提示", "请先新建或打开项目。");
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, "添加类别", "类别名称：", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    m_project->addClass(name.trimmed());
    updateClassWidgets();
}

void AnnotationPage::onRenameClass()
{
    const int row = m_classList->currentRow();
    if (row < 0)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, "重命名类别", "新名称：",
                                               QLineEdit::Normal, m_classList->item(row)->text(), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    m_project->renameClass(row, name.trimmed());
    updateClassWidgets();
}

void AnnotationPage::onRemoveClass()
{
    const int row = m_classList->currentRow();
    if (row < 0)
        return;
    m_project->removeClass(row);
    updateClassWidgets();
}

void AnnotationPage::onGenerateDataYaml()
{
    if (!m_project->isValid()) {
        QMessageBox::information(this, "提示", "请先新建或打开项目。");
        return;
    }
    const QString path = m_project->generateDataYaml(0.2);
    if (path.isEmpty()) {
        QMessageBox::warning(this, "失败", "生成 data.yaml 失败。");
        return;
    }
    QMessageBox::information(this, "完成",
        QString("已生成训练数据配置：\n%1\n\n已标注图片：%2 张\n请到「模型训练」页开始训练。")
            .arg(path)
            .arg(m_project->labeledCount()));
}

// 图片列表右键菜单：删除图片（含标注）
void AnnotationPage::onImageListMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_imageList->itemAt(pos);
    if (!item || !m_project || !m_project->isValid())
        return;
    const int row = m_imageList->row(item);
    QMenu menu(this);
    QAction *del = menu.addAction("删除图片（含标注）");
    QAction *clean = menu.addAction("数据清洗...");
    QAction *act = menu.exec(m_imageList->viewport()->mapToGlobal(pos));
    if (act == del) {
        deleteImageAt(row);
    } else if (act == clean) {
        onCleanupDataset();
    }
}

// 删除指定行的图片（先保存当前，再删文件，再刷新列表）
void AnnotationPage::deleteImageAt(int row)
{
    if (!m_project || !m_project->isValid())
        return;
    const QStringList imgs = m_project->imageFiles();
    if (row < 0 || row >= imgs.size())
        return;
    saveCurrentImage();
    const QString path = imgs[row];
    const QString name = QFileInfo(path).fileName();
    const auto ret = QMessageBox::question(this, "删除图片",
        QString("确定删除图片「%1」及其标注文件吗？\n该操作不可恢复。").arg(name));
    if (ret != QMessageBox::Yes)
        return;
    m_project->deleteImage(path);
    refresh();
    // 尽量定位到删除位置附近的图片
    const int total = m_imageList->count();
    if (total > 0) {
        const int next = qBound(0, row, total - 1);
        m_imageList->setCurrentRow(next);
        onImageSelected(next);
    }
    updateStatus();
}

// 数据清洗：列出全部图片与标注状态，支持批量删除、修复 yaml 划分
void AnnotationPage::onCleanupDataset()
{
    if (!m_project || !m_project->isValid()) {
        QMessageBox::information(this, "提示", "请先新建或打开项目。");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("数据清洗");
    dlg.resize(620, 460);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);

    QLabel *summaryLabel = new QLabel(&dlg);
    summaryLabel->setWordWrap(true);
    summaryLabel->setText("正在扫描...");
    lay->addWidget(summaryLabel);

    auto *table = new QTableWidget(&dlg);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({"删除", "图片", "状态"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    lay->addWidget(table, 1);

    // 状态按钮行
    QHBoxLayout *btnLay = new QHBoxLayout();
    QPushButton *bBad = new QPushButton("全选异常", &dlg);
    QPushButton *bAll = new QPushButton("全选", &dlg);
    QPushButton *bDel = new QPushButton("删除所选", &dlg);
    QPushButton *bYaml = new QPushButton("重新生成 data.yaml", &dlg);
    QPushButton *bClose = new QPushButton("关闭", &dlg);
    btnLay->addWidget(bBad);
    btnLay->addWidget(bAll);
    btnLay->addWidget(bDel);
    btnLay->addWidget(bYaml);
    btnLay->addStretch(1);
    btnLay->addWidget(bClose);
    lay->addLayout(btnLay);

    // 填充数据
    const QStringList imgs = m_project->imageFiles();
    const QStringList classes = m_project->classes();
    QVector<bool> checked(imgs.size(), false);
    QVector<bool> bad(imgs.size(), false);

    auto refill = [&]() {
        QVector<int> classCount(qMax(1, classes.size()), 0);
        int corruptCount = 0;
        table->setRowCount(imgs.size());
        for (int i = 0; i < imgs.size(); ++i) {
            QStringList issues;
            // 1) 图片完整性
            cv::Mat probe = cv::imread(imgs[i].toLocal8Bit().constData(), cv::IMREAD_REDUCED_COLOR_8);
            if (probe.empty()) {
                issues << "⚠ 图片损坏/无法读取";
                ++corruptCount;
            }
            // 2) 标签
            const QString lblPath = m_project->labelPathFor(imgs[i]);
            QString status;
            if (QFileInfo::exists(lblPath)) {
                const Annotation ann = LabelIO::readYoloTxt(lblPath);
                if (ann.isEmpty()) {
                    issues << "⚠ 标签异常（文件存在但无法解析）";
                } else {
                    status = QString("已标注 %1 个").arg(ann.boxes.size() + ann.polygons.size());
                    // 类别越界 + 坐标越界 + 类别计数
                    bool clsBad = false, coordBad = false;
                    for (const LabelBox &b : ann.boxes) {
                        if (b.classId < 0 || b.classId >= classes.size()) clsBad = true;
                        else classCount[b.classId]++;
                        if (b.x < 0 || b.x > 1 || b.y < 0 || b.y > 1 || b.w <= 0 || b.w > 1
                            || b.h <= 0 || b.h > 1)
                            coordBad = true;
                    }
                    for (const LabelPolygon &p : ann.polygons) {
                        if (p.classId < 0 || p.classId >= classes.size()) clsBad = true;
                        else classCount[p.classId]++;
                        for (const QPointF &pt : p.points)
                            if (pt.x() < 0 || pt.x() > 1 || pt.y() < 0 || pt.y() > 1) {
                                coordBad = true;
                                break;
                            }
                    }
                    if (clsBad)
                        issues << "⚠ 类别越界";
                    if (coordBad)
                        issues << "⚠ 坐标越界";
                }
            } else {
                status = "未标注";
            }

            bad[i] = !issues.isEmpty();
            if (!issues.isEmpty())
                status = issues.join(" / ");
            else if (status.isEmpty())
                status = "已标注";

            auto *chk = new QTableWidgetItem();
            chk->setCheckState(checked[i] ? Qt::Checked : Qt::Unchecked);
            table->setItem(i, 0, chk);
            table->setItem(i, 1, new QTableWidgetItem(QFileInfo(imgs[i]).fileName()));
            QTableWidgetItem *st = new QTableWidgetItem(status);
            if (bad[i])
                st->setForeground(Qt::red);
            table->setItem(i, 2, st);
        }
        table->resizeColumnToContents(0);

        // 类别分布统计（检测类别不平衡）
        QStringList dist;
        for (int c = 0; c < classes.size(); ++c)
            dist << QString("%1×%2").arg(classes[c]).arg(classCount[c]);
        QString summary = QString("共 %1 张，异常 %2 张，损坏 %3 张")
                              .arg(imgs.size())
                              .arg([&]() { int n = 0; for (bool b : bad) n += b ? 1 : 0; return n; }())
                              .arg(corruptCount);
        if (!dist.isEmpty())
            summary += "\n类别分布：" + dist.join("  ");
        if (summaryLabel)
            summaryLabel->setText(summary);
    };
    refill();

    connect(bBad, &QPushButton::clicked, &dlg, [&]() {
        for (int i = 0; i < imgs.size(); ++i) {
            checked[i] = bad[i];
            if (table->item(i, 0))
                table->item(i, 0)->setCheckState(bad[i] ? Qt::Checked : Qt::Unchecked);
        }
    });
    connect(bAll, &QPushButton::clicked, &dlg, [&]() {
        for (int i = 0; i < imgs.size(); ++i) {
            checked[i] = true;
            if (table->item(i, 0))
                table->item(i, 0)->setCheckState(Qt::Checked);
        }
    });
    connect(bDel, &QPushButton::clicked, &dlg, [&]() {
        QVector<int> toDelete;
        for (int i = 0; i < imgs.size(); ++i)
            if (checked[i])
                toDelete.append(i);
        if (toDelete.isEmpty()) {
            QMessageBox::information(&dlg, "提示", "请先勾选要删除的图片。");
            return;
        }
        const auto ret = QMessageBox::question(&dlg, "删除图片",
            QString("确定删除选中的 %1 张图片及其标注吗？\n该操作不可恢复。").arg(toDelete.size()));
        if (ret != QMessageBox::Yes)
            return;
        // 从后往前删，避免下标变化
        for (int i = toDelete.size() - 1; i >= 0; --i)
            m_project->deleteImage(imgs[toDelete[i]]);
        QMessageBox::information(&dlg, "完成", QString("已删除 %1 张图片。\n建议点击「重新生成 data.yaml」刷新训练划分。").arg(toDelete.size()));
        // 关闭对话框并刷新页面
        dlg.accept();
        refresh();
        updateStatus();
    });
    connect(bYaml, &QPushButton::clicked, &dlg, [&]() {
        const QString path = m_project->generateDataYaml(0.2);
        if (path.isEmpty())
            QMessageBox::warning(&dlg, "失败", "生成 data.yaml 失败。");
        else
            QMessageBox::information(&dlg, "完成", "已重新生成训练划分与 data.yaml：\n" + path);
    });
    connect(bClose, &QPushButton::clicked, &dlg, &QDialog::accept);

    dlg.exec();
}

void AnnotationPage::updateStatus()
{
    if (!m_project || !m_project->isValid()) {
        m_status->setText("未打开项目");
        return;
    }
    const int total = m_project->imageCount();
    const int labeled = m_project->labeledCount();
    m_status->setText(QString("共 %1 张 / 已标注 %2 张 / 待标注 %3 张")
                          .arg(total).arg(labeled).arg(total - labeled));
}

// ---------- AI 辅助预标注 ----------

QString AnnotationPage::findBestOnnx() const
{
    if (!m_project || !m_project->isValid())
        return QString();
    QDirIterator it(m_project->rootDir() + "/runs", QStringList() << "best.pt",
                    QDir::Files, QDirIterator::Subdirectories);
    QString best;
    QDateTime newest;
    while (it.hasNext()) {
        const QString p = it.next();
        const QDateTime t = QFileInfo(p).lastModified();
        if (best.isEmpty() || t > newest) {
            best = p;
            newest = t;
        }
    }
    if (!best.isEmpty()) {
        const QString onnx = best.left(best.lastIndexOf('.')) + ".onnx";
        if (QFileInfo::exists(onnx))
            return onnx;
    }
    return QString();
}

// AI 预标注：选择 ONNX 模型 + 置信度，对当前图片推理并追加标注
void AnnotationPage::onPrelabel()
{
    if (!m_project || !m_project->isValid()) {
        QMessageBox::information(this, "提示", "请先新建或打开项目。");
        return;
    }
    if (m_currentIndex < 0) {
        QMessageBox::information(this, "提示", "请先在左侧选择要标注的图片。");
        return;
    }
    if (m_prelabelPending) {
        m_status->setText("AI 预标注进行中，请稍候...");
        return;
    }

    QSettings s;
    QString modelPath = s.value("prelabelModel").toString();
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath))
        modelPath = findBestOnnx();

    QDialog dlg(this);
    dlg.setWindowTitle("AI 预标注");
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QLabel *hint = new QLabel(
        "用已导出的 ONNX 模型对当前图片自动生成标注：\n"
        "· 检测模型 → 生成矩形框\n"
        "· 分割模型 → 生成缺陷轮廓多边形\n"
        "结果会追加到当前标注，可用 Ctrl+Z 撤销。\n"
        "注意：模型类别顺序需与当前项目类别一致。", &dlg);
    hint->setWordWrap(true);
    lay->addWidget(hint);

    QFormLayout *form = new QFormLayout();
    QLineEdit *pathEdit = new QLineEdit(modelPath, &dlg);
    QPushButton *bBrowse = new QPushButton("浏览...", &dlg);
    QHBoxLayout *hl = new QHBoxLayout();
    hl->addWidget(pathEdit, 1);
    hl->addWidget(bBrowse);
    form->addRow("ONNX 模型", hl);
    QDoubleSpinBox *confSpin = new QDoubleSpinBox(&dlg);
    confSpin->setRange(0.01, 1.0);
    confSpin->setValue(0.25);
    confSpin->setSingleStep(0.05);
    confSpin->setDecimals(2);
    form->addRow("置信度", confSpin);
    lay->addLayout(form);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(bBrowse, &QPushButton::clicked, &dlg, [&]() {
        const QString p = QFileDialog::getOpenFileName(
            &dlg, "选择 ONNX 模型", QFileInfo(pathEdit->text()).absolutePath(),
            "ONNX 模型 (*.onnx)");
        if (!p.isEmpty())
            pathEdit->setText(p);
    });
    if (dlg.exec() != QDialog::Accepted)
        return;

    modelPath = pathEdit->text().trimmed();
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        QMessageBox::warning(this, "提示", "请选择有效的 ONNX 模型文件。");
        return;
    }
    s.setValue("prelabelModel", modelPath);

    m_prelabelImagePath = m_project->imageFiles()[m_currentIndex];
    m_prelabelConf = (float)confSpin->value();
    m_prelabelPending = true;
    if (m_preModelReady && m_preModelPath == modelPath) {
        m_prelabelPending = false;
        m_status->setText("AI 预标注中...");
        QMetaObject::invokeMethod(m_preWorker, "prelabelImage", Qt::QueuedConnection,
                                  Q_ARG(QString, m_prelabelImagePath),
                                  Q_ARG(float, m_prelabelConf));
    } else {
        m_preModelReady = false;
        m_preModelPath = modelPath;
        m_status->setText("正在加载模型...");
        QMetaObject::invokeMethod(m_preWorker, "loadModel", Qt::QueuedConnection,
                                  Q_ARG(QString, modelPath));
    }
}

void AnnotationPage::onPrelabelModelLoaded(const QString &path, bool ok, const QString &error)
{
    if (!ok) {
        m_preModelReady = false;
        m_prelabelPending = false;
        m_status->setText("模型加载失败：" + error);
        return;
    }
    m_preModelReady = true;
    m_preModelPath = path;
    if (m_prelabelPending) {
        m_prelabelPending = false;
        m_status->setText("AI 预标注中...");
        QMetaObject::invokeMethod(m_preWorker, "prelabelImage", Qt::QueuedConnection,
                                  Q_ARG(QString, m_prelabelImagePath),
                                  Q_ARG(float, m_prelabelConf));
    }
}

void AnnotationPage::onPrelabelDone(const Annotation &labels, const QString &error)
{
    if (!error.isEmpty()) {
        m_status->setText("AI 预标注失败：" + error);
        return;
    }
    if (labels.isEmpty()) {
        m_status->setText("AI 预标注：未检测到目标（可降低置信度后重试）");
        return;
    }
    m_view->appendAnnotations(labels);
    m_status->setText(QString("AI 预标注完成：%1 个框 / %2 个多边形（Ctrl+Z 可撤销）")
                          .arg(labels.boxes.size()).arg(labels.polygons.size()));
}

AnnotationPage::~AnnotationPage()
{
    if (m_preThread) {
        m_preWorker->abort();
        m_preThread->quit();
        m_preThread->wait(3000);
        delete m_preWorker;
        m_preWorker = nullptr;
    }
}
