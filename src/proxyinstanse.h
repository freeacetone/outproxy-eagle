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

#include <QProcess>
#include <QMap>
#include <QMutex>
#include <QObject>

class ProxyInstanse : public QObject
{
    Q_OBJECT
public:
    ProxyInstanse(QObject* parent = nullptr);
    ~ProxyInstanse();
    void set3proxyBinaryFile(const QString& pathTo3proxyBinFile);
    void set3proxyConfigFile(const QString& pathTo3proxyConfFile);

    QString binPath() const { return m_binPath; }
    QString confPath() const { return m_confPath; }
    QMap<QString, QStringList> blackList() const { return m_blackList; }

public slots:
    void start();

private slots:
    void reader();
    void restart();

private:
    void createConfigurationFile();
    void generateBlackList(const QStringList& domainsOrAddresses);
    QString workingConfigPath() const;

    QString m_binPath;
    QString m_confPath;
    QProcess* m_process = nullptr;
    QMap<QString, QStringList> m_blackList; // domain, addresses

    QMutex m_readerMtx;
    QString m_readBuffer;
};
