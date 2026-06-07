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

#include "webserverbase.h"
#include "g.h"

#include <QDebug>

WebServerBase::WebServerBase(QObject *parent) :
    QTcpServer(parent)
{
    m_pool = new QThreadPool(this);
    auto threadCount = std::thread::hardware_concurrency() * 2;
    m_pool->setMaxThreadCount(threadCount);

    if (not listen(QHostAddress(g::p::BIND_TO_ADDRESS), g::p::BIND_TO_PORT))
    {
        throw std::runtime_error("Web server not binded at " +
                                 g::p::BIND_TO_ADDRESS.toStdString() + " / " +
                                 QString::number(g::p::BIND_TO_PORT).toStdString());
    }
    qInfo() << "Web server started at" << g::p::BIND_TO_ADDRESS << "/" << g::p::BIND_TO_PORT;
}

WebServerBase::~WebServerBase()
{
    if (m_pool)
    {
        m_pool->clear();
        if (not m_pool->waitForDone(100))
        {
            qWarning() << "Thread pool of web server not finished at 100ms";
        }
        m_pool->deleteLater();
    }
}

QThreadPool *WebServerBase::pool()
{
    return m_pool;
}
