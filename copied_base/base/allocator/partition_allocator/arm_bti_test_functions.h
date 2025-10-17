// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_ARM_BTI_TEST_FUNCTIONS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_ARM_BTI_TEST_FUNCTIONS_H_

#include "build/build_config.h"

#if defined(ARCH_CPU_ARM64)
extern "C" {
/**
 * A valid BTI function. Jumping to this funtion should not cause any problem in
 * a BTI enabled environment.
 **/
int64_t arm_bti_test_function(int64_t);

/**
 * A function without proper BTI landing pad. Jumping here should crash the
 * program on systems which support BTI.
 **/
int64_t arm_bti_test_function_invalid_offset(int64_t);

/**
 * A simple function which immediately returns to sender.
 **/
void arm_bti_test_function_end(void);
}
#endif  // defined(ARCH_CPU_ARM64)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_ARM_BTI_TEST_FUNCTIONS_H_
