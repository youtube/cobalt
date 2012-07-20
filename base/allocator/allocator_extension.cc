// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_extension.h"

#include "base/logging.h"

namespace base {
namespace allocator {

void GetStats(char* buffer, int buffer_length) {
  DCHECK_GT(buffer_length, 0);
  if (thunks::GetStatsFunction* get_stats_function =
          base::allocator::thunks::GetGetStatsFunction())
    get_stats_function(buffer, buffer_length);
  else
    buffer[0] = '\0';
}

void ReleaseFreeMemory() {
  if (thunks::ReleaseFreeMemoryFunction* release_free_memory_function =
          base::allocator::thunks::GetReleaseFreeMemoryFunction())
    release_free_memory_function();
}

void SetGetStatsFunction(thunks::GetStatsFunction* get_stats_function) {
  DCHECK_EQ(base::allocator::thunks::GetGetStatsFunction(),
            reinterpret_cast<thunks::GetStatsFunction*>(NULL));
  base::allocator::thunks::SetGetStatsFunction(get_stats_function);
}

void SetReleaseFreeMemoryFunction(
    thunks::ReleaseFreeMemoryFunction* release_free_memory_function) {
  DCHECK_EQ(base::allocator::thunks::GetReleaseFreeMemoryFunction(),
            reinterpret_cast<thunks::ReleaseFreeMemoryFunction*>(NULL));
  base::allocator::thunks::SetReleaseFreeMemoryFunction(
      release_free_memory_function);
}

}  // namespace allocator
}  // namespace base
