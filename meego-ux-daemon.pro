VERSION = 0.2.28

QT += declarative opengl network dbus
CONFIG += mobility link_pkgconfig
MOBILITY += sensors
PKGCONFIG += \
    xscrnsaver \
    gconf-2.0 \
    mlite \
    contextprovider-1.0 \
    xrandr \
    libcgroup \
    xext \
    libpulse \
    libpulse-mainloop-glib \
    meegolocale
TARGET = meego-ux-daemon
TEMPLATE = app
SOURCES += main.cpp \
    application.cpp \
    dialog.cpp \
    desktop.cpp \
    process.cpp \
    notificationsmanageradaptor.cpp \
    notificationdatastore.cpp \
    statusindicatormenuadaptor.cpp \
    notificationmodel.cpp \
    volumecontrol.cpp \
    lockscreenadaptor.cpp \
    atoms.cpp \
    panelview.cpp
HEADERS += \
    application.h \
    dialog.h \
    windowinfo.h \
    desktop.h \
    process.h \
    notificationsmanageradaptor.h \
    notificationdatastore.h \
    statusindicatormenuadaptor.h \
    notificationitem.h \
    notificationmodel.h \
    volumecontrol.h \
    lockscreenadaptor.h \
    atoms.h \
    panelview.h

OBJECTS_DIR = .obj
MOC_DIR = .moc

target.files += $$TARGET start-meego-ux
target.path += $$INSTALL_ROOT/usr/libexec/

share.files += *.qml
share.path += $$INSTALL_ROOT/usr/share/$$TARGET

desktop.files += meego-ux-daemon.desktop
desktop.path += $$INSTALL_ROOT/etc/xdg/autostart

context.files += *.context
context.path += $$INSTALL_ROOT/usr/share/contextkit/providers

dbusconfig.files += meego-ux-daemon.conf
dbusconfig.path += $$INSTALL_ROOT/etc/dbus-1/system.d

INSTALLS += target share desktop context dbusconfig

OTHER_FILES += \
    taskswitcher.qml \
    lockscreen.qml \
    interfaces/notificationmanager.xml \
    com.meego.meego-ux-daemon.context \
    statusindicatormenu.qml \
    NotificationDelegate.qml \
    real.qml \
    start-meego-ux

TRANSLATIONS += $${SOURCES} $${HEADERS} $${OTHER_FILES}
PROJECT_NAME = meego-ux-daemon

dist.commands += rm -fR $${PROJECT_NAME}-$${VERSION} &&
dist.commands += git clone . $${PROJECT_NAME}-$${VERSION} &&
dist.commands += rm -fR $${PROJECT_NAME}-$${VERSION}/.git &&
dist.commands += mkdir -p $${PROJECT_NAME}-$${VERSION}/ts &&
dist.commands += lupdate $${TRANSLATIONS} -ts $${PROJECT_NAME}-$${VERSION}/ts/$${PROJECT_NAME}.ts &&
dist.commands += tar jcpvf $${PROJECT_NAME}-$${VERSION}.tar.bz2 $${PROJECT_NAME}-$${VERSION}
QMAKE_EXTRA_TARGETS += dist
