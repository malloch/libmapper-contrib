#-------------------------------------------------
#
# Project created by QtCreator 2015-03-26T12:06:56
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

TARGET = SignalPlotter
TEMPLATE = app

CONFIG += c++11

SOURCES += main.cpp\
        signalplotter.cpp \
    qcustomplot.cpp

HEADERS  += signalplotter.h \
    qcustomplot.h

FORMS    += signalplotter.ui

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../../../../../usr/local/lib/release/ -llo
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../../../../../usr/local/lib/debug/ -llo
else:unix: LIBS += -L$$PWD/../../../../../../../usr/local/lib/ -llo

INCLUDEPATH += $$PWD/../../../../../../../usr/local/include
DEPENDPATH += $$PWD/../../../../../../../usr/local/include

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../../../../../usr/local/lib/release/ -lmapper-0
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../../../../../usr/local/lib/debug/ -lmapper-0
else:unix: LIBS += -L$$PWD/../../../../../../../usr/local/lib/ -lmapper-0

INCLUDEPATH += $$PWD/../../../../../../../usr/local/include/mapper-0
DEPENDPATH += $$PWD/../../../../../../../usr/local/include/mapper-0
