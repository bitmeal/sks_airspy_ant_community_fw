# Copyright (c) 2026 Arne Wendt (@bitmeal)
# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.20.0)

### runner configuration and injection for all boards
# board_set_runner(FLASH blackmagicprobe)
set(BOARD_FLASH_RUNNER blackmagicprobe)
# board_set_runner(DEBUG blackmagicprobe)
set(BOARD_DEBUG_RUNNER blackmagicprobe)

macro(app_set_runner_args)
    if(NOT EXECUTED_APP_SET_RUNNER_ARGS__SKS_AIRSPY_COMMUNITY)
        # set guard in current scope to prevent recursion
        set(EXECUTED_APP_SET_RUNNER_ARGS__SKS_AIRSPY_COMMUNITY TRUE)
        # set guard in parent scope to prevent multiple additions to runners.yaml
        set(EXECUTED_APP_SET_RUNNER_ARGS__SKS_AIRSPY_COMMUNITY TRUE PARENT_SCOPE)

        # add BMP runner
        # board_runner_args(blackmagicprobe "")
        include(${ZEPHYR_BASE}/boards/common/blackmagicprobe.board.cmake)
    endif()
endmacro()

### Zephyr
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

## configure project
project(sks_airspy_community)

## check if we are building with sysbuild
test_sysbuild(REQUIRED)


### APP
# app sources
target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/common.c)
target_sources(app PRIVATE src/settings.c)
target_sources(app PRIVATE src/bluetooth.c)
target_sources(app PRIVATE src/ant.c)
target_sources(app PRIVATE src/retained.c)
target_sources(app PRIVATE src/spi.c)
target_sources(app PRIVATE src/zbus_com.c)
target_sources(app PRIVATE src/sensor.c)

# ant tpms profile sources
target_sources(app PRIVATE src/ant_profiles/ant_tpms/ant_tpms.c)
target_sources(app PRIVATE src/ant_profiles/ant_tpms/pages/ant_tpms_page_1.c)
target_sources(app PRIVATE src/ant_profiles/ant_tpms/pages/ant_tpms_page_16.c)
target_sources(app PRIVATE src/ant_profiles/ant_common/pages/ant_common_page_82.c)


## include paths
target_include_directories(app PRIVATE include/)
zephyr_library_include_directories(.)
