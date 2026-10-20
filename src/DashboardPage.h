#pragma once

#include <QWidget>

class ProjectManager;
class Database;
class QLabel;
class QProgressBar;
class QTableWidget;
class QTimer;

// 仪表盘：项目概括 / 训练性能 / 系统状态 + 训练历史
class DashboardPage : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardPage(ProjectManager *project, Database *db, QWidget *parent = nullptr);

    void refresh();

private slots:
    void onHistoryMenu(const QPoint &pos);
    void onSystemTick();

private:
    void refreshOverview();
    void refreshPerformance();
    void refreshHistory();

    ProjectManager *m_project;
    Database *m_db;

    // 项目概括
    QLabel *m_projectName = nullptr;
    QLabel *m_imageCount = nullptr;
    QLabel *m_classCount = nullptr;
    QLabel *m_labeled = nullptr;
    QLabel *m_unlabeled = nullptr;
    QProgressBar *m_labelProgress = nullptr;

    // 训练性能（4 项指标 + 进度条）
    QLabel *m_metricNames[4] = {nullptr, nullptr, nullptr, nullptr};
    QLabel *m_metricValues[4] = {nullptr, nullptr, nullptr, nullptr};
    QProgressBar *m_metricBars[4] = {nullptr, nullptr, nullptr, nullptr};

    // 系统状态
    QProgressBar *m_cpuBar = nullptr;
    QProgressBar *m_memBar = nullptr;
    QProgressBar *m_diskBar = nullptr;
    QLabel *m_cpuVal = nullptr;
    QLabel *m_memVal = nullptr;
    QLabel *m_diskVal = nullptr;
    QTimer *m_sysTimer = nullptr;
    quint64 m_prevIdle = 0;
    quint64 m_prevTotal = 0;

    // 训练历史
    QTableWidget *m_historyTable = nullptr;
};
