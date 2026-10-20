#pragma once

#include <QList>
#include <QObject>
#include <QString>

struct TrainRecord
{
    int     id = 0;
    QString model;         // 接入模型
    int     epochs = 0;
    double  precision = 0;
    double  recall = 0;
    double  map50 = 0;
    double  map5095 = 0;
    QString duration;      // 训练时长
    QString resultModel;   // 结果模型路径
    QString status;        // 成功/失败/进行中
    QString createdAt;     // 时间
};

// SQLite 存储训练历史
class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QObject *parent = nullptr);
    ~Database();

    void setDbPath(const QString &path);
    QString dbPath() const;

    bool open();               // 打开并建表
    bool isOpen() const;

    int  addRecord(const TrainRecord &r);   // 返回新记录 id
    QList<TrainRecord> records() const;     // 按时间倒序
    bool deleteRecord(int id);              // 删除单条记录
    void clear();

private:
    bool createTables();
    QString m_dbPath;
};
