/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

.section .note.GNU-stack,"",@progbits

#if defined(__x86_64__)
#include "delock_x86_64.S"
#elif defined(__aarch64__)
#include "delock_aarch64.S"
#else
#error "architecture not supported"
#endif

