QT = core gui xml network testlib

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

TARGET = tst_untitledtest
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

HEADERS += syncdocument.h \
           syncpage.h \
           synctrack.h

SOURCES += tst_syncdocument.cpp \
           syncdocument.cpp \
           syncpage.cpp
