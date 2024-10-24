# Copyright (C) Huawei Technologies Co., Ltd. 2022. All rights reserved.
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.10)
project(
    delock
    LANGUAGES C ASM
    VERSION 0.2.0
    DESCRIPTION "Delegation Lock")

include(cmake/CPM.cmake)

CPMAddPackage("gh:open-s4c/libvsync@4.0.1")
CPMAddPackage("gh:open-s4c/tilt@2.1.0")

set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_C_FLAGS "-std=gnu99 -Wall -g -Wextra -DVSYNC_SC")

include_directories(include)
add_subdirectory(src)
