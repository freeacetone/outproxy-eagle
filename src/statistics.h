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

#include <QStringList>
#include <QMutex>

struct LogEvent
{
    QString type; // not handled
    QString dest;
    quint64 to = 0;
    quint64 from = 0;
};

class Statistics
{
public:
    Statistics() = delete;
    static void report (const LogEvent& event);

    static const QStringList lastDestinations();

    static const DestinationList dailyTopDestinations();
    static const DestinationList totalTopDestinations();

    static void initTrafficCounters();
    static quint64 dailyUpload() { return m_dailyUpload; }
    static quint64 totalUpload() { return m_totalUpload; }
    static quint64 dailyDownload() { return m_dailyDownload; }
    static quint64 totalDownload() { return m_totalDownload; }

    static QString statsTxt();

private:
    static void actualizeTopLists();
    static bool isIpAddress(const QString& dest);

    static QMutex m_lastDestinationsMtx;
    static QStringList m_lastDestinations;

    static bool m_cacheUpdatedAfterLastReport;
    static qint64 m_actualizeTopListsLastTimestamp;

    static std::atomic<quint64> m_dailyUpload;
    static std::atomic<quint64> m_dailyDownload;
    static std::atomic<quint64> m_totalUpload;
    static std::atomic<quint64> m_totalDownload;

    static QMutex m_topDestinationsMtx;
    static DestinationList m_dailyTopDestinations;
    static DestinationList m_totalTopDestinations;
};

