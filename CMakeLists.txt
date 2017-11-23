# main

include(CheckIncludeFile)
cmake_minimum_required(VERSION 3.6)
project (ReadDHT11)

set (CMAKE_C_FLAGS "-Wall -lwiringPi -lmosquitto")

# all executables end up in bin
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

add_executable(ReadDHT11 ReadDHT11.c DHT11.c MQTT.c)

install(TARGETS ReadDHT11 DESTINATION bin)
