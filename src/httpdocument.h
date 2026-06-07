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
#include <QMap>
#include <QJsonObject>
#include <QFile>

class HttpDocument
{
public:
    struct CommonHeader
    {
        static const QString ContentType;
        static const QString ContentLength;
        static const QString ETag;
        static const QString ProcessingDuration;
        static const QString SetCookie;
    };

    struct ContentType
    {
        static const QString HTML;
        static const QString TEXT;
        static const QString CSS;
        static const QString SVG;
        static const QString ICO;
        static const QString PNG;
        static const QString GIF;
        static const QString JS;
        static const QString WOFF;
        static const QString WOFF2;
        static const QString JSON;
        static const QString MP3;
        static const QString BIN;
    };

    struct Code {
        static const QString _200;
        static const QString _303;
        static const QString _304;
        static const QString _400;
        static const QString _403;
        static const QString _404;
        static const QString _413;
        static const QString _500;
    };

    HttpDocument();
    void setCode(const QString& code = Code::_200);
    QString code() const { return m_code; }

    void setHeader(const QString& name, const QString& value);
    const QMap<QString, QString>& headers() const { return m_headers; }

    void setBody(const QByteArray& body);
    void setBody(const QJsonObject& body);
    void setBody(QFile&& payload, const QString& eTag = QString());
    void setBody(const QString& body, const QString& eTag);
    void setBodySingleHTMLLabel(const QString& text, const QString& title = "Notice");
    QByteArray body() const { return m_body; }

    QByteArray document() const;

private:
    QString m_code = Code::_200;
    QMap<QString, QString> m_headers;
    QByteArray m_body;

    const qint64 m_processingDurationStartMarker;
};
