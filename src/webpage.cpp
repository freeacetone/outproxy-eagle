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

#include "webpage.h"
#include "proxyinstanse.h"
#include "g.h"
#include "statistics.h"

#include <QFile>
#include <QDebug>
#include <cmath>

const QString WebPage::m_lastDestinationItemAllowed = "\
          <li class=\"lastDestinations__item lastDestinations__item_allowed\" title=\"{{VALUE}}\">\n\
            {{VALUE}}\n\
          </li>\n";

const QString WebPage::m_lastDestinationItemBlocked ="\
          <li class=\"lastDestinations__item lastDestinations__item_blocked\" title=\"{{VALUE}}\">\n\
            {{VALUE}}\n\
          </li>\n";

const QString WebPage::m_topDestinationItem = "\
            <li class=\"topDestinations__item\" title=\"{{VALUE}}\">\n\
              <span class=\"topDestinations__destination\">{{VALUE}}</span>\n\
              <span class=\"topDestinations__counter\">{{COUNT}}</span>\n\
            </li>\n";

const QString WebPage::m_blockedDestinationsBlock = "\
      <section class=\"proxyService__blockedDestinations blockedDestinations section\">\n\
        <div class=\"blockedDestinations__title section__title\">\n\
          Blocked destinations\n\
        </div>\n\
        <ul class=\"blockedDestinations__list accordeo\">\n\
{{VALUE}}\n\
        </ul>\n\
      </section>\n";

const QString WebPage::m_blockedDestinationItemLvl1 = "\
          <li class=\"blockedDestinations__item accordeo__item\">\n\
            <label class=\"blockedDestinations__url accordeo__visible\" for=\"{{VALUE}}\">\n\
              {{VALUE}}\n\
            </label>\n\
            <input type=\"radio\" class=\"accordeo__radio\" name=\"blockedDestinations__accordeo\" id=\"{{VALUE}}\">\n\
            <ul class=\"blockedDestinations__ipList accordeo__dropdown\">\n\
{{LIST}}\n\
            </ul>\n\
          </li>\n";

const QString WebPage::m_blockedDestinationItemLvl2 = "\
              <li class=\"blockedDestinations__ipItem\">\n\
                {{VALUE}}\n\
              </li>\n";

const QString WebPage::m_informationBlock = "\
        <section class=\"proxyService__information information section\">\n\
          <div class=\"information__title section__title\">\n\
            Information\n\
          </div>\n\
          <div class=\"information__text\">\n\
            {{VALUE}}\n\
          </div>\n\
        </section>\n";

struct HumanReadableValue
{
    QString integer = "3";
    QString decimal = "5";
    QString measure = "anon";
};

QByteArray WebPage::document(quint64 lastTrafficVolume, quint64* currentTrafficVolume)
{
    QFile src(":/html/index.html");
    if (not src.open(QIODevice::ReadOnly))
    {
        qCritical() << "Can not read built-in index.html";
        return QByteArray();
    }

    QString page = src.readAll();
    src.close();

    setTitle(page);
    customStyles(page);
    lastDestinations(page);
    topDestinations(page);
    blockedDestinations(page);
    informationBlock(page);
    footer(page);

    quint64 traffic = trafficBlock(page, lastTrafficVolume);

    if (currentTrafficVolume != nullptr)
    {
        *currentTrafficVolume = traffic;
    }

    return page.toUtf8();
}

void WebPage::setTitle(QString &document)
{
    document.replace(g::c::HA_PAGE_TITLE, g::p::SERVICE_TITLE);
}

void WebPage::customStyles(QString &document)
{
    bool exists = QFile::exists(g::p::WORKING_DIR + "/html/styles.css");
    document.replace(g::c::HA_CUSTOM_CSS, exists ? "\n  <link rel=\"stylesheet\" href=\"/styles.css\">" : "");
}

void WebPage::lastDestinations(QString &document)
{
    static QStringList allBlockedDestinations;
    static bool inited = false;
    if (not inited)
    {
        inited = true;
        for (const auto& inst: g::instanses)
        {
            QMapIterator<QString, QStringList> mIter(inst.second->blackList());
            while (mIter.hasNext())
            {
                mIter.next();
                if (not allBlockedDestinations.contains(mIter.key()))
                {
                    allBlockedDestinations.push_back(mIter.key());
                }
                for (const auto& addr: mIter.value())
                {
                    if (not allBlockedDestinations.contains(addr))
                    {
                        allBlockedDestinations.push_back(addr);
                    }
                }
            }
        }
    }

    QStringList dests = Statistics::lastDestinations();
    QString destinationsBlock;

    for (const auto& dest: dests)
    {
        bool blocked = allBlockedDestinations.contains(dest);
        destinationsBlock += blocked ? lastDestinationItemBlocked(dest) : lastDestinationItemAllowed(dest);
    }

    document.replace(g::c::HA_LAST_DESTINATIONS_LIST, destinationsBlock);
}

quint64 WebPage::trafficBlock(QString &document, quint64 lastTrafficVolume)
{
    quint64 currentVolume = Statistics::totalUpload() + Statistics::totalDownload();
    if (lastTrafficVolume != 0 and currentVolume > lastTrafficVolume)
    {
        quint64 difference = currentVolume - lastTrafficVolume;
        auto differenceHr = bytesToHumanReadableString(difference);
        QString volume = differenceHr.integer + ((differenceHr.decimal != ".00") ? differenceHr.decimal : "") + " " + differenceHr.measure;
        document.replace(g::c::HA_TRAFFIC_SECTION_TITLE, "Since your last visit: " + volume);
    }
    else
    {
        document.replace(g::c::HA_TRAFFIC_SECTION_TITLE, "Traffic");
    }

    auto dailyUpload = bytesToHumanReadableString( Statistics::dailyUpload() );
    document.replace(g::c::HA_DAILY_UPLOAD, dailyUpload.integer);
    document.replace(g::c::HA_DAILY_UPLOAD_DECIMAL, dailyUpload.decimal);
    document.replace(g::c::HA_DAILY_UPLOAD_MEASURE, dailyUpload.measure);

    auto dailyDownload = bytesToHumanReadableString( Statistics::dailyDownload() );
    document.replace(g::c::HA_DAILY_DOWNLOAD, dailyDownload.integer);
    document.replace(g::c::HA_DAILY_DOWNLOAD_DECIMAL, dailyDownload.decimal);
    document.replace(g::c::HA_DAILY_DOWNLOAD_MEASURE, dailyDownload.measure);

    auto totalUpload = bytesToHumanReadableString( Statistics::totalUpload() );
    document.replace(g::c::HA_TOTAL_UPLOAD, totalUpload.integer + totalUpload.decimal + " " + totalUpload.measure);

    auto totalDownload = bytesToHumanReadableString( Statistics::totalDownload() );
    document.replace(g::c::HA_TOTAL_DOWNLOAD, totalDownload.integer + totalDownload.decimal + " " + totalDownload.measure);

    return currentVolume;
}

void WebPage::topDestinations(QString &document)
{
    document.replace(g::c::HA_TOP_MEASURE, g::p::IGNORE_IP_ADDRS_IN_STATISTCS ? "domains" : "destinations");

    QString dailyTopList;
    for (const auto& dTop: Statistics::dailyTopDestinations())
    {
        dailyTopList += topDestinationItem(dTop.first, dTop.second);
    }
    document.replace(g::c::HA_DAILY_TOP_DESTINATIONS_LIST, dailyTopList);

    QString totalTopList;
    for (const auto& dTop: Statistics::totalTopDestinations())
    {
        totalTopList += topDestinationItem(dTop.first, dTop.second);
    }
    document.replace(g::c::HA_TOTAL_TOP_DESTINATIONS_LIST, totalTopList);
}

void WebPage::blockedDestinations(QString &document)
{
    static QString blockedDestinationsBlock;
    static bool inited = false;
    if (not inited)
    {
        inited = true;

        QMap<QString,QSet<QString>> uniqueRecords;

        QString blockedDestinationsList;

        for (const auto& inst: g::instanses)
        {
            QMapIterator<QString, QStringList> mIter(inst.second->blackList());
            while (mIter.hasNext())
            {
                auto record = mIter.next();
                for (const auto& addr: record.value())
                {
                    uniqueRecords[record.key()].insert(addr);
                }

                if (not uniqueRecords.contains(record.key()))
                {
                    uniqueRecords.insert(record.key(), QSet<QString>());
                }
            }
        }

        QMapIterator<QString, QSet<QString>> mIter(uniqueRecords);
        while (mIter.hasNext())
        {
            auto record = mIter.next();
            blockedDestinationsList += blockedDestinationItem(record.key(), record.value().values());
        }

        if (not blockedDestinationsList.isEmpty())
        {
            blockedDestinationsBlock = m_blockedDestinationsBlock;
            blockedDestinationsBlock.replace("{{VALUE}}", blockedDestinationsList);
            blockedDestinationsBlock = '\n'+blockedDestinationsBlock;
        }
    }

    document.replace(g::c::HA_BLOCKED_DESTINTIONS, blockedDestinationsBlock);
}

void WebPage::informationBlock(QString &document)
{
    QFile f(g::p::WORKING_DIR + "/information.html");
    if (not f.exists())
    {
        document.replace(g::c::HA_INFORMATION, "");
        return;
    }

    QString infoBlock {m_informationBlock};
    QString customText;
    if (f.exists())
    {
        if (not f.open(QIODevice::ReadOnly))
        {
            qWarning() << "information.html exists, but reading failed";
        }
        else
        {
            customText = f.readAll();
            f.close();
        }
    }

    infoBlock.replace("{{VALUE}}", customText);
    document.replace(g::c::HA_INFORMATION, "\n"+infoBlock);
}

void WebPage::footer(QString &document)
{
    document.replace(g::c::HA_COPYRIGHT, g::c::COPYRIGHT);
    document.replace(g::c::HA_VERSION, g::c::SOFTWARE_VERSION);
}

QString WebPage::lastDestinationItemAllowed(const QString &destination)
{
    QString result {m_lastDestinationItemAllowed};
    result.replace("{{VALUE}}", destination);
    return result;
}

QString WebPage::lastDestinationItemBlocked(const QString &destination)
{
    QString result {m_lastDestinationItemBlocked};
    result.replace("{{VALUE}}", destination);
    return result;
}

QString WebPage::topDestinationItem(const QString &destination, quint64 count)
{
    QString result {m_topDestinationItem};
    result.replace("{{VALUE}}", destination);
    result.replace("{{COUNT}}", counterToHumanReadableString(count));
    return result;
}

QString WebPage::blockedDestinationItem(const QString &destination, const QStringList &addresses)
{
    QString result {m_blockedDestinationItemLvl1};
    result.replace("{{VALUE}}", destination);
    QString addressesItems;
    for (const auto& addr: addresses)
    {
        QString addrItem {m_blockedDestinationItemLvl2};
        addrItem.replace("{{VALUE}}", addr);
        addressesItems += addrItem;
    }
    result.replace("{{LIST}}", addressesItems);
    return result;
}

HumanReadableValue WebPage::bytesToHumanReadableString(quint64 bytes)
{
    HumanReadableValue result;

    constexpr const quint64 KBYTE = 1024;

    if (bytes < KBYTE)
    {
        result.integer = QString::number(bytes);
        result.decimal = "0";
        result.measure = "B";
    }

    else if (bytes <= KBYTE*KBYTE)
    {
        result.integer = QString::number(bytes/KBYTE);
        result.decimal = QString::number(bytes%KBYTE);
        result.measure = "KiB";
    }

    else if (bytes <= KBYTE*KBYTE*KBYTE)
    {
        result.integer = QString::number(bytes /KBYTE/KBYTE);
        result.decimal = QString::number(bytes% (KBYTE*KBYTE));
        result.measure = "MiB";
    }

    else if (bytes <= KBYTE*KBYTE*KBYTE*KBYTE)
    {
        result.integer = QString::number(bytes /KBYTE/KBYTE/KBYTE);
        result.decimal = QString::number(bytes% (KBYTE*KBYTE*KBYTE));
        result.measure = "GiB";
    }

    else if (bytes <= KBYTE*KBYTE*KBYTE*KBYTE*KBYTE)
    {
        result.integer = QString::number(bytes /KBYTE/KBYTE/KBYTE/KBYTE);
        result.decimal = QString::number(bytes% (KBYTE*KBYTE*KBYTE*KBYTE));
        result.measure = "TiB";
    }

    else
    {
        result.integer = QString::number(bytes /KBYTE/KBYTE/KBYTE/KBYTE/KBYTE);
        result.decimal = QString::number(bytes% (KBYTE*KBYTE*KBYTE*KBYTE*KBYTE));
        result.integer = "PiB";
    }

    if (result.decimal.size() > 2)
    {
        result.decimal = result.decimal.remove(2, result.decimal.size()-2);
    }
    else if (result.decimal.size() < 2)
    {
        result.decimal.push_back('0');
    }

    result.decimal.push_front('.');

    return result;
}

QString WebPage::counterToHumanReadableString(quint64 count)
{
    constexpr const quint64 THOUSAND = 1000;

    QString result;

    if (count < THOUSAND)
    {
        result = QString::number(count);
    }

    else if (count <= THOUSAND*THOUSAND)
    {
        result = QString::number(count/THOUSAND);
        auto decimal = count%THOUSAND;
        result += "." + QString::number(decimal).mid(0,1);
        result += "K";
    }

    else if (count <= THOUSAND*THOUSAND*THOUSAND)
    {
        result = QString::number(count /THOUSAND/THOUSAND);
        auto decimal = count% (THOUSAND*THOUSAND);
        result += "." + QString::number(decimal).mid(0,1);
        result += "M";
    }

    else if (count <= THOUSAND*THOUSAND*THOUSAND*THOUSAND)
    {
        result = QString::number(count /THOUSAND/THOUSAND/THOUSAND);
        auto decimal = count% (THOUSAND*THOUSAND*THOUSAND);
        result += "." + QString::number(decimal).mid(0,1);
        result += "B";
    }

    return result;
}
