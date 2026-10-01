# Copyright (c) 2026 Arne Wendt (@bitmeal)
# SPDX-License-Identifier: MPL-2.0

# include board definitions
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/../)

### Sysbuild
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

### runners
# we cannot modify runners when using existing boards (for debugging); patch without using zephyr provisions
#   - patch all runners.yaml to include blackmagicprobe with "--rtt" argument
#   - patch all runners.yaml to to use blackmagicprobe as default runner
function(patch_runners)
    message(STATUS "patching runners...")

    set(APP_RUNNERS_YAML "${CMAKE_BINARY_DIR}/${DEFAULT_IMAGE}/zephyr/runners.yaml")
    set(MCUBOOT_RUNNERS_YAML "${CMAKE_BINARY_DIR}/mcuboot/zephyr/runners.yaml")
    set(PATCH_SCRIPT )

    message(STATUS "patching: ${APP_RUNNERS_YAML}")
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_LIST_DIR}/patch_runners.py" 
                ${APP_RUNNERS_YAML} "blackmagicprobe" "--rtt"
        RESULT_VARIABLE patch_result
    )
    if(NOT patch_result EQUAL 0)
        message(FATAL_ERROR "Failed patching ${APP_RUNNERS_YAML}")
    endif()

    message(STATUS "patching: ${MCUBOOT_RUNNERS_YAML}")
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_LIST_DIR}/patch_runners.py" 
                ${MCUBOOT_RUNNERS_YAML} "blackmagicprobe" "--rtt"
        RESULT_VARIABLE patch_result
    )
    if(NOT patch_result EQUAL 0)
        message(FATAL_ERROR "Failed patching ${MCUBOOT_RUNNERS_YAML}")
    endif()
endfunction()
cmake_language(DEFER CALL patch_runners)
