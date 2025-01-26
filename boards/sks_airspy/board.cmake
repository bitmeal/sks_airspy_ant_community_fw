# Copyright (c) 2025 Arne Wendt (@bitmeal)
# SPDX-License-Identifier: MPL-2.0

board_runner_args(jlink "--device=nRF52832_xxAA" "--speed=4000")
board_runner_args(pyocd "--target=nrf52832" "--frequency=4000000")
include(${ZEPHYR_BASE}/boards/common/nrfjprog.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/blackmagicprobe.board.cmake)
