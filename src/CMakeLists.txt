# Copyright (C) Huawei Technologies Co., Ltd. 2022. All rights reserved.
# SPDX-License-Identifier: MIT
set(SOURCES delock.c delock.S wrapper.c)
add_library(delock SHARED ${SOURCES})
target_compile_definitions(delock PUBLIC _GNU_SOURCE)
target_link_libraries(delock vsync tilt)

# test
add_executable(delock_test delock_test.c)
target_link_libraries(delock_test delock pthread)

add_executable(delock_test_ld delock_test.c)
target_link_libraries(delock_test_ld pthread)

# bench
add_executable(delock_bench delock_bench.c)
target_link_libraries(delock_bench delock vsync pthread)

add_executable(delock_bench_ld delock_bench.c)
target_link_libraries(delock_bench_ld vsync pthread)
