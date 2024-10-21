#
# Copyright 2023 by Garmin Ltd. or its subsidiaries.
# All rights reserved.
#
# Use of this Software is limited and subject to the License Agreement for ANT SoftDevice
# and Associated Software. The Agreement accompanies the Software in the root directory of
# the repository.
#

# zephyr_library_include_directories(include)
zephyr_include_directories(include)

zephyr_library()
zephyr_library_sources("lib/ant_tpms.c")

zephyr_library_link_libraries(ant)

add_subdirectory("lib/pages")
add_subdirectory("lib/simulator")
