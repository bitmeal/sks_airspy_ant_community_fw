# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

### Zephyr
## early stage configuration
# MCUBoot signing key
set(mcuboot_CONFIG_BOOT_SIGNATURE_KEY_FILE \"${CMAKE_CURRENT_LIST_DIR}/mcuboot.pem\")
# include board definition
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})

## zephyr
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
zephyr_include_directories(include)

## configure project
project(sks_airspy)



## test for ANT SDK compatibility
function(ant_kconfig_validator validator_res ant_kconfig_path_var)
    file(READ ant_kconfig_path_var ANT_KCONFIG_TEXT)

    string(FIND "${ANT_KCONFIG_TEXT}" "SOC_NRF52832" ant_kconfig_text_match_compat_token_res)

    if(${ant_kconfig_text_match_compat_token_res} EQUAL -1)
        # TODO(bitmeal): auto patch
        message(FATAL_ERROR "ANT SDK not compatible with SOC_NRF52832; please patch!")
    else()
        message(STATUS "ANT SDK compatible with SOC_NRF52832!")
    endif ()
endfunction()

function(ant_kconfig_compat_test ant_kconfig_path_var)
    file(READ ant_kconfig_path_var ANT_KCONFIG_TEXT)

    string(FIND "${ANT_KCONFIG_TEXT}" "config ANT" ant_kconfig_text_match_validation_token_res)

    if(${ant_kconfig_text_match_validation_token_res} EQUAL -1)
        set(${validator_res} FALSE PARENT_SCOPE)
        message(AUTHOR_WARNING "is not ant/Kconfig file")
    endif ()
endfunction()

find_path(ANT_SDK_KCONFIG_PATH "ant/Kconfig"
    PATHS
        ${TOOLCHAIN_ROOT}
        ${ZEPHYR_BASE}
        ${NRF_DIR}
    PATH_SUFFIXES "../"
    VALIDATOR ant_kconfig_validator
    REQUIRED
    NO_DEFAULT_PATH
)


### APP
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
