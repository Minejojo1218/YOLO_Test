#include "Database.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static const char *kConnName = "yolo_studio_conn";

Database::Database(QObject *parent)
    : QObject(parent)
{
}

Database::~Database()
{
    if (QSqlDatabase::contains(kConnName)) {
        QSqlDatabase::database(kConnName).close();
        QSqlDatabase::removeDatabase(kConnName);
    }
}

void Database::setDbPath(const QString &path)
{
    m_dbPath = path;
}

QString Database::dbPath() const
{
    return m_dbPath;
}

bool Database::open()
{
    QSqlDatabase db;
    if (QSqlDatabase::contains(kConnName)) {
        db = QSqlDatabase::database(kConnName);
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", kConnName);
    }
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        return false;
    }
    return createTables();
}

bool Database::isOpen() const
{
    if (!QSqlDatabase::contains(kConnName))
        return false;
    return QSqlDatabase::database(kConnName).isOpen();
}

bool Database::createTables()
{
    QSqlQuery q(QSqlDatabase::database(kConnName));
    return q.exec(
        "CREATE TABLE IF NOT EXISTS train_history ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " model TEXT,"
        " epochs INTEGER,"
        " precision REAL,"
        " recall REAL,"
        " map50 REAL,"
        " map5095 REAL,"
        " duration TEXT,"
        " result_model TEXT,"
        " status TEXT,"
        " created_at TEXT"
        ")"
    );
}

int Database::addRecord(const TrainRecord &r)
{
    if (!isOpen())
        return -1;
    QSqlQuery q(QSqlDatabase::database(kConnName));
    q.prepare(
        "INSERT INTO train_history"
        " (model, epochs, precision, recall, map50, map5095, duration, result_model, status, created_at)"
        " VALUES (?,?,?,?,?,?,?,?,?,?)"
    );
    q.addBindValue(r.model);
    q.addBindValue(r.epochs);
    q.addBindValue(r.precision);
    q.addBindValue(r.recall);
    q.addBindValue(r.map50);
    q.addBindValue(r.map5095);
    q.addBindValue(r.duration);
    q.addBindValue(r.resultModel);
    q.addBindValue(r.status);
    q.addBindValue(r.createdAt);
    if (!q.exec())
        return -1;
    return q.lastInsertId().toInt();
}

QList<TrainRecord> Database::records() const
{
    QList<TrainRecord> out;
    if (!isOpen())
        return out;
    QSqlQuery q(QSqlDatabase::database(kConnName));
    q.exec(
        "SELECT id, model, epochs, precision, recall, map50, map5095, duration,"
        " result_model, status, created_at FROM train_history ORDER BY id DESC"
    );
    while (q.next()) {
        TrainRecord r;
        r.id = q.value(0).toInt();
        r.model = q.value(1).toString();
        r.epochs = q.value(2).toInt();
        r.precision = q.value(3).toDouble();
        r.recall = q.value(4).toDouble();
        r.map50 = q.value(5).toDouble();
        r.map5095 = q.value(6).toDouble();
        r.duration = q.value(7).toString();
        r.resultModel = q.value(8).toString();
        r.status = q.value(9).toString();
        r.createdAt = q.value(10).toString();
        out.append(r);
    }
    return out;
}

bool Database::deleteRecord(int id)
{
    if (!isOpen() || id <= 0)
        return false;
    QSqlQuery q(QSqlDatabase::database(kConnName));
    q.prepare("DELETE FROM train_history WHERE id = ?");
    q.addBindValue(id);
    return q.exec();
}

void Database::clear()
{
    if (!isOpen())
        return;
    QSqlQuery q(QSqlDatabase::database(kConnName));
    q.exec("DELETE FROM train_history");
}
