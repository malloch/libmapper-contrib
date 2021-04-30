#-------------------------------------------------
#
# Project created by QtCreator 2015-03-26T12:06:56
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

TARGET = SignalPlotter
TEMPLATE = app

CONFIG      += no_keywords #signals

CONFIG += c++11

SOURCES += main.cpp\
        signalplotter.cpp \
    qcustomplot.cpp

HEADERS  += signalplotter.h \
    qcustomplot.h

FORMS    += signalplotter.ui

#win32:CONFIG(release, debug|release): LIBS += -L/usr/local/lib/release/ -llo
#else:win32:CONFIG(debug, debug|release): LIBS += -L/usr/local/lib/debug/ -llo
#else:unix: LIBS += -L/usr/local/lib/ -llo

INCLUDEPATH += /usr/local/include
DEPENDPATH += /usr/local/include

win32:CONFIG(release, debug|release): LIBS += -L/usr/local/lib/release/ -lmapper
else:win32:CONFIG(debug, debug|release): LIBS += -L/usr/local/lib/debug/ -lmapper
else:unix: LIBS += -L/usr/local/lib/ -lmapper

INCLUDEPATH += /usr/local/include/mapper
DEPENDPATH += /usr/local/include/mapper
