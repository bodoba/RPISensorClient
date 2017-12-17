# main

include(CheckIncludeFile)
cmake_minimum_required(VERSION 3.6)
project (RPISensorClient)

set (CMAKE_C_FLAGS "-Wall -std=gnu11 -lwiringPi -lmosquitto")

# all executables end up in bin
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

add_executable(RPISensorClient RPISensorClient.c MQTT.c)

install(TARGETS RPISensorClient DESTINATION bin)
