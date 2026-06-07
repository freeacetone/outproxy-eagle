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

#include "httpdocument.h"

#include <QRunnable>
#include <QTcpSocket>
#include <QObject>

/* 1) override reader(); 2) call setDataToWrite() at and of reader() */

class SocketRunnableBase : public QObject, public QRunnable
{
    Q_OBJECT
public:
    SocketRunnableBase(qintptr, QObject* parent = nullptr);
    ~SocketRunnableBase();
    void run() override;
    virtual void reader() = 0;
    void setDataToWrite(const QByteArray& data);
    QTcpSocket *socket();

private:
    qintptr m_socketDescriptor;
    QTcpSocket* m_socket = nullptr;
    QByteArray m_data;
};
