cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

set(ACROMAGE_SOURCES Arcomage.cpp
                     ArcomageCards.cpp)

set(ACROMAGE_HEADERS Arcomage.h)

if(WIN32)
else()
endif()

add_library(arcomage STATIC ${ACROMAGE_SOURCES} ${ACROMAGE_HEADERS})
target_check_style(arcomage)