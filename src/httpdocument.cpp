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

#include "httpdocument.h"
#include "g.h"

#include <QMapIterator>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QDebug>
#include <QDateTime>

const QString HttpDocument::Code::_200 = "200 OK";
const QString HttpDocument::Code::_303 = "303 See Other";
const QString HttpDocument::Code::_304 = "304 Not Modified";
const QString HttpDocument::Code::_400 = "400 Bad request";
const QString HttpDocument::Code::_403 = "403 Access Denied";
const QString HttpDocument::Code::_404 = "404 Not Found";
const QString HttpDocument::Code::_413 = "413 Payload Too Large";
const QString HttpDocument::Code::_500 = "500 Internal Error";

const QString HttpDocument::ContentType::HTML  = "text/html; charset=utf-8";
const QString HttpDocument::ContentType::TEXT  = "text/plain; charset=utf-8";
const QString HttpDocument::ContentType::CSS   = "text/css";
const QString HttpDocument::ContentType::SVG   = "image/svg+xml";
const QString HttpDocument::ContentType::ICO   = "image/ico";
const QString HttpDocument::ContentType::PNG   = "image/png";
const QString HttpDocument::ContentType::GIF   = "image/gif";
const QString HttpDocument::ContentType::JS    = "text/javascript; charset=utf-8";
const QString HttpDocument::ContentType::WOFF  = "font/woff";
const QString HttpDocument::ContentType::WOFF2 = "font/woff2";
const QString HttpDocument::ContentType::JSON  = "application/json; charset=utf-8";
const QString HttpDocument::ContentType::MP3   = "audio/mpeg";
const QString HttpDocument::ContentType::BIN   = "application/octet-stream";

const QString HttpDocument::CommonHeader::ContentType = "Content-Type";
const QString HttpDocument::CommonHeader::ContentLength = "Content-Length";
const QString HttpDocument::CommonHeader::ETag = "ETag";
const QString HttpDocument::CommonHeader::ProcessingDuration = "Processing-duration";
const QString HttpDocument::CommonHeader::SetCookie = "Set-Cookie";

HttpDocument::HttpDocument() :
    m_processingDurationStartMarker(QDateTime::currentMSecsSinceEpoch())
{
    setCode(Code::_200);
}

void HttpDocument::setCode(const QString &code)
{
    m_code = code;
}

void HttpDocument::setHeader(const QString &name, const QString &value)
{
    if (name.isEmpty() or value.isEmpty())
    {
        qWarning() << "HTTP header setting rejected (empty value):" << name << ";" << value;
        return;
    }

    m_headers.insert(name, value);
}

void HttpDocument::setBody(const QByteArray &body)
{
    m_body = body;
}

void HttpDocument::setBody(const QString &body, const QString &oldETag)
{
    QString eTag = g::hash(body.toUtf8());

    if (oldETag == eTag)
    {
        qDebug() << "Data cached by user (not file on disk) / Hash:" << oldETag;

        setCode(Code::_304);
        return;
    }

    setBody(body.toUtf8());
    setHeader(CommonHeader::ETag, eTag);
}

void HttpDocument::setBody(const QJsonObject &body)
{
    m_body = QJsonDocument(body).toJson(QJsonDocument::JsonFormat::Compact);
    setHeader(CommonHeader::ContentType, ContentType::JSON);
}

void HttpDocument::setBody(QFile&& payload, const QString& oldETag)
{
    if (not payload.exists())
    {
        qDebug() << "File" << payload.fileName() << "not found";

        setCode(Code::_404);
        setBodySingleHTMLLabel("Not found", "Not found");
        return;
    }

    if (not payload.open(QIODevice::ReadOnly))
    {
        qDebug() << "File" << payload.fileName() << "reading failed";

        setCode(Code::_500);
        setBodySingleHTMLLabel("Not available", "Can't read");
        return;
    }

    QByteArray fileBytes = payload.readAll();
    payload.close();

    QString eTag = g::hash(fileBytes);

    if (oldETag == eTag)
    {
        qDebug() << "File cached by user:" << payload.fileName() << "/ Hash:" << oldETag;

        setCode(Code::_304);
        return;
    }

    setBody(fileBytes);
    setHeader(CommonHeader::ETag, eTag);

    setHeader(CommonHeader::ContentType,
              payload.fileName().endsWith(".html")  ? ContentType::HTML :
              payload.fileName().endsWith(".css")   ? ContentType::CSS :
              payload.fileName().endsWith(".js")    ? ContentType::JS :
              payload.fileName().endsWith(".ico")   ? ContentType::ICO :
              payload.fileName().endsWith(".svg")   ? ContentType::SVG :
              payload.fileName().endsWith(".png")   ? ContentType::PNG :
              payload.fileName().endsWith(".woff")  ? ContentType::WOFF :
              payload.fileName().endsWith(".woff2") ? ContentType::WOFF2 :
              payload.fileName().endsWith(".json")  ? ContentType::JSON :
              payload.fileName().endsWith(".mp3")   ? ContentType::MP3 :
              payload.fileName().endsWith(".gif")   ? ContentType::GIF :
              payload.fileName().endsWith(".txt")   ? ContentType::TEXT :
                                                      ContentType::BIN);
}

void HttpDocument::setBodySingleHTMLLabel(const QString &text, const QString& title)
{
    m_body = "<!DOCTYPE html>\r\n"
             "<head>\r\n"
             "    <title>"+title.toUtf8()+"</title>\r\n"
             "</head>\r\n"
             "<style>\r\n"
             "    * { font-family: monospace }\r\n"
             "</style>\r\n"
             "<center>\r\n"
             "    <h1><p>"+text.toUtf8()+"</p></h1>\r\n"
             "    <h2><p>[<a href=\"/\" style=\"text-decoration: none\">back to main</a>]</p></h2>\r\n"
             "    <br><br>\r\n"
             "</p>\r\n"
              "    <p style=\"color: gray\">3PROXY-EAGLE</p>\r\n"
              "</center>";
}

QByteArray HttpDocument::document() const
{
    static constexpr const char HTML_NEWLINE[] {"\r\n"};

    QByteArray result = "HTTP/1.0 " + code().toUtf8() + HTML_NEWLINE;

    QMapIterator<QString, QString> iter( headers() );
    while (iter.hasNext())
    {
        iter.next();
        result += (iter.key()+": "+iter.value()+HTML_NEWLINE).toUtf8();
    }

    result += QString( CommonHeader::ContentLength + ": " +
              QString::number( body().size()).toUtf8() ).toUtf8() + HTML_NEWLINE;
    result += QString( CommonHeader::ProcessingDuration + ": " +
              QString::number( QDateTime::currentMSecsSinceEpoch()-m_processingDurationStartMarker) ).toUtf8() + HTML_NEWLINE;
    result += HTML_NEWLINE;

    result += body();

    return result;
}
