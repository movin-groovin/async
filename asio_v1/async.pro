TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt


QMAKE_CXXFLAGS += -std=c++14 -pthread

LIBS += -lboost_thread -lboost_program_options -lboost_system -lpthread



SOURCES += main.cpp \
    server.cpp \
    cconnection.cpp \
    request_handler.cpp

HEADERS += \
    server.hpp \
    logger.hpp \
    cconnection.h \
    request_handler.h

