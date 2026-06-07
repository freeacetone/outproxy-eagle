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

#include "proxyinstanse.h"
#include "statistics.h"
#include "g.h"

#include <QRegularExpression>
#include <QHostInfo>
#include <QDebug>
#include <QFile>

ProxyInstanse::ProxyInstanse(QObject* parent)
    : QObject(parent)
{

}

ProxyInstanse::~ProxyInstanse()
{
    qInfo() << "Proxy instanse" << g::hash(confPath().toUtf8()) << "terminated";

    if (m_process)
    {
        m_process->terminate();
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                m_process, &QProcess::deleteLater);
    }
}

void ProxyInstanse::set3proxyBinaryFile(const QString &pathTo3proxyBinFile)
{
    if (m_process)
    {
        qWarning() << "ProxyInstanse::set3proxyBinaryFile() process already started";
    }
    if (not QFile::exists(pathTo3proxyBinFile))
    {
        qCritical() << "ProxyInstanse::set3proxyBinaryFile()" << pathTo3proxyBinFile << "not exists";
    }
    m_binPath = pathTo3proxyBinFile;
}

void ProxyInstanse::set3proxyConfigFile(const QString &pathTo3proxyConfFile)
{
    if (m_process)
    {
        qWarning() << "ProxyInstanse::set3proxyConfigFile() process already started";
    }

    if (not QFile::exists(pathTo3proxyConfFile))
    {
        qCritical() << "ProxyInstanse::set3proxyConfigFile()" << pathTo3proxyConfFile << "not exists";
    }

    m_confPath = pathTo3proxyConfFile;

    createConfigurationFile();
}

void ProxyInstanse::generateBlackList(const QStringList &domainsOrAddresses)
{
    qDebug() << "ProxyInstanse::generateBlackList() raw:" << domainsOrAddresses;

    QStringListIterator iterRaw(domainsOrAddresses);
    while (iterRaw.hasNext())
    {
        QString name = iterRaw.next();
        m_blackList.insert(name, QStringList());
        static QRegularExpression rgx_hiddenNetworks("^.*\\.(i2p|onion|loki)$");
        if (name.contains(rgx_hiddenNetworks))
        {
            qDebug() << "Domain" << name << "is hidden network name, no attempt to resolve";
            continue;
        }

        name.remove(' ');

        QHostInfo host = QHostInfo::fromName(name);
        if (host.error() != QHostInfo::NoError)
        {
            qDebug() << "Can not resolve" << name << ":" << host.errorString();
            continue;
        }
        else if (host.addresses().first().toString() == name)
        {
            qDebug() << name << "not a domain name (it's normal, just debug info)";
            continue;
        }

        auto resolved = host.addresses();
        for (auto it = resolved.begin(); it != resolved.end(); ++it)
        {
            QString address = it->toString();
            qDebug() << "Domain" << name << "resolved to" << address;
            m_blackList[name].push_back(address);
        }
    }
}

QString ProxyInstanse::workingConfigPath() const
{
    return g::p::WORKING_DIR + "/" + g::hash(confPath()) + ".cfg";
}

void ProxyInstanse::start()
{
    m_process = new QProcess;
    m_process->setProgram(binPath());
    m_process->setArguments(QStringList() << workingConfigPath());

    connect (m_process, &QProcess::readyReadStandardOutput, this, &ProxyInstanse::reader, Qt::DirectConnection);
    connect (m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &ProxyInstanse::restart);

    qDebug() << "Proxy instanse starting";
    m_process->start();
    connect (m_process, &QProcess::started, [](){
        qDebug() << "Proxy instanse started";
    });
}

void ProxyInstanse::reader()
{
    QMutexLocker lock (&m_readerMtx);

    m_readBuffer = m_process->readAll();

    bool lastStringIsUncomplete = not m_readBuffer.endsWith('\n');
    QStringList completeLines = m_readBuffer.split('\n');

    if (lastStringIsUncomplete)
    {
        m_readBuffer = completeLines.last();
        completeLines.pop_back();
    }

    for (const auto& str: completeLines)
    {
        if (str.isEmpty()) continue;
        QString type = g::getValue(str, "type");
        QString dest = g::getValue(str, "destination");
        quint64 to = g::getValue(str, "to").toULongLong();
        quint64 from = g::getValue(str, "from").toULongLong();

        if (type.isEmpty() or dest.isEmpty())
        {
            qWarning() << "Receive incorrect log format. Please read --help for usage information";
            continue;
        }

        qDebug() << "Type:" << type
                 << "dest:" << dest
                 << "to:" << to
                 << "from:" << from;

        Statistics::report( {type, dest, to, from} );
    }
}

void ProxyInstanse::restart()
{
    qCritical() << "Called because process unexpected finished";
    m_process->deleteLater();
    start();
}

void ProxyInstanse::createConfigurationFile()
{
    QFile original (confPath());
    if (not original.open(QIODevice::ReadOnly))
    {
        qFatal("Can not read original cfg");
        return;
    }
    QString originalFile = original.readAll();
    original.close();

    static QRegularExpression rgx_domainsPattern ("^.*\\{\\{.*\\}\\}.*", QRegularExpression::DotMatchesEverythingOption);
    if (originalFile.contains(rgx_domainsPattern))
    {
        QString rawDomainsList = originalFile;
        static QRegularExpression rgx_listBegin ("^.*\\{\\{", QRegularExpression::DotMatchesEverythingOption);
        rawDomainsList.remove(rgx_listBegin);
        static QRegularExpression rgx_listEnd ("\\}\\}.*$", QRegularExpression::DotMatchesEverythingOption);
        rawDomainsList.remove(rgx_listEnd);
        if (rawDomainsList.isEmpty())
        {
            qWarning() << "Blocked domains pattern detected, but list is empty in" << confPath();
        }
        else
        {
            generateBlackList(rawDomainsList.split(','));

            QString blockedDomainsConfigPart;
            QMapIterator<QString, QStringList> mapIterator(m_blackList);
            while (mapIterator.hasNext())
            {
                auto currentNode = mapIterator.next();
                blockedDomainsConfigPart += "deny * * " + currentNode.key() + '\n';
                for (const auto& addr: currentNode.value())
                {
                    blockedDomainsConfigPart += "deny * * " + addr + '\n';
                }
            }

            static QRegularExpression rgx_domains ("\\{\\{.*\\}\\}", QRegularExpression::DotMatchesEverythingOption);
            originalFile.replace(rgx_domains, blockedDomainsConfigPart);
        }
    }

    QFile tmpConfig (workingConfigPath());
    if (not tmpConfig.open(QIODevice::WriteOnly))
    {
        qFatal("Can not write working config");
        return;
    }

    auto cfgByteArray = originalFile.toUtf8();
    int cfgSize = cfgByteArray.size();
    int writed = tmpConfig.write(cfgByteArray);
    if (cfgSize != writed)
    {
        qCritical() << "Write error:" << writed << "/" << cfgSize;
    }
    tmpConfig.close();
}
