#include "ProjectManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSet>
#include <QTextStream>

ProjectManager::ProjectManager(QObject *parent)
    : QObject(parent)
{
}

bool ProjectManager::createProject(const QString &parentDir, const QString &projectName)
{
    if (parentDir.isEmpty() || projectName.isEmpty())
        return false;

    const QString root = QDir(parentDir).filePath(projectName);
    QDir d;
    if (!d.mkpath(root + "/images") || !d.mkpath(root + "/labels"))
        return false;

    m_root = root;
    m_name = projectName;
    m_classes.clear();
    m_images.clear();
    saveProjectMeta();
    refreshImageCache();
    emit projectChanged();
    return true;
}

bool ProjectManager::openProject(const QString &projectDir)
{
    QDir d(projectDir);
    if (!d.exists())
        return false;
    if (!QFileInfo::exists(projectDir + "/images"))
        return false;

    m_root = QDir::cleanPath(projectDir);
    m_name = d.dirName();
    loadProjectMeta();
    refreshImageCache();
    emit projectChanged();
    return true;
}

void ProjectManager::closeProject()
{
    m_root.clear();
    m_name.clear();
    m_classes.clear();
    m_images.clear();
    emit projectChanged();
}

void ProjectManager::setClasses(const QStringList &classes)
{
    m_classes = classes;
    saveProjectMeta();
    emit projectChanged();
}

void ProjectManager::addClass(const QString &name)
{
    if (name.isEmpty())
        return;
    if (!m_classes.contains(name))
        m_classes.append(name);
    saveProjectMeta();
    emit projectChanged();
}

void ProjectManager::removeClass(int index)
{
    if (index < 0 || index >= m_classes.size())
        return;
    m_classes.removeAt(index);
    saveProjectMeta();
    emit projectChanged();
}

void ProjectManager::renameClass(int index, const QString &name)
{
    if (index < 0 || index >= m_classes.size() || name.isEmpty())
        return;
    m_classes[index] = name;
    saveProjectMeta();
    emit projectChanged();
}

QString ProjectManager::labelPathFor(const QString &imagePath) const
{
    const QFileInfo fi(imagePath);
    return labelsDir() + "/" + fi.completeBaseName() + ".txt";
}

bool ProjectManager::isLabeled(const QString &imagePath) const
{
    QFileInfo fi(labelPathFor(imagePath));
    return fi.exists() && fi.size() > 0;
}

int ProjectManager::labeledCount() const
{
    int n = 0;
    for (const QString &img : m_images)
        if (isLabeled(img))
            ++n;
    return n;
}

int ProjectManager::unlabeledCount() const
{
    return m_images.size() - labeledCount();
}

int ProjectManager::importImages(const QStringList &files)
{
    if (!isValid())
        return 0;

    int imported = 0;
    const QDir imagesDirObj(imagesDir());
    for (const QString &f : files) {
        if (!isImageFile(f))
            continue;
        const QFileInfo fi(f);
        const QString dest = imagesDirObj.filePath(fi.fileName());
        if (QFileInfo::exists(dest) && QFileInfo(dest) == QFileInfo(f)) {
            // 已在目标目录
            ++imported;
            continue;
        }
        if (QFile::copy(f, dest))
            ++imported;
    }
    refreshImageCache();
    emit projectChanged();
    return imported;
}

// 删除图片及其标注文件，并刷新图片缓存。返回是否成功。
bool ProjectManager::deleteImage(const QString &imagePath)
{
    if (!isValid() || imagePath.isEmpty())
        return false;
    bool ok = true;
    if (QFileInfo::exists(imagePath) && !QFile::remove(imagePath))
        ok = false;
    const QString lbl = labelPathFor(imagePath);
    if (QFileInfo::exists(lbl) && !QFile::remove(lbl))
        ok = false;
    refreshImageCache();
    emit projectChanged();
    return ok;
}

QString ProjectManager::generateDataYaml(double valRatio, bool includeNegatives)
{
    if (!isValid())
        return QString();

    // 已标注图片 + 可选负样本（未标注图片）
    QStringList labeled;
    QStringList negatives;
    for (const QString &img : m_images) {
        if (isLabeled(img))
            labeled.append(img);
        else if (includeNegatives)
            negatives.append(img);
    }

    // 打乱已标注图片并划分 train/val
    QStringList shuffled = labeled;
    for (int i = shuffled.size() - 1; i > 0; --i) {
        int j = QRandomGenerator::global()->bounded(i + 1);
        shuffled.swapItemsAt(i, j);
    }
    int valCount = 0;
    if (shuffled.size() > 1)
        valCount = qBound(1, (int)(shuffled.size() * valRatio), shuffled.size() - 1);
    const QStringList valList = shuffled.mid(0, valCount);
    QStringList trainList = shuffled.mid(valCount);
    // 负样本全部进训练集
    trainList.append(negatives);

    auto writeList = [](const QString &path, const QStringList &items) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return false;
        QTextStream out(&f);
        for (const QString &s : items)
            out << QDir::toNativeSeparators(s) << '\n';
        return true;
    };

    const QString trainTxt = m_root + "/train.txt";
    const QString valTxt = m_root + "/val.txt";
    writeList(trainTxt, trainList);
    writeList(valTxt, valList);

    // 写 data.yaml
    QFile fy(dataYamlPath());
    if (!fy.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return QString();
    QTextStream out(&fy);
    out << "# YoloStudio 自动生成\n";
    out << "train: " << QDir::toNativeSeparators(trainTxt) << "\n";
    out << "val: " << QDir::toNativeSeparators(valTxt) << "\n";
    out << "nc: " << m_classes.size() << "\n";
    out << "names: [";
    for (int i = 0; i < m_classes.size(); ++i) {
        if (i > 0)
            out << ", ";
        out << "'" << m_classes[i] << "'";
    }
    out << "]\n";
    return dataYamlPath();
}

bool ProjectManager::isImageFile(const QString &path)
{
    static const QSet<QString> exts = {
        "jpg", "jpeg", "png", "bmp", "webp", "tif", "tiff"
    };
    const QFileInfo fi(path);
    return exts.contains(fi.suffix().toLower());
}

void ProjectManager::refreshImageCache()
{
    m_images.clear();
    if (!isValid())
        return;
    const QDir d(imagesDir());
    const QFileInfoList list = d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &fi : list) {
        if (isImageFile(fi.filePath()))
            m_images.append(fi.absoluteFilePath());
    }
}

void ProjectManager::saveProjectMeta()
{
    if (!isValid())
        return;
    QJsonObject o;
    o["name"] = m_name;
    QJsonArray arr;
    for (const QString &c : m_classes)
        arr.append(c);
    o["classes"] = arr;

    QFile f(m_root + "/project.json");
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));

    // 同时写 classes.txt（YOLO 约定）
    QFile fc(classesPath());
    if (fc.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&fc);
        for (const QString &c : m_classes)
            out << c << '\n';
    }
}

void ProjectManager::loadProjectMeta()
{
    m_classes.clear();
    QFile f(m_root + "/project.json");
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        const QJsonObject o = doc.object();
        m_name = o.value("name").toString(m_name);
        const QJsonArray arr = o.value("classes").toArray();
        for (const QJsonValue &v : arr)
            m_classes.append(v.toString());
    }
    // 兜底：从 classes.txt 读
    if (m_classes.isEmpty()) {
        QFile fc(classesPath());
        if (fc.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&fc);
            while (!in.atEnd()) {
                const QString line = in.readLine().trimmed();
                if (!line.isEmpty())
                    m_classes.append(line);
            }
        }
    }
}
