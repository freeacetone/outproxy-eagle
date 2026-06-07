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

#include <QByteArray>
#include <QStringList>

struct HumanReadableValue;

class WebPage
{
public:
    WebPage() = delete;
    static QByteArray document(quint64 lastTrafficVolume, quint64* currentTrafficVolume = nullptr);
    static QString counterToHumanReadableString(quint64 count);

private:
    static void setTitle(QString& document);
    static void customStyles(QString& document);
    static void lastDestinations(QString& document);
    static quint64 trafficBlock(QString& document, quint64 lastTrafficVolume);
    static void topDestinations(QString& document);
    static void blockedDestinations(QString& document);
    static void informationBlock(QString& document);
    static void footer(QString& document);

    static QString lastDestinationItemAllowed(const QString& destination);
    static QString lastDestinationItemBlocked(const QString& destination);
    static QString topDestinationItem(const QString& destination, quint64 count);
    static QString blockedDestinationItem(const QString& destination, const QStringList& addresses);
    static HumanReadableValue bytesToHumanReadableString(quint64 bytes);

    static const QString m_lastDestinationItemAllowed;
    static const QString m_lastDestinationItemBlocked;
    static const QString m_topDestinationItem;
    static const QString m_blockedDestinationsBlock;
    static const QString m_blockedDestinationItemLvl1;
    static const QString m_blockedDestinationItemLvl2;
    static const QString m_informationBlock;
};

