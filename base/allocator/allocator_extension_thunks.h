// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_ALLOCATOR_THUNKS_EXTENSION_H
#define BASE_ALLOCATOR_ALLOCATOR_THUNKS_EXTENSION_H
#pragma once

namespace base {
namespace allocator {
namespace thunks {

// WARNING: You probably don't want to use this file unless you are routing a
// new allocator extension from a specific allocator implementation to base.
// See allocator_extension.h to see the interface that base exports.

typedef void GetStatsFunction(char*, int);
void SetGetStatsFunction(GetStatsFunction* get_stats_function);
GetStatsFunction* GetGetStatsFunction();

typedef void ReleaseFreeMemoryFunction();
void SetReleaseFreeMemoryFunction(
    ReleaseFreeMemoryFunction* release_free_memory_function);
ReleaseFreeMemoryFunction* GetReleaseFreeMemoryFunction();

}  // namespace thunks
}  // namespace allocator
}  // namespace base

#endif
