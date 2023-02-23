TEMPLATE = app
TARGET = editor
DEPENDPATH += .

QT = core gui xml network widgets

qtHaveModule(websockets): QT += websockets

!contains(QT, websockets): message("QWebSockets module not found, disabling websocket support...")

# Input
HEADERS += syncclient.h \
    mainwindow.h \
    syncdocument.h \
    synctrack.h \
    trackview.h \
    syncpage.h

SOURCES += syncclient.cpp \
    editor.cpp \
    mainwindow.cpp \
    syncdocument.cpp \
    trackview.cpp \
    syncpage.cpp

RESOURCES += editor.qrc

RC_FILE = editor.rc
ICON = appicon.icns

target.path = $${PREFIX}/bin/
INSTALLS += target
