# Makefile - MMA7660 3-axis accelerometer
#
# Copyright (c) Paul Timke <ptimkec@live.com>
#
# SPDX-License-Identifier: Apache-2.0
#
zephyr_library()

zephyr_library_sources(mma7660.c)
zephyr_library_sources_ifdef(CONFIG_MMA7660_TRIGGER mma7660_trigger.c)

