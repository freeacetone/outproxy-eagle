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
#include "httpserver.h"
#include "statistics.h"
#include "dbmanager.h"
#include "g.h"

#include <QCoreApplication>
#include <QProcess>
#include <QDebug>
#include <QThread>
#include <iostream>
#include <signal.h>

void terminate(int)
{
    std::cout << "Terminating..." << std::endl;
    for (const auto& instanse: g::instanses)
    {
        instanse.second->deleteLater();
        instanse.first->terminate();
    }
    std::exit(0);
}

void usage()
{
    std::cout << g::c::SOFTWARE_NAME.toStdString() << " " << g::c::SOFTWARE_VERSION.toStdString() << std::endl
              << "Accumulate ethical 3proxy statistics with web interface\n\n"

                 "U S A G E:\n"
                 "  -i  --instanse                  <3proxy>,<3proxy.cfg>\n"
                 "  -w  --working-directory         <data>\n"
                 "  -t  --service-title             <3proxy-eagle>\n"
                 "  -I  --ignored-destinations      <[0.0.0.0],0.0.0.0>\n"
                 "  -a  --bind-to-address           <127.0.0.1>\n"
                 "  -p  --bind-to-port              <8161>\n"
                 "  -l  --log-level                 <info> (off, error, warn, info, debug)\n"
                 "  -s  --top-lists-size            <10>\n"
                 "  -D  --reject-ip-from-statistics\n\n"

                 "N O T E S:\n"
                 "* Multi instanses supported. Just pass new one --instanse value!\n"
                 "* Main 3proxy cfg must contain log to stdout with strict format:\n"
                 "    log\n"
                 "    logformat \" type=%N destination=%n to=%O from=%I\"\n"
                 "  Also for normal logging by 3proxy main config should contain\n"
                 "    fakeresolve\n"
                 "  otherwise 3proxy may be very uninformative.\n"
                 "  Check \"outproxy_config\" folder as working example.\n"
                 "* 3proxy cfg can contain blocked domains in format:\n"
                 "    {{vk.com,mail.ru,google.com}}\n"
                 "  This domains will be resolved automatically and replaced by:\n"
                 "    deny * * original.domain\n"
                 "    deny * * 8.8.8.8 # resolved addresses\n"
                 "    deny * * 1.1.1.1\n"
                 "* If the working directory contains a information.html, the Information\n"
                 "  box will be added to the web page. The block is full html-formatted.\n"
                 "* The html folder can contain any files, they will be available\n"
                 "  for downloading through the web browser.\n"
                 "* html folder can contain styles.css file for overloading default styles.\n"
                 "  See page source via web browser to customize CSS classes.\n"

                 "\n" << g::c::COPYRIGHT.toStdString() << std::endl;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, terminate);
    signal(SIGTERM, terminate);

    qInstallMessageHandler(g::customMessageOutput);

    QCoreApplication a(argc, argv);

    QList<QPair<QString,QString>> instanses;

    for (int i = 1; i < argc; i++)
    {
        QString key(argv[i]);
        QString value;
        if (i+1 < argc)
        {
            value = argv[i+1];
        }

        if (key == "--log-level" and not value.isEmpty())
        {
            if (value.contains("off", Qt::CaseInsensitive))
            {
                g::logLevel = g::LogLevel::Off;
            }
            else if (value.contains("error", Qt::CaseInsensitive))
            {
                g::logLevel = g::LogLevel::Error;
            }
            else if (value.contains("warn", Qt::CaseInsensitive))
            {
                g::logLevel = g::LogLevel::Warning;
            }
            else if (value.contains("info", Qt::CaseInsensitive))
            {
                g::logLevel = g::LogLevel::Info;
            }
            else if (value.contains("debug", Qt::CaseInsensitive))
            {
                g::logLevel = g::LogLevel::Debug;
            }
            else
            {
                qWarning() << "Invalid log level flag:" << value << "(maybe you should read the --help)";
            }
        }

        else if ((key == "-i" or key == "--instanse") and not value.isEmpty())
        {
            QStringList splitted = value.split(',');
            if (splitted.size() != 2)
            {
                qWarning() << "--instanse parsing failed:" << value;
                continue;
            }
            instanses.push_back( {splitted.front(), splitted.back()} );
        }

        else if ((key == "-w" or key == "--working-directory") and not value.isEmpty())
        {
            g::p::WORKING_DIR = value;
        }

        else if ((key == "-I" or key == "--ignored-destinations") and not value.isEmpty())
        {
            g::p::IGNORED_DESTINATIONS = value.split(',');
            for (auto& string: g::p::IGNORED_DESTINATIONS)
            {
                string.remove(' ');
            }
        }

        else if ((key == "-b" or key == "--bind-to-address") and not value.isEmpty())
        {
            g::p::BIND_TO_ADDRESS = value;
        }

        else if ((key == "-p" or key == "--bind-to-port") and not value.isEmpty())
        {
            bool ok = false;
            value.toUShort(&ok);
            if (not ok)
            {
                qWarning() << "--bind-to-port parsing failed, incorrect port value:" << value;
                continue;
            }
            g::p::BIND_TO_PORT = value.toUShort();
        }

        else if ((key == "-t" or key == "--service-title") and not value.isEmpty())
        {
            g::p::SERVICE_TITLE = value;
        }

        else if ((key == "-s" or key == "--top-lists-size") and not value.isEmpty())
        {
            bool ok = false;
            value.toUInt(&ok);
            if (not ok)
            {
                qWarning() << "--top-lists-size parsing failed, incorrect unsigned integer" << value;
                continue;
            }
            g::p::LAST_AND_TOP_LIST_SIZE = value.toUInt();
        }

        else if (key == "-D" or key == "--reject-ip-from-statistics")
        {
            g::p::IGNORE_IP_ADDRS_IN_STATISTCS = true;
        }

        else if (key == "-h" or key == "--help")
        {
            usage();
            return 0;
        }
    }

    if (instanses.isEmpty())
    {
        qFatal("Instanses not defined. Read --help information");
    }

    DBManager().initAtStart();
    Statistics::initTrafficCounters();

    for (const auto& pair: instanses)
    {
        QThread* thread = new QThread;
        ProxyInstanse* pi = new ProxyInstanse;
        pi->set3proxyBinaryFile(pair.first);
        pi->set3proxyConfigFile(pair.second);
        pi->moveToThread(thread);
        QObject::connect(thread, &QThread::started, pi, &ProxyInstanse::start);
        thread->start();
        g::instanses.push_back( {thread, pi} );
    }

    qInfo().noquote() << "Instanses count:" << g::instanses.size();
    qInfo().noquote() << "Working directory:" << g::p::WORKING_DIR;
    qInfo().noquote() << "Ignored destinations:" << g::p::IGNORED_DESTINATIONS;
    qInfo().noquote() << "Top lists size:" << g::p::LAST_AND_TOP_LIST_SIZE;
    qInfo() << "Reject IP addresses from statistics:" << g::p::IGNORE_IP_ADDRS_IN_STATISTCS;

    (new HttpServer)/*->killTheCapitalism()*/;

    return a.exec();
}
