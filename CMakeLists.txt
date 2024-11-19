# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})
# list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/ant_tpms)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_devel)

zephyr_include_directories(include)
# add_subdirectory("src/ant_tpms")

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/bluetooth.c)
target_sources(app PRIVATE src/ant.c)
target_sources(app PRIVATE src/ant_tpms/ant_tpms.c)
target_sources(app PRIVATE src/ant_tpms/pages/ant_tpms_page_1.c)
target_sources(app PRIVATE src/ant_tpms/simulator/ant_tpms_simulator.c)

# zephyr_library_include_directories(.)