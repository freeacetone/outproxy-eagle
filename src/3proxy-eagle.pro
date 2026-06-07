QT -= gui
QT += network sql

CONFIG += c++17 console
CONFIG -= app_bundle

SOURCES += \
        dbmanager.cpp \
        g.cpp \
        httpdocument.cpp \
        httpserver.cpp \
        main.cpp \
        proxyinstanse.cpp \
        socketrunnable.cpp \
        socketrunnablebase.cpp \
        statistics.cpp \
        webpage.cpp \
        webserverbase.cpp

HEADERS += \
    dbmanager.h \
    g.h \
    httpdocument.h \
    httpserver.h \
    proxyinstanse.h \
    socketrunnable.h \
    socketrunnablebase.h \
    statistics.h \
    webpage.h \
    webserverbase.h

RESOURCES += \
    html-static.qrc
