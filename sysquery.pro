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
    prc_query.h \
    query.h

SOURCES = \
    0Temp.c \
    main.c \
    prc_query.c \
    query.c

DISTFILES = \
    install.sh \
    meson.build \


