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

#include "httpserver.h"
#include "socketrunnable.h"

HttpServer::HttpServer(QObject *parent) :
    WebServerBase(parent)
{

}

void HttpServer::killTheCapitalism()
{
    qWarning() << "Try to kill capitalism...";
    QThread::sleep(1);
    qWarning() << "Pick up black flag...";
    QThread::sleep(1);
    qWarning() << "Hmmm. Timed out. Try later with friends!";
}

void HttpServer::incomingConnection(qintptr handle)
{
    SocketRunnable* socket = new SocketRunnable(handle);
    socket->setAutoDelete(true);
    pool()->start(socket);
}
