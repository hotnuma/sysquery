TEMPLATE = app
TARGET = sysquery
CONFIG = c99 link_pkgconfig
DEFINES =
INCLUDEPATH =
PKGCONFIG =

PKGCONFIG += tinyc
#PKGCONFIG += gtk+-3.0
#PKGCONFIG += glib-2.0

HEADERS = \
    pkg_dev.h \
    prc_query.h \
    str_ext.h \
    svc_query.h

SOURCES = \
    0Temp.c \
    main.c \
    pkg_dev.c \
    prc_query.c \
    str_ext.c \
    svc_query.c

DISTFILES = \
    install.sh \
    meson.build \


