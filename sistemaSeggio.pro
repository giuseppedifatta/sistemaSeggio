
QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets


QMAKE_CXXFLAGS += -g \
-O1 \
-Wextra

TARGET = sistemaSeggio
TEMPLATE = app

FORMS += \
    mainwindowseggio.ui

HEADERS += \
    mainwindowseggio.h \
    seggio.h \
    associazione.h \
    sslclient.h \
    sslserver.h \
    aggiornamentostatopv.h \
    hardwaretoken.h \
    risultatiSeggio.h \
    schedavoto.h \
    candidato.h \
    listaelettorale.h \
    conf.h

SOURCES += \
    mainwindowseggio.cpp \
    main.cpp \
    seggio.cpp \
    associazione.cpp \
    sslclient.cpp \
    sslserver.cpp \
    hardwaretoken.cpp \
    risultatiSeggio.cpp \
    schedavoto.cpp \
    candidato.cpp \
    listaelettorale.cpp \
    conf.cpp



win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../../usr/lib/x86_64-linux-gnu/release/ -lssl
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../../usr/lib/x86_64-linux-gnu/debug/ -lssl
else:unix: LIBS += -L$$PWD/../../../../usr/lib/x86_64-linux-gnu/ -lssl

INCLUDEPATH += $$PWD/../../../../usr/lib/x86_64-linux-gnu
DEPENDPATH += $$PWD/../../../../usr/lib/x86_64-linux-gnu

LIBS += \
    -lssl \
    -lcrypto \
    -lcryptopp



win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../../usr/lib/x86_64-linux-gnu/release/ -lcryptopp
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../../usr/lib/x86_64-linux-gnu/debug/ -lcryptopp
else:unix: LIBS += -L$$PWD/../../../../usr/lib/x86_64-linux-gnu/ -lcryptopp


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../../usr/lib/i386-linux-gnu/release/ -ltinyxml2
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../../usr/lib/i386-linux-gnu/debug/ -ltinyxml2
else:unix: LIBS += -L$$PWD/../../../../usr/lib/i386-linux-gnu/ -ltinyxml2
