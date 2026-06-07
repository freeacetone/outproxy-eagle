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

#include "g.h"

#include <QDebug>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QMutex>
#include <QFile>
#include <QRandomGenerator>
#include <iostream>

namespace g {

namespace c {
    const QString SOFTWARE_NAME = "3proxy-eagle";
    const QString SOFTWARE_VERSION = "0.0.2";
    const QString COPYRIGHT = "GPLv3+ &copy acetone, 2022-2023";
    const qint64  DB_DAILY_TOP_TABLE_ACTUALIZE_MINIMAL_INTERVAL_MS = 300000; // 5 min
    const qint64  CACHE_ACTUALIZE_TOP_LISTS_FROM_DB_MINIMAL_INTERVAL_MS = 5000; // 5 sec
    const QString HA_PAGE_TITLE = "{{PAGE_TITLE}}";
    const QString HA_TRAFFIC_SECTION_TITLE = "{{TRAFFIC_SECTION_TITLE}}";
    const QString HA_CUSTOM_CSS = "{{CUSTOM_CSS}}";
    const QString HA_DAILY_UPLOAD = "{{DAILY_UPLOAD}}";
    const QString HA_DAILY_UPLOAD_DECIMAL = "{{DAILY_UPLOAD_DECIMAL}}";
    const QString HA_DAILY_UPLOAD_MEASURE = "{{DAILY_UPLOAD_MEASURE}}";
    const QString HA_DAILY_DOWNLOAD = "{{DAILY_DOWNLOAD}}";
    const QString HA_DAILY_DOWNLOAD_DECIMAL = "{{DAILY_DOWNLOAD_DECIMAL}}";
    const QString HA_DAILY_DOWNLOAD_MEASURE = "{{DAILY_DOWNLOAD_MEASURE}}";
    const QString HA_TOTAL_UPLOAD = "{{TOTAL_UPLOAD}}";
    const QString HA_TOTAL_DOWNLOAD = "{{TOTAL_DOWNLOAD}}";
    const QString HA_LAST_DESTINATIONS_LIST = "{{LAST_DESTINATIONS_LIST}}";
    const QString HA_DAILY_TOP_DESTINATIONS_LIST = "{{DAILY_TOP_DESTINATIONS_LIST}}";
    const QString HA_TOTAL_TOP_DESTINATIONS_LIST = "{{TOTAL_TOP_DESTINATIONS_LIST}}";
    const QString HA_TOP_MEASURE = "{{TOP_MEASURE}}";
    const QString HA_BLOCKED_DESTINTIONS = "{{BLOCKED_DESTINATIONS}}";
    const QString HA_INFORMATION = "{{INFORMATION}}";
    const QString HA_COPYRIGHT = "{{COPYRIGHT}}";
    const QString HA_VERSION = "{{VERSION}}";
} // namespace c

namespace p {
    uint LAST_AND_TOP_LIST_SIZE = 10;
    QStringList IGNORED_DESTINATIONS = {"[0.0.0.0]", "0.0.0.0"};
    QString WORKING_DIR = "data";
    QString SERVICE_TITLE = "3proxy-eagle";
    QString BIND_TO_ADDRESS = "127.0.0.1";
    quint16 BIND_TO_PORT = 8161;
    bool IGNORE_IP_ADDRS_IN_STATISTCS = false;
} // namespace p

std::list< std::pair<QThread*, ProxyInstanse*> > instanses;

LogLevel logLevel = LogLevel::Info;

void customMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (logLevel == LogLevel::Off) return;

    static QMutex mtx;
    QMutexLocker lock (&mtx);

    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";

    if (type == QtDebugMsg)
    {
        if (logLevel == LogLevel::Debug)
        {
            std::cout << "[Dbug]: " << localMsg.constData() << " " << function << std::endl;
        }
    }

    else if (type == QtInfoMsg)
    {
        if (logLevel >= LogLevel::Info)
        {
            std::cout << "[Info]: " << localMsg.constData() << std::endl;
        }
    }

    else if (type == QtWarningMsg)
    {
        if (logLevel >= LogLevel::Warning)
        {
            std::cerr << "[Warn]: " << localMsg.constData() << std::endl;
        }
    }

    else if (type == QtCriticalMsg)
    {
        if (logLevel >= LogLevel::Error)
        {
            std::cerr << "[Crit]: " << localMsg.constData() << " (" << file << "," << context.line << "," << function  << ")" << std::endl;
        }
    }

    else if (type == QtFatalMsg)
    {
        if (logLevel >= LogLevel::Error)
        {
            std::cerr << "[Fatl]: " << localMsg.constData() << " (" << file << "," << context.line << "," << function << ")" << std::endl;
        }
    }
}

QString getValue(const QString &string, const QString &key, GetValueType type)
{
    if (key.isEmpty())
    {
        return QString();
    }

    if (type == GetValueType::HttpHeader)
    {
        if (string.indexOf(QRegularExpression(key + ":")) == -1)
        {
            return QString();
        }
    }
    else if (string.indexOf(QRegularExpression(key + "\\s*=")) == -1)
    {
        return QString();
    }

    QString result {string};
    if (type == GetValueType::HttpHeader)
    {
        result.remove(QRegularExpression("^.*"+key+":\\s", QRegularExpression::DotMatchesEverythingOption));
    }
    else
    {
        result.remove(QRegularExpression("^.*"+key+"\\s*=", QRegularExpression::DotMatchesEverythingOption));
    }

    QString separator {' '};
    if (type == GetValueType::Url)
    {
        separator = '&';
    }
    else if (type == GetValueType::HttpHeader)
    {
        separator = "\r\n";
    }

    int valueEnd = result.indexOf(separator);

    if (valueEnd == -1 and type == GetValueType::Url)
    {
        separator = ' ';
        valueEnd = result.indexOf(separator);
        if (valueEnd != -1)
        {
            result.remove(valueEnd, result.size()-valueEnd);
        }
    }
    else if (valueEnd != -1)
    {
        result.remove(valueEnd, result.size()-valueEnd);
    }

    if (type == GetValueType::Url)
    {
        result = QByteArray::fromPercentEncoding(result.toUtf8());
        result.replace('+', ' ');
    }
    else if (type == GetValueType::HttpHeader)
    {
        result.remove('\"');
    }
    else
    {
        static QRegularExpression beginSpaces("^\\s*");
        static QRegularExpression endSpaces("\\s*$");
        result.remove(beginSpaces);
        result.remove(endSpaces);
    }

    return result;
}

QString randomString(int length)
{
    static const QString table
         {"0123456789"
          "abcdefghij"
          "klmnkpqrst"
          "uvwxyzABCD"
          "EFGHIJKLMN"
          "hPQRSTUVWX"};

    static const int posLimit = table.size() - 1;

    QString value;
    while(value.size() < length)
    {
        value += table[ QRandomGenerator::system()->bounded (0, posLimit) ];
    }
    return value;
}

QString hash(const QByteArray &data)
{
    QString hash = QCryptographicHash::hash(data, QCryptographicHash::Md5).toBase64();
    static QRegularExpression rgx_removeTrash ("[^0-9a-zA-Z_]");
    hash.remove(rgx_removeTrash);
    return hash;
}

QString hash(QFile&& file)
{
    if (not file.open(QIODevice::ReadOnly))
    {
        qDebug() << "Hash failed: file openning failed:" << file.fileName();
        return QString();
    }
    QString result = hash(file.readAll());
    file.close();
    return result;
}

} // namespace g
