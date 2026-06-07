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

#include "socketrunnablebase.h"
#include "httpdocument.h"

class SocketRunnable : public SocketRunnableBase
{
    Q_OBJECT
public:
    SocketRunnable(qintptr, QObject* parent = nullptr);

    // SocketRunnableBase interface
public:
    void reader() override;

private:
    HttpDocument& httpDocument() { return m_httpDocument; }
    HttpDocument m_httpDocument;

    QString m_urlPath;
    const QString& urlPath() const { return m_urlPath; }

    QString m_headers;
    const QString& headers() const { return m_headers; }

    void get();

    QString getValueBeforeSpace();
    QString getValueBeforeBody();
};
