// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_extension.h"

#include "base/logging.h"

namespace base {
namespace allocator {

bool GetProperty(const char* name, size_t* value) {
  thunks::GetPropertyFunction get_property_function =
      base::allocator::thunks::GetGetPropertyFunction();
  return get_property_function != NULL && get_property_function(name, value);
}

void GetStats(char* buffer, int buffer_length) {
  DCHECK_GT(buffer_length, 0);
  thunks::GetStatsFunction get_stats_function =
      base::allocator::thunks::GetGetStatsFunction();
  if (get_stats_function)
    get_stats_function(buffer, buffer_length);
  else
    buffer[0] = '\0';
}

void ReleaseFreeMemory() {
  thunks::ReleaseFreeMemoryFunction release_free_memory_function =
      base::allocator::thunks::GetReleaseFreeMemoryFunction();
  if (release_free_memory_function)
    release_free_memory_function();
}

void SetGetPropertyFunction(
    thunks::GetPropertyFunction get_property_function) {
  DCHECK_EQ(base::allocator::thunks::GetGetPropertyFunction(),
            reinterpret_cast<thunks::GetPropertyFunction>(NULL));
  base::allocator::thunks::SetGetPropertyFunction(get_property_function);
}

void SetGetStatsFunction(thunks::GetStatsFunction get_stats_function) {
  DCHECK_EQ(base::allocator::thunks::GetGetStatsFunction(),
            reinterpret_cast<thunks::GetStatsFunction>(NULL));
  base::allocator::thunks::SetGetStatsFunction(get_stats_function);
}

void SetReleaseFreeMemoryFunction(
    thunks::ReleaseFreeMemoryFunction release_free_memory_function) {
  DCHECK_EQ(base::allocator::thunks::GetReleaseFreeMemoryFunction(),
            reinterpret_cast<thunks::ReleaseFreeMemoryFunction>(NULL));
  base::allocator::thunks::SetReleaseFreeMemoryFunction(
      release_free_memory_function);
}

}  // namespace allocator
}  // namespace base
