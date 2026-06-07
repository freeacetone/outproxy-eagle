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

#include <QString>
#include <QList>
#include <QPair>

class QFile;
class ProxyInstanse;
class QThread;

using DestinationList = QList<QPair<QString, quint64>>;

namespace g /* for Global */ {

namespace c /* for Constants */ {
    extern const QString SOFTWARE_NAME;
    extern const QString SOFTWARE_VERSION;
    extern const QString COPYRIGHT;
    extern const qint64  DB_DAILY_TOP_TABLE_ACTUALIZE_MINIMAL_INTERVAL_MS;
    extern const qint64  CACHE_ACTUALIZE_TOP_LISTS_FROM_DB_MINIMAL_INTERVAL_MS;
    extern const QString HA_PAGE_TITLE;
    extern const QString HA_TRAFFIC_SECTION_TITLE;
    extern const QString HA_CUSTOM_CSS;
    extern const QString HA_DAILY_UPLOAD;
    extern const QString HA_DAILY_UPLOAD_DECIMAL;
    extern const QString HA_DAILY_UPLOAD_MEASURE;
    extern const QString HA_DAILY_DOWNLOAD;
    extern const QString HA_DAILY_DOWNLOAD_DECIMAL;
    extern const QString HA_DAILY_DOWNLOAD_MEASURE;
    extern const QString HA_TOTAL_UPLOAD;
    extern const QString HA_TOTAL_DOWNLOAD;
    extern const QString HA_LAST_DESTINATIONS_LIST;
    extern const QString HA_DAILY_TOP_DESTINATIONS_LIST;
    extern const QString HA_TOTAL_TOP_DESTINATIONS_LIST;
    extern const QString HA_TOP_MEASURE;
    extern const QString HA_BLOCKED_DESTINTIONS;
    extern const QString HA_INFORMATION;
    extern const QString HA_COPYRIGHT;
    extern const QString HA_VERSION;
} // namespace c

namespace p /* for Parameters */ {
    extern uint LAST_AND_TOP_LIST_SIZE;
    extern QString WORKING_DIR;
    extern QStringList IGNORED_DESTINATIONS;
    extern QString SERVICE_TITLE;
    extern QString BIND_TO_ADDRESS;
    extern quint16 BIND_TO_PORT;
    extern bool IGNORE_IP_ADDRS_IN_STATISTCS;
} // namespace p

extern std::list< std::pair<QThread*, ProxyInstanse*> > instanses;

enum LogLevel : uint8_t
{
    Off     = 0,
    Error   = 1,
    Warning = 2,
    Info    = 3,
    Debug   = 4
};
extern LogLevel logLevel;

void customMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);
enum class GetValueType { // QString getValue()
    Default = 1,
    Url = 2,
    HttpHeader = 3
};
QString getValue(const QString &string, const QString &key, GetValueType type = GetValueType::Default);
QString randomString(int length = 10);
QString hash(const QByteArray &data);
QString hash(QFile&& file);

} // namespace g
