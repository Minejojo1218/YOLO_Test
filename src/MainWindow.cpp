#include "MainWindow.h"
#include "AnnotationPage.h"
#include "DashboardPage.h"
#include "Database.h"
#include "ExportPage.h"
#include "PreprocessDialog.h"
#include "ProjectManager.h"
#include "Theme.h"
#include "TrainingPage.h"
#include "ValidationPage.h"

#include <QAction>
#include <QActionGroup>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("YoloStudio - 标注训练一体化平台");
    resize(1440, 900);

    // ---------- 共享对象 ----------
    m_project = new ProjectManager(this);
    m_db = new Database(this);
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    m_db->setDbPath(dataDir + "/history.db");
    m_db->open();

    // ---------- 菜单栏 ----------
    QMenu *fileMenu = menuBar()->addMenu("文件");
    QAction *aQuit = fileMenu->addAction("退出");
    connect(aQuit, &QAction::triggered, this, &QWidget::close);

    QMenu *editMenu = menuBar()->addMenu("编辑");
    QMenu *themeMenu = editMenu->addMenu("主题");
    QActionGroup *themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    QAction *aDark = themeMenu->addAction("深色主题");
    aDark->setCheckable(true);
    themeGroup->addAction(aDark);
    QAction *aLight = themeMenu->addAction("浅色主题");
    aLight->setCheckable(true);
    themeGroup->addAction(aLight);
    QSettings settings;
    const QString savedTheme = settings.value("theme", "dark").toString();
    (savedTheme == "light" ? aLight : aDark)->setChecked(true);
    connect(aDark, &QAction::triggered, this, [this]() { applyTheme("dark"); });
    connect(aLight, &QAction::triggered, this, [this]() { applyTheme("light"); });

    QMenu *helpMenu = menuBar()->addMenu("帮助");
    QAction *aAbout = helpMenu->addAction("关于 YoloStudio");
    connect(aAbout, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "关于 YoloStudio",
                           "<b>YoloStudio</b> v1.0<br><br>"
                           "图像标注（框 / 多边形 / 自由绘制）→ 图像预处理 → YOLO 训练 → ONNX 导出 → 模型验证。<br><br>"
                           "技术栈：Qt 6.9.1 + OpenCV DNN + Ultralytics（SQLite 训练历史）");
    });

    // ---------- 工具菜单（把常用工具放到这里，避免工具栏折叠后找不到） ----------
    QMenu *toolsMenu = menuBar()->addMenu("工具");
    QAction *aPreprocess = toolsMenu->addAction("图像预处理...");
    connect(aPreprocess, &QAction::triggered, this, [this]() {
        if (!m_project || !m_project->isValid()) {
            QMessageBox::information(this, "提示",
                "请先在「数据集」页新建或打开项目（预处理以项目 images/ 目录为输入）。");
            return;
        }
        PreprocessDialog dlg(m_project, this);
        dlg.exec();
    });
    QAction *aCleanup = toolsMenu->addAction("数据清洗...");
    connect(aCleanup, &QAction::triggered, this, [this]() {
        m_nav->setCurrentRow(1);
        m_annotation->openCleanupDialog();
    });

    // ---------- 侧边导航 ----------
    QWidget *side = new QWidget(this);
    side->setObjectName("sideBar");
    side->setFixedWidth(210);
    QVBoxLayout *sideLay = new QVBoxLayout(side);
    sideLay->setContentsMargins(10, 16, 10, 12);
    sideLay->setSpacing(4);

    QLabel *logo = new QLabel("🎯 YoloStudio", side);
    logo->setObjectName("logoTitle");
    QLabel *logoSub = new QLabel("标注 · 训练 · 验证 一体化", side);
    logoSub->setObjectName("logoSub");
    sideLay->addWidget(logo);
    sideLay->addWidget(logoSub);
    sideLay->addSpacing(10);

    m_nav = new QListWidget(side);
    m_nav->setObjectName("navList");
    m_nav->setFocusPolicy(Qt::NoFocus);
    m_nav->addItems({"🏠  仪表盘", "🏷  数据集", "🚀  训练", "📦  导出", "🔍  推理 · 评估"});
    m_nav->setCurrentRow(0);
    sideLay->addWidget(m_nav, 1);

    // ---------- 页面 ----------
    m_dashboard = new DashboardPage(m_project, m_db, this);
    m_annotation = new AnnotationPage(m_project, this);
    m_training = new TrainingPage(m_project, m_db, this);
    m_export = new ExportPage(m_project, this);
    m_validation = new ValidationPage(m_project, this);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_dashboard);
    m_stack->addWidget(m_annotation);
    m_stack->addWidget(m_training);
    m_stack->addWidget(m_export);
    m_stack->addWidget(m_validation);

    connect(m_nav, &QListWidget::currentRowChanged, this, &MainWindow::onNavChanged);

    QWidget *central = new QWidget(this);
    QHBoxLayout *lay = new QHBoxLayout(central);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(side);
    lay->addWidget(m_stack, 1);
    setCentralWidget(central);

    statusBar()->showMessage("就绪", 3000);

    // 项目变化时刷新所有页面
    connect(m_project, &ProjectManager::projectChanged, this, &MainWindow::refreshAll);

    applyTheme(savedTheme);
    refreshAll();
}

MainWindow::~MainWindow() = default;

void MainWindow::applyTheme(const QString &theme)
{
    setStyleSheet(theme == "light" ? Theme::light() : Theme::dark());
    // 页面内部硬编码的深色小部件的背景色跟随主题
    const QString panel = (theme == "light") ? "#eef2fa" : "#14141e";
    const QString fg = (theme == "light") ? "#4a4f66" : "#9a9ab0";
    m_validation->setStyleSheet(
        QString("QLabel#imageLabel{background:%1;color:%2;}").arg(panel, fg));
    m_annotation->setStyleSheet(
        QString("QLabel#imageLabel{background:%1;color:%2;}").arg(panel, fg));
    QSettings settings;
    settings.setValue("theme", theme);
}

void MainWindow::onNavChanged(int row)
{
    m_stack->setCurrentIndex(row);
    QString name;
    switch (row) {
    case 0: m_dashboard->refresh(); name = "仪表盘"; break;
    case 1: m_annotation->refresh(); name = "数据集"; break;
    case 2: m_training->refresh(); name = "训练"; break;
    case 3: m_export->refresh(); name = "导出"; break;
    case 4: m_validation->refresh(); name = "推理 · 评估"; break;
    default: break;
    }
    statusBar()->showMessage("当前页面：" + name);
}

void MainWindow::refreshAll()
{
    m_dashboard->refresh();
    m_annotation->refresh();
    m_training->refresh();
    m_export->refresh();
    m_validation->refresh();
}
