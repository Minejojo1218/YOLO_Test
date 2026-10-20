#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// 管理一个标注/训练项目目录：
//   project/
//   ├── images/        图片
//   ├── labels/        YOLO 标注 txt
//   ├── classes.txt    类别（每行一个）
//   ├── project.json   项目元信息（名称、类别）
//   ├── data.yaml      训练数据描述（生成）
//   ├── train.txt / val.txt  划分清单（生成）
class ProjectManager : public QObject
{
    Q_OBJECT
public:
    explicit ProjectManager(QObject *parent = nullptr);

    bool createProject(const QString &parentDir, const QString &projectName);
    bool openProject(const QString &projectDir);
    void closeProject();
    bool isValid() const { return !m_root.isEmpty(); }

    QString rootDir() const { return m_root; }
    QString name() const { return m_name; }
    QString imagesDir() const { return m_root + "/images"; }
    QString labelsDir() const { return m_root + "/labels"; }
    QString dataYamlPath() const { return m_root + "/data.yaml"; }
    QString classesPath() const { return m_root + "/classes.txt"; }

    QStringList classes() const { return m_classes; }
    void setClasses(const QStringList &classes);
    void addClass(const QString &name);
    void removeClass(int index);
    void renameClass(int index, const QString &name);

    QStringList imageFiles() const { return m_images; }
    int imageCount() const { return m_images.size(); }
    QString labelPathFor(const QString &imagePath) const;
    bool isLabeled(const QString &imagePath) const;

    int labeledCount() const;
    int unlabeledCount() const;

    // 复制图片到 images/，返回成功数量
    int importImages(const QStringList &files);

    // 删除图片及其标注文件，返回是否成功
    bool deleteImage(const QString &imagePath);

    // 生成 train.txt/val.txt（按比例、可选把未标注图作为负样本）和 data.yaml，返回 data.yaml 路径
    QString generateDataYaml(double valRatio = 0.2, bool includeNegatives = false);

    static bool isImageFile(const QString &path);

signals:
    void projectChanged();

private:
    void refreshImageCache();
    void saveProjectMeta();
    void loadProjectMeta();

    QString m_root;
    QString m_name;
    QStringList m_classes;
    QStringList m_images;
};
