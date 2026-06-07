/*
3proxy-eagle: Accumulate ethical 3proxy statistics with web interface.
Source code: https://notabug.org/acetone/3proxy-eagle.
Copyright (C) 2022, acetone

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "dbmanager.h"
#include "g.h"

#include <QDebug>
#include <QSqlQuery>
#include <QSqlResult>
#include <QSqlError>
#include <QSqlRecord>
#include <QDateTime>
#include <QRegularExpression>

QMutex DBManager::m_mtx;
qint64 DBManager::m_dailyTopTableLastActualizeTimestamp = 0;

constexpr int DB_VERSION = 1;

DBManager::DBManager() : m_connName(g::randomString())
{
    m_mtx.lock();
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connName);
    m_db.setDatabaseName(g::p::WORKING_DIR+"/statistics.db");

    if (not m_db.open())
    {
       qFatal("Connection with database failed (maybe working directory not found or not writable)");
    }
}

void DBManager::initAtStart()
{
    QSqlQuery query(m_db);

    bool appSettings = query.exec ("CREATE TABLE app_settings(param VARCHAR PRIMARY KEY, val VARCHAR)");
    if (appSettings)
    {
        setStandaloneVariable("db_version", QString::number(DB_VERSION));
    }
    else
    {
        QString version = getStandaloneVariable("db_version");
        if (version != QString::number(DB_VERSION))
        {
            qWarning().noquote() << "Your database version is [ " + (version.isEmpty() ? "UNKNOWN" : version) + " ]";
            qWarning().noquote() << "Required version is      [ " + QString::number(DB_VERSION) + " ]";
            qWarning().noquote() << "You should delete old database and restart app. Old statistics will be lost. Database file path: " + g::p::WORKING_DIR + "/statistics.db";
            qFatal("Database version error");
        }
    }

    query.exec ("CREATE TABLE IF NOT EXISTS total_history (name VARCHAR PRIMARY KEY, request_counter UNSIGNED BIGINT)");
    query.exec ("CREATE TABLE IF NOT EXISTS daily_history (name VARCHAR PRIMARY KEY, request_counter UNSIGNED BIGINT, date_of_record VARCHAR)");

    // For traffic used:
    // total_download_traffic
    // total_upload_traffic
    // daily_upload_traffic   + daily_upload_traffic_date
    // daily_download_traffic + daily_download_traffic_date

    query.exec ("VACUUM");
}

DestinationList DBManager::allHistory() const
{
    QSqlQuery query(m_db);

    DestinationList result;

    if (not query.exec("SELECT * FROM total_history ORDER BY request_counter DESC"))
    {
        qCritical() << "Db query failed:" << query.lastError().text();
    }
    else
    {
        while (query.next())
        {
            result.push_back( {query.value(0).toString(), query.value(1).toULongLong()} );
        }
    }

    return result;
}

DestinationList DBManager::totalTop() const
{
    QSqlQuery query(m_db);

    DestinationList result;

    if (not query.exec("SELECT * FROM total_history ORDER BY request_counter DESC LIMIT " + QString::number(g::p::LAST_AND_TOP_LIST_SIZE)))
    {
        qCritical() << "Db query failed:" << query.lastError().text();
    }
    else
    {
        while (query.next())
        {
            result.push_back( {query.value(0).toString(), query.value(1).toULongLong()} );
        }
    }

    return result;
}

DestinationList DBManager::dailyTop() const
{
    QSqlQuery query(m_db);

    DestinationList result;

    if (not query.exec("SELECT * FROM daily_history ORDER BY request_counter DESC LIMIT " + QString::number(g::p::LAST_AND_TOP_LIST_SIZE)))
    {
        qCritical() << "Db query failed:" << query.lastError().text();
    }
    else
    {
        const QString currd = currentDate();
        bool oldExists = false;

        while (query.next())
        {
            if (query.value(2).toString() == currd)
            {
                result.push_back( {query.value(0).toString(), query.value(1).toULongLong()} );
            }
            else
            {
                oldExists = true;
            }
        }

        if (oldExists)
        {
            actualizeDailyTopTable();
        }
    }

    return result;
}

quint64 DBManager::totalTopCount(const QString &dest) const
{
    QSqlQuery query(m_db);

    if (not query.exec("SELECT request_counter FROM total_history WHERE name = '" + escaped(dest) + "'"))
    {
        qCritical() << "Db query failed:" << query.lastError().text();
        return 0;
    }

    if (query.next())
    {
        return query.value(0).toULongLong();
    }

    return 0;
}

quint64 DBManager::dailyTopCount(const QString &dest) const
{
    QSqlQuery query(m_db);

    if (not query.exec("SELECT * FROM daily_history WHERE name = '" + escaped(dest) + "'"))
    {
        qCritical() << "Db query failed:" << query.lastError().text();
        return 0;
    }

    if (query.next())
    {
        if (query.value(2).toString() != currentDate())
        {
            actualizeDailyTopTable();
            return 0;
        }
        else
        {
            return query.value(1).toULongLong();
        }
    }

    return 0;
}

quint64 DBManager::incrementTotalTopCount(const QString &dest)
{
    QSqlQuery query(m_db);
    quint64 newValue = totalTopCount(dest) + 1;
    if (not query.exec("INSERT OR REPLACE INTO total_history(name, request_counter) VALUES ('"+ escaped(dest) +"', "+ QString::number(newValue) +")"))
    {
        qCritical() << "Db query failed:" << query.lastError().text();
    }
    return newValue;
}

quint64 DBManager::incrementDailyTopCount(const QString &dest)
{
    QSqlQuery query(m_db);
    quint64 newValue = dailyTopCount(dest) + 1;
    if (not query.exec("INSERT OR REPLACE INTO daily_history(name, request_counter, date_of_record) VALUES "
                       "('"+ escaped(dest) +"', '"+ QString::number(newValue) +"', '"+ currentDate() +"')"))
    {
        qCritical() << "Db query failed:" << query.lastError().text();
    }
    return newValue;
}

quint64 DBManager::incrementDailyUploadTraffic(quint64 bytes)
{
    quint64 newValue = dailyUploadTraffic() + bytes;
    setStandaloneVariable("daily_upload_traffic", QString::number(newValue));
    return newValue;
}

quint64 DBManager::incrementTotalUploadTraffic(quint64 bytes)
{
    quint64 newValue = totalUploadTraffic() + bytes;
    setStandaloneVariable("total_upload_traffic", QString::number(newValue));
    return newValue;
}

quint64 DBManager::incrementDailyDownloadTraffic(quint64 bytes)
{
    quint64 newValue = dailyDownloadTraffic() + bytes;
    setStandaloneVariable("daily_download_traffic", QString::number(newValue));
    return newValue;
}

quint64 DBManager::incrementTotalDownloadTraffic(quint64 bytes)
{
    quint64 newValue = totalDownloadTraffic() + bytes;
    setStandaloneVariable("total_download_traffic", QString::number(newValue));
    return newValue;
}

quint64 DBManager::dailyUploadTraffic() const
{
    const QString currd = currentDate();
    if (getStandaloneVariable("daily_upload_traffic_date") != currd)
    {
        setStandaloneVariable("daily_upload_traffic_date", currd);
        setStandaloneVariable("daily_upload_traffic", "0");
        return 0;
    }
    return getStandaloneVariable("daily_upload_traffic").toULongLong();
}

quint64 DBManager::totalUploadTraffic() const
{
    return getStandaloneVariable("total_upload_traffic").toULongLong();
}

quint64 DBManager::dailyDownloadTraffic() const
{
    const QString currd = currentDate();
    if (getStandaloneVariable("daily_download_traffic_date") != currd)
    {
        setStandaloneVariable("daily_download_traffic_date", currd);
        setStandaloneVariable("daily_download_traffic", "0");
        return 0;
    }
    return getStandaloneVariable("daily_download_traffic").toULongLong();
}

quint64 DBManager::totalDownloadTraffic() const
{
    return getStandaloneVariable("total_download_traffic").toULongLong();
}

DBManager::~DBManager()
{
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connName);
    m_mtx.unlock();
}

void DBManager::actualizeDailyTopTable() const
{
    qint64 started = QDateTime::currentMSecsSinceEpoch();
    if (started - m_dailyTopTableLastActualizeTimestamp < g::c::DB_DAILY_TOP_TABLE_ACTUALIZE_MINIMAL_INTERVAL_MS)
    {
        qDebug() << "Database daily history table actualizing rejected by minimal interval:" << g::c::DB_DAILY_TOP_TABLE_ACTUALIZE_MINIMAL_INTERVAL_MS << "ms";
        return;
    }

    QSqlQuery query(m_db);
    if (not query.exec("DELETE FROM daily_history WHERE date_of_record != '"+ currentDate() + "'"))
    {
        qCritical() << "Db query failed:" << query.lastError().text();
    }

    m_dailyTopTableLastActualizeTimestamp = QDateTime::currentMSecsSinceEpoch();
    qInfo() << "Daily history database table actualized in" << m_dailyTopTableLastActualizeTimestamp-started << "ms";
}

QString DBManager::currentDate()
{
    return QDateTime::currentDateTimeUtc().toString("yyyyMMdd");
}

DBManager::ResultPair DBManager::setStandaloneVariable(const QString &key, const QString &value) const
{
    QSqlQuery query(m_db);
    bool status = query.exec("INSERT OR REPLACE INTO app_settings(param, val) VALUES ('"+ escaped(key) +"', '"+ escaped(value) +"')");
    return status ? ResultPair(true) : ResultPair(false, query.lastError().text());
}

QString DBManager::getStandaloneVariable(const QString &key) const
{
    QSqlQuery query(m_db);

    if (not query.exec("SELECT val FROM app_settings WHERE param = '" + escaped(key) + "'"))
    {
        qCritical() << "Db query failed:" << query.lastError().text();
        return QString();
    }

    if (query.next())
    {
        return query.value(0).toString();
    }
    else
    {
        qDebug() << "Db query failed (empty result for " + key + "):" << query.lastError().text();
        return QString();
    }
}

QString DBManager::escaped(const QString &str)
{
    QString result {str};
    static QRegularExpression rgx("[';`]");
    result.replace(rgx, "_");
    return result;
}
