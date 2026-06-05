# Copyright (c) 2026 Arne Wendt (@bitmeal)
# SPDX-License-Identifier: MPL-2.0

find_package(Sysbuild REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(sysbuild LANGUAGES)

### artifacts
# create merged.hex with sysbuild and without partition manager

if(
    SB_CONFIG_BOOTLOADER_MCUBOOT AND
    NOT SB_CONFIG_PARTITION_MANAGER AND
    # testing NOT SB_CONFIG_MERGED_HEX_FILES to guard against new zephyr versions that automate the process
    NOT SB_CONFIG_MERGED_HEX_FILES
    )
    # # TODO: handle unsigned images
    # sysbuild_get(${DEFAULT_IMAGE}_MCUBOOT_GENERATE_UNSIGNED_IMAGE IMAGE ${DEFAULT_IMAGE} VAR CONFIG_MCUBOOT_GENERATE_UNSIGNED_IMAGE KCONFIG)

    add_custom_command(
    OUTPUT
        ${APPLICATION_BINARY_DIR}/merged.hex
    COMMAND
        ${PYTHON_EXECUTABLE}
        ${ZEPHYR_BASE}/scripts/build/mergehex.py
        -o ${APPLICATION_BINARY_DIR}/merged.hex
        ${CMAKE_BINARY_DIR}/${DEFAULT_IMAGE}/zephyr/zephyr.signed.hex
        ${CMAKE_BINARY_DIR}/mcuboot/zephyr/zephyr.hex
    DEPENDS
        ${DEFAULT_IMAGE}
        ${CMAKE_BINARY_DIR}/${DEFAULT_IMAGE}/zephyr/zephyr.signed.hex
        mcuboot
        ${CMAKE_BINARY_DIR}/mcuboot/zephyr/zephyr.hex
    WORKING_DIRECTORY
        ${APPLICATION_BINARY_DIR}
    COMMENT "Post-build: merge application and mcuboot image in merged.hex"
    #   VERBATIM
    )

    add_custom_target(
    merged_hex
    ALL
    DEPENDS
        ${APPLICATION_BINARY_DIR}/merged.hex
    )
endif()