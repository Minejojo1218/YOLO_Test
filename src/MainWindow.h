#pragma once

#include <QMainWindow>

class ProjectManager;
class Database;
class DashboardPage;
class AnnotationPage;
class TrainingPage;
class ExportPage;
class ValidationPage;
class QListWidget;
class QStackedWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onNavChanged(int row);
    void applyTheme(const QString &theme);

private:
    void refreshAll();

    ProjectManager *m_project = nullptr;
    Database *m_db = nullptr;

    QListWidget *m_nav = nullptr;
    QStackedWidget *m_stack = nullptr;

    DashboardPage *m_dashboard = nullptr;
    AnnotationPage *m_annotation = nullptr;
    TrainingPage *m_training = nullptr;
    ExportPage *m_export = nullptr;
    ValidationPage *m_validation = nullptr;
};
