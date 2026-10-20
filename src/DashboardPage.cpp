#include "DashboardPage.h"
#include "Database.h"
#include "ProjectManager.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

DashboardPage::DashboardPage(ProjectManager *project, Database *db, QWidget *parent)
    : QWidget(parent)
    , m_project(project)
    , m_db(db)
{
    // ---------- 项目概括 ----------
    QGroupBox *overview = new QGroupBox("项目概括", this);
    QVBoxLayout *ovLay = new QVBoxLayout(overview);

    m_projectName = new QLabel("-", overview);
    m_imageCount = new QLabel("-", overview);
    m_classCount = new QLabel("-", overview);
    m_labeled = new QLabel("-", overview);
    m_unlabeled = new QLabel("-", overview);
    m_labelProgress = new QProgressBar(overview);
    m_labelProgress->setRange(0, 100);

    auto addRow = [&](const QString &title, QLabel *val) {
        QHBoxLayout *r = new QHBoxLayout();
        r->addWidget(new QLabel(title));
        r->addStretch(1);
        r->addWidget(val);
        ovLay->addLayout(r);
    };
    ovLay->addWidget(new QLabel("项目："));
    ovLay->addWidget(m_projectName);
    addRow("图片总数", m_imageCount);
    addRow("类别数量", m_classCount);
    addRow("已标注数量", m_labeled);
    addRow("待标注数量", m_unlabeled);
    ovLay->addWidget(new QLabel("标注完成率："));
    ovLay->addWidget(m_labelProgress);
    ovLay->addStretch(1);

    // ---------- 训练性能 ----------
    QGroupBox *perf = new QGroupBox("训练性能（最近一次）", this);
    QVBoxLayout *perfLay = new QVBoxLayout(perf);
    const char *metricTitles[4] = {"mAP@0.5", "mAP@0.5:0.95", "精确率 Precision", "召回率 Recall"};
    for (int i = 0; i < 4; ++i) {
        QVBoxLayout *col = new QVBoxLayout();
        m_metricNames[i] = new QLabel(metricTitles[i], perf);
        m_metricValues[i] = new QLabel("—", perf);
        m_metricValues[i]->setObjectName("metricValue");
        m_metricBars[i] = new QProgressBar(perf);
        m_metricBars[i]->setRange(0, 100);
        m_metricBars[i]->setValue(0);
        col->addWidget(m_metricNames[i]);
        col->addWidget(m_metricValues[i]);
        col->addWidget(m_metricBars[i]);
        perfLay->addLayout(col);
    }
    perfLay->addStretch(1);

    // ---------- 系统状态 ----------
    QGroupBox *sysBox = new QGroupBox("系统状态", this);
    QVBoxLayout *sysLay = new QVBoxLayout(sysBox);
    auto addSysRow = [&](const QString &title, QProgressBar *&bar, QLabel *&val) {
        QHBoxLayout *row = new QHBoxLayout();
        row->addWidget(new QLabel(title));
        bar = new QProgressBar(sysBox);
        bar->setRange(0, 100);
        bar->setValue(0);
        val = new QLabel("—", sysBox);
        val->setMinimumWidth(64);
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(bar, 1);
        row->addWidget(val);
        sysLay->addLayout(row);
    };
    addSysRow("CPU 使用率", m_cpuBar, m_cpuVal);
    addSysRow("内存 使用率", m_memBar, m_memVal);
    addSysRow("磁盘 使用率", m_diskBar, m_diskVal);
    QLabel *sysHint = new QLabel("提示：训练时可在「模型训练」页查看训练曲线与日志。", sysBox);
    sysHint->setWordWrap(true);
    sysLay->addWidget(sysHint);
    sysLay->addStretch(1);

    // ---------- 训练历史 ----------
    QGroupBox *history = new QGroupBox("训练历史", this);
    QVBoxLayout *hisLay = new QVBoxLayout(history);
    m_historyTable = new QTableWidget(0, 11, history);
    m_historyTable->setHorizontalHeaderLabels({
        "训练ID", "接入模型", "Epochs", "精度P", "召回率R",
        "mAP@0.5", "mAP@0.5:0.95", "训练时长", "结果模型", "状态", "时间"
    });
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_historyTable->horizontalHeader()->setStretchLastSection(true);
    m_historyTable->verticalHeader()->setVisible(false);
    m_historyTable->setAlternatingRowColors(true);
    hisLay->addWidget(m_historyTable, 1);
    connect(m_historyTable, &QTableWidget::customContextMenuRequested,
            this, &DashboardPage::onHistoryMenu);

    // ---------- 布局 ----------
    QHBoxLayout *top = new QHBoxLayout();
    top->addWidget(overview, 1);
    top->addWidget(perf, 1);
    top->addWidget(sysBox, 1);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->addLayout(top, 1);
    root->addWidget(history, 2);

    // ---------- 系统状态定时刷新 ----------
    m_sysTimer = new QTimer(this);
    m_sysTimer->setInterval(2000);
    connect(m_sysTimer, &QTimer::timeout, this, &DashboardPage::onSystemTick);
    m_sysTimer->start();
    onSystemTick();
}

void DashboardPage::onSystemTick()
{
#ifdef Q_OS_WIN
    // CPU：基于 GetSystemTimes 的差值计算
    FILETIME idleFt, kernelFt, userFt;
    if (GetSystemTimes(&idleFt, &kernelFt, &userFt)) {
        auto toU64 = [](const FILETIME &ft) {
            return (quint64(ft.dwHighDateTime) << 32) | quint64(ft.dwLowDateTime);
        };
        const quint64 idle = toU64(idleFt);
        const quint64 total = toU64(kernelFt) + toU64(userFt);
        if (m_prevTotal != 0 && total > m_prevTotal) {
            const quint64 dTotal = total - m_prevTotal;
            const quint64 dIdle = idle - m_prevIdle;
            const int usage = dTotal > 0
                ? int(100.0 * double(dTotal - dIdle) / double(dTotal))
                : 0;
            m_cpuBar->setValue(qBound(0, usage, 100));
            m_cpuVal->setText(QString("%1%").arg(qBound(0, usage, 100)));
        }
        m_prevIdle = idle;
        m_prevTotal = total;
    }

    // 内存
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        m_memBar->setValue(int(ms.dwMemoryLoad));
        const double usedGb = double(ms.ullTotalPhys - ms.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
        const double totalGb = double(ms.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
        m_memVal->setText(QString("%1%").arg(int(ms.dwMemoryLoad)));
        m_memVal->setToolTip(QString("%1 / %2 GB").arg(usedGb, 0, 'f', 1).arg(totalGb, 0, 'f', 1));
    }

    // 磁盘（项目所在盘，无项目时用 C 盘）
    QString root = "C:\\";
    if (m_project && m_project->isValid() && m_project->rootDir().size() >= 2
        && m_project->rootDir()[1] == ':')
        root = m_project->rootDir().left(3);
    ULARGE_INTEGER freeBytes, totalBytes, totalFree;
    if (GetDiskFreeSpaceExW((LPCWSTR)root.utf16(), &freeBytes, &totalBytes, &totalFree)
        && totalBytes.QuadPart > 0) {
        const int used = int(100.0 * double(totalBytes.QuadPart - totalFree.QuadPart)
                             / double(totalBytes.QuadPart));
        m_diskBar->setValue(qBound(0, used, 100));
        m_diskVal->setText(QString("%1%").arg(qBound(0, used, 100)));
    }
#endif
}

void DashboardPage::refresh()
{
    refreshOverview();
    refreshPerformance();
    refreshHistory();
}

void DashboardPage::refreshOverview()
{
    if (m_project && m_project->isValid()) {
        m_projectName->setText(m_project->name());
        const int total = m_project->imageCount();
        const int labeled = m_project->labeledCount();
        m_imageCount->setText(QString::number(total));
        m_classCount->setText(QString::number(m_project->classes().size()));
        m_labeled->setText(QString::number(labeled));
        m_unlabeled->setText(QString::number(total - labeled));
        m_labelProgress->setValue(total > 0 ? labeled * 100 / total : 0);
    } else {
        m_projectName->setText("未打开项目");
        m_imageCount->setText("-");
        m_classCount->setText("-");
        m_labeled->setText("-");
        m_unlabeled->setText("-");
        m_labelProgress->setValue(0);
    }
}

void DashboardPage::refreshPerformance()
{
    double map50 = 0, map5095 = 0, precision = 0, recall = 0;
    bool has = false;
    if (m_db && m_db->isOpen()) {
        const QList<TrainRecord> recs = m_db->records();
        if (!recs.isEmpty()) {
            const TrainRecord &r = recs.first();
            map50 = r.map50;
            map5095 = r.map5095;
            precision = r.precision;
            recall = r.recall;
            has = true;
        }
    }

    const double vals[4] = {map50, map5095, precision, recall};
    for (int i = 0; i < 4; ++i) {
        if (has) {
            m_metricValues[i]->setText(QString::number(vals[i], 'f', 4));
            m_metricBars[i]->setValue((int)(vals[i] * 100.0));
        } else {
            m_metricValues[i]->setText("—");
            m_metricBars[i]->setValue(0);
        }
    }
}

void DashboardPage::onHistoryMenu(const QPoint &pos)
{
    if (!m_db || !m_db->isOpen())
        return;
    const QModelIndex idx = m_historyTable->indexAt(pos);
    QMenu menu(this);
    QAction *del = nullptr;
    QAction *clear = menu.addAction("清空历史");
    if (idx.isValid()) {
        del = menu.addAction("删除该条记录");
        menu.insertAction(clear, del);
    }
    QAction *act = menu.exec(m_historyTable->viewport()->mapToGlobal(pos));
    if (act == del) {
        const int id = m_historyTable->item(idx.row(), 0)->text().toInt();
        const auto ret = QMessageBox::question(this, "删除记录", "确定删除该条训练记录吗？");
        if (ret == QMessageBox::Yes) {
            m_db->deleteRecord(id);
            refreshHistory();
            refreshPerformance();
        }
    } else if (act == clear) {
        const auto ret = QMessageBox::question(this, "清空历史", "确定清空全部训练历史吗？");
        if (ret == QMessageBox::Yes) {
            m_db->clear();
            refreshHistory();
            refreshPerformance();
        }
    }
}

void DashboardPage::refreshHistory()
{
    m_historyTable->setRowCount(0);
    if (!m_db || !m_db->isOpen())
        return;

    const QList<TrainRecord> recs = m_db->records();
    m_historyTable->setRowCount(recs.size());
    for (int i = 0; i < recs.size(); ++i) {
        const TrainRecord &r = recs[i];
        int c = 0;
        m_historyTable->setItem(i, c++, new QTableWidgetItem(QString::number(r.id)));
        m_historyTable->setItem(i, c++, new QTableWidgetItem(r.model));
        m_historyTable->setItem(i, c++, new QTableWidgetItem(QString::number(r.epochs)));
        m_historyTable->setItem(i, c++, new QTableWidgetItem(QString::number(r.precision, 'f', 4)));
        m_historyTable->setItem(i, c++, new QTableWidgetItem(QString::number(r.recall, 'f', 4)));
        m_historyTable->setItem(i, c++, new QTableWidgetItem(QString::number(r.map50, 'f', 4)));
        m_historyTable->setItem(i, c++, new QTableWidgetItem(QString::number(r.map5095, 'f', 4)));
        m_historyTable->setItem(i, c++, new QTableWidgetItem(r.duration));
        m_historyTable->setItem(i, c++, new QTableWidgetItem(r.resultModel));
        m_historyTable->setItem(i, c++, new QTableWidgetItem(r.status));
        m_historyTable->setItem(i, c++, new QTableWidgetItem(r.createdAt));
    }
}
