# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# include board definition
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})

###
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sks_airspy)

zephyr_include_directories(include)

## app target
# app sources
target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/common.c)
target_sources(app PRIVATE src/bluetooth.c)
target_sources(app PRIVATE src/ant.c)
target_sources(app PRIVATE src/retained.c)
target_sources(app PRIVATE src/spi.c)
target_sources(app PRIVATE src/zbus_com.c)
target_sources(app PRIVATE src/sensor.c)

# ant tpms profile sources
target_sources(app PRIVATE src/ant_profiles/ant_tpms/ant_tpms.c)
target_sources(app PRIVATE src/ant_profiles/ant_tpms/pages/ant_tpms_page_1.c)
target_sources(app PRIVATE src/ant_profiles/ant_common/pages/ant_common_page_82.c)
