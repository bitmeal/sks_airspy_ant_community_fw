# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/ant_tpms)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_devel)

zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR}/include)
# add_subdirectory("src/ant_tpms")

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/bluetooth.c)
target_sources(app PRIVATE src/ant.c)

zephyr_library_include_directories(.)