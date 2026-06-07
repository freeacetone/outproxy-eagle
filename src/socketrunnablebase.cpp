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

#include "socketrunnablebase.h"

#include <QDebug>

SocketRunnableBase::SocketRunnableBase(qintptr socketDescriptor, QObject *parent) :
    QObject(parent),
    m_socketDescriptor(socketDescriptor)
{
}

SocketRunnableBase::~SocketRunnableBase()
{
    if (m_socket) delete m_socket;
}

void SocketRunnableBase::setDataToWrite(const QByteArray &data)
{
    m_data = data;
}

QTcpSocket *SocketRunnableBase::socket()
{
    return m_socket;
}

void SocketRunnableBase::run()
{
    if (not m_socketDescriptor)
    {
        return;
    }

    m_socket = new QTcpSocket;
    m_socket->setSocketDescriptor(m_socketDescriptor);

    if (m_socket->waitForReadyRead())
    {
        reader();

        if (not m_data.isEmpty())
        {
            m_socket->write(m_data);
            while (m_socket->bytesToWrite() > 0)
            {
                if (m_socket->state() == QTcpSocket::UnconnectedState)
                {
                    qWarning() << "Socket turned to unconnected state at writing moment";
                    break;
                }
                m_socket->waitForBytesWritten(100);
            }
            m_socket->close();
        }
        else
        {
            qWarning() << "Data to write is empty";
        }
    }
    else
    {
        qWarning() << "Socket reading timed out";
    }
}
