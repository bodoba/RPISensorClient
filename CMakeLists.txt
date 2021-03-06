# *********************************************************************************** *
# Copyright 2017 by Bodo Bauer <bb@bb-zone.com>
#
# This file is part of the RPI Sensor Client
#
# PRISensorClient is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# PRISensorClient is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with ReadDHT11.  If not, see <http://www.gnu.org/licenses/>.
# *********************************************************************************** *

include(CheckIncludeFile)
cmake_minimum_required(VERSION 3.5)
project (RPISensorClient)

find_library(LIB_MQTT mosquitto)
find_library(LIB_WIRING wiringPi)

set(CMAKE_C_FLAGS "-Wall -std=gnu11 -g")
set(CMAKE_INSTALL_PREFIX /)

# all executables end up in bin
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

add_executable(rpisensorclient RPISensorClient.c MQTT.c DHT11.c)

target_link_libraries(rpisensorclient "${LIB_MQTT}")
target_link_libraries(rpisensorclient "${LIB_WIRING}")

INSTALL(PROGRAMS bin/rpisensorclient DESTINATION usr/sbin)

add_subdirectory(Contrib)
