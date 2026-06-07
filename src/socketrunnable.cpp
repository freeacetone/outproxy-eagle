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

#include "socketrunnable.h"
#include "statistics.h"
#include "g.h"
#include "webpage.h"

#include <QRegularExpression>

constexpr const int getValueBeforeSpaceLIMIT = 200;
constexpr const int getValueBeforeBodyLIMIT = 1000;

constexpr const char* LAST_TRAFFIC_VOLUME_COOKIE = "trafficvolume";

SocketRunnable::SocketRunnable(qintptr socketDescriptor, QObject *parent) :
    SocketRunnableBase(socketDescriptor, parent)
{
}

void SocketRunnable::get()
{
    if (urlPath() == "/index.html")
    {
        const QString cookieString = g::getValue(m_headers, "Cookie", g::GetValueType::HttpHeader);
        quint64 lastTrafficVolume = g::getValue(cookieString, LAST_TRAFFIC_VOLUME_COOKIE).toULongLong();
        qDebug().noquote() << "Cookie trafficvolume:" << lastTrafficVolume << "(from " + cookieString+")";
        quint64 currentTrafficVolume = 0;
        httpDocument().setBody( WebPage::document(lastTrafficVolume, &currentTrafficVolume) );
        httpDocument().setHeader(HttpDocument::CommonHeader::SetCookie,
                                 QString(LAST_TRAFFIC_VOLUME_COOKIE)+"="+QString::number(currentTrafficVolume)+"; max-age=31536000;");
        return;
    }

    QString eTag = g::getValue(m_headers, "If-None-Match", g::GetValueType::HttpHeader);

    if (urlPath() == "/~stats.txt")
    {
        httpDocument().setBody(Statistics::statsTxt(), eTag);
        httpDocument().setHeader(HttpDocument::CommonHeader::ContentType, HttpDocument::ContentType::TEXT);
    }
    else if (urlPath() == "/main.css")
    {
        httpDocument().setBody(QFile(":/html/main.css"), eTag);
    }
    else if (urlPath() == "/favicon.ico")
    {
        httpDocument().setBody(QFile(":/html/favicon.ico"), eTag);
    }
    else
    {
        httpDocument().setBody(QFile(g::p::WORKING_DIR + "/html" + urlPath()), eTag);
    }
}

void SocketRunnable::reader()
{
    QString reqType = getValueBeforeSpace();
    m_urlPath = QByteArray::fromPercentEncoding( getValueBeforeSpace().toUtf8() );
    if (m_urlPath.contains(".."))
    {
        // Possible attempt to explore server directories
        httpDocument().setCode(HttpDocument::Code::_404);
        httpDocument().setBodySingleHTMLLabel("What you where looking for is not here", "Not found");
        return;
    }

    if (m_urlPath == "/")
    {
        m_urlPath = "/index.html";
    }
    m_headers = getValueBeforeBody();

    if (reqType == "GET")
    {
        get();
    }
    else if (reqType != "HEAD")
    {
        httpDocument().setCode(HttpDocument::Code::_400);
    }

    setDataToWrite(httpDocument().document());
}

QString SocketRunnable::getValueBeforeSpace()
{
    QString result;
    char symbol {0};
    qint64 tail = socket()->read(&symbol, 1);
    while (symbol != ' ' and tail > 0)
    {
        if (result.size() > getValueBeforeSpaceLIMIT)
        {
            qDebug() << "Parsing error: TYPE or URL length >" << getValueBeforeSpaceLIMIT;
            break;
        }
        result += symbol;
        tail = socket()->read(&symbol, 1);
    }
    return result;
}

QString SocketRunnable::getValueBeforeBody()
{
    QString result;
    char symbol {0};
    qint64 tail = socket()->read(&symbol, 1);
    while (not result.endsWith("\r\n\r") and tail > 0)
    {
        if (result.size() > getValueBeforeBodyLIMIT)
        {
            qDebug() << "Parsing error: headers length >" << getValueBeforeBodyLIMIT;
            break;
        }
        result += symbol;
        tail = socket()->read(&symbol, 1);
    }
    return result;
}
