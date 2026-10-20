#pragma once

#include "Annotation.h"

#include <QWidget>

class ProjectManager;
class AnnotationView;
class InferenceWorker;
class QListWidget;
class QComboBox;
class QLabel;
class QThread;

// 数据标注页：左图片列表 + 中画布 + 右类别管理（支持 AI 预标注、撤销/重做）
class AnnotationPage : public QWidget
{
    Q_OBJECT
public:
    explicit AnnotationPage(ProjectManager *project, QWidget *parent = nullptr);
    ~AnnotationPage();

    void refresh();
    // 供菜单栏调用：打开数据清洗对话框
    void openCleanupDialog();

private slots:
    void onNewProject();
    void onOpenProject();
    void onImportImages();
    void onImageSelected(int row);
    void onPrevImage();
    void onNextImage();
    void onSaveCurrent();
    void onAddClass();
    void onRenameClass();
    void onRemoveClass();
    void onCurrentClassChanged(int row);
    void onGenerateDataYaml();
    void onImageListMenu(const QPoint &pos);
    void onCleanupDataset();
    void onPrelabel();
    void onPrelabelModelLoaded(const QString &path, bool ok, const QString &error);
    void onPrelabelDone(const Annotation &labels, const QString &error);

private:
    void saveCurrentImage();
    void loadImage(int index);
    void updateClassWidgets();
    void updateStatus();
    void deleteImageAt(int row);
    QString findBestOnnx() const;

    ProjectManager *m_project;

    QListWidget *m_imageList = nullptr;
    AnnotationView *m_view = nullptr;
    QListWidget *m_classList = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_imageName = nullptr;

    int m_currentIndex = -1;

    // AI 辅助预标注（独立工作线程）
    QThread *m_preThread = nullptr;
    InferenceWorker *m_preWorker = nullptr;
    bool m_preModelReady = false;
    QString m_preModelPath;
    bool m_prelabelPending = false;
    QString m_prelabelImagePath;
    float m_prelabelConf = 0.25f;
};
