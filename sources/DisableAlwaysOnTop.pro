QT += widgets

CONFIG += C++11

SOURCES += \
    main.cpp

RESOURCES += \
    resource.qrc

TRANSLATIONS = \
    disablealwaysontop_fr.ts\
    disablealwaysontop_sv.ts

win32:RC_FILE = resource.rc
