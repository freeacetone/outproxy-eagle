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

#include "statistics.h"
#include "dbmanager.h"
#include "webpage.h"
#include "g.h"

#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QFile>

QMutex Statistics::m_lastDestinationsMtx;
QStringList Statistics::m_lastDestinations;

std::atomic<quint64> Statistics::m_dailyUpload {0};
std::atomic<quint64> Statistics::m_dailyDownload {0};
std::atomic<quint64> Statistics::m_totalUpload {0};
std::atomic<quint64> Statistics::m_totalDownload {0};

QMutex Statistics::m_topDestinationsMtx;
DestinationList Statistics::m_dailyTopDestinations;
DestinationList Statistics::m_totalTopDestinations;

bool Statistics::m_cacheUpdatedAfterLastReport = false;
qint64 Statistics::m_actualizeTopListsLastTimestamp = 0;

void Statistics::report(const LogEvent& event)
{
    if (g::p::IGNORED_DESTINATIONS.contains(event.dest))
    {
        qDebug() << "Destination" << event.dest << "ignored";
        return;
    }

    m_lastDestinationsMtx.lock();
    int index = m_lastDestinations.indexOf(event.dest);
    if (index == -1)
    {
        if (m_lastDestinations.size() == 5)
        {
            m_lastDestinations.pop_back();
        }
    }
    else
    {
        m_lastDestinations.removeAt(index);
    }
    m_lastDestinations.push_front(event.dest);
    m_lastDestinationsMtx.unlock();

    if (g::p::IGNORE_IP_ADDRS_IN_STATISTCS and isIpAddress(event.dest))
    {
        qDebug() << event.dest << "rejected from statistics";
        return;
    }

    DBManager db;

    m_dailyDownload = db.incrementDailyDownloadTraffic(event.from);
    m_dailyUpload = db.incrementDailyUploadTraffic(event.to);
    m_totalDownload = db.incrementTotalDownloadTraffic(event.from);
    m_totalUpload = db.incrementTotalUploadTraffic(event.to);

    db.incrementTotalTopCount(event.dest);
    db.incrementDailyTopCount(event.dest);

    m_cacheUpdatedAfterLastReport = false;
}

const QStringList Statistics::lastDestinations()
{
    QMutexLocker lock (&m_lastDestinationsMtx);
    return m_lastDestinations;
}

const DestinationList Statistics::dailyTopDestinations()
{
    if (not m_cacheUpdatedAfterLastReport)
    {
        actualizeTopLists();
    }
    QMutexLocker lock (&m_topDestinationsMtx);
    return m_dailyTopDestinations;
}

const DestinationList Statistics::totalTopDestinations()
{
    QMutexLocker lock (&m_topDestinationsMtx);
    return m_totalTopDestinations;
}

void Statistics::initTrafficCounters()
{
    DBManager db;
    m_dailyDownload = db.dailyDownloadTraffic();
    m_dailyUpload = db.dailyUploadTraffic();
    m_totalDownload = db.totalDownloadTraffic();
    m_totalUpload = db.totalUploadTraffic();
}

QString Statistics::statsTxt()
{
    static QMutex mtx;
    if (not mtx.tryLock())
    {
        return "Service is busy, try again later.";
    }

    auto raw = DBManager().allHistory();

    quint64 totalRequests = 0;
    QString result;

    for (const auto& record: raw)
    {
        result += WebPage::counterToHumanReadableString(record.second) + "\t" + record.first + "\n";
        totalRequests += record.second;
    }

    result.push_front("Total destinations: " + WebPage::counterToHumanReadableString(raw.size()) + "\n"
                      "Total requests: " + WebPage::counterToHumanReadableString(totalRequests) + "\n\n");

    mtx.unlock();
    return result;
}

void Statistics::actualizeTopLists()
{
    QMutexLocker lock (&m_topDestinationsMtx);

    qint64 started = QDateTime::currentMSecsSinceEpoch();
    if (started - m_actualizeTopListsLastTimestamp < g::c::CACHE_ACTUALIZE_TOP_LISTS_FROM_DB_MINIMAL_INTERVAL_MS)
    {
        qDebug() << "Cache actualizing rejected by minimal interval:" << g::c::CACHE_ACTUALIZE_TOP_LISTS_FROM_DB_MINIMAL_INTERVAL_MS << "ms";
        return;
    }

    DBManager db;
    m_dailyTopDestinations = db.dailyTop();
    m_totalTopDestinations = db.totalTop();

    m_cacheUpdatedAfterLastReport = true;
    m_actualizeTopListsLastTimestamp = QDateTime::currentMSecsSinceEpoch();
    qDebug() << "Cache actualized in" << m_actualizeTopListsLastTimestamp-started << "ms";
}

bool Statistics::isIpAddress(const QString& dest)
{
    static QRegularExpression ipv4("^[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}$");
    if (ipv4.match(dest).hasMatch())
    {
        return true;
    }

    static QRegularExpression ipv6("^\\[[a-f0-9:]*\\]$");
    if (ipv6.match(dest).hasMatch())
    {
        return true;
    }

    return false;
}
