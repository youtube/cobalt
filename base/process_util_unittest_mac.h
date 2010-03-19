// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains helpers for the process_util_unittest to allow it to fully
// test the Mac code.

#ifndef BASE_PROCESS_UTIL_UNITTEST_MAC_H_
#define BASE_PROCESS_UTIL_UNITTEST_MAC_H_

#include "base/basictypes.h"

namespace base {

// Allocates memory via system allocators. Alas, they take a _signed_ size for
// allocation.
void* AllocateViaCFAllocatorSystemDefault(int32 size);
void* AllocateViaCFAllocatorMalloc(int32 size);
void* AllocateViaCFAllocatorMallocZone(int32 size);

// Allocates a huge Objective C object.
void* AllocatePsychoticallyBigObjCObject();

} // namespace base

#endif  // BASE_PROCESS_UTIL_UNITTEST_MAC_H_
