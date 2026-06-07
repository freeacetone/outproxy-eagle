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

#pragma once

#include "g.h"

#include <QString>
#include <QSqlDatabase>
#include <QMutex>

class DBManager
{
public:
    class ResultPair
    {
    public:
        ResultPair (bool status, QString string = QString()) : m_str(string), m_bool(status) {}
        bool status() const { return m_bool; }
        QString string() const { return m_str; }

    private:
        QString m_str;
        bool m_bool;
    };

    DBManager();
    void initAtStart();

    DestinationList allHistory() const;
    DestinationList totalTop() const;
    DestinationList dailyTop() const;
    quint64 totalTopCount(const QString& dest) const;
    quint64 dailyTopCount(const QString& dest) const;
    quint64 incrementTotalTopCount(const QString& dest);
    quint64 incrementDailyTopCount(const QString& dest);

    quint64 incrementDailyUploadTraffic(quint64 bytes);
    quint64 incrementTotalUploadTraffic(quint64 bytes);
    quint64 incrementDailyDownloadTraffic(quint64 bytes);
    quint64 incrementTotalDownloadTraffic(quint64 bytes);
    quint64 dailyUploadTraffic() const;
    quint64 totalUploadTraffic() const;
    quint64 dailyDownloadTraffic() const;
    quint64 totalDownloadTraffic() const;

    static QString currentDate();
    static QString escaped(const QString &str);

    ~DBManager();

private:
    void actualizeDailyTopTable() const;

    ResultPair setStandaloneVariable(const QString& key, const QString& value) const;
    QString getStandaloneVariable(const QString& key) const;

    const QString m_connName;
    QSqlDatabase m_db;
    static QMutex m_mtx;
    static qint64 m_dailyTopTableLastActualizeTimestamp;
};
