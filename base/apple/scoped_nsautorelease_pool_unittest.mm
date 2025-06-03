// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/apple/scoped_nsautorelease_pool.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base::apple {

#if DCHECK_IS_ON()
TEST(ScopedNSAutoreleasePoolTest, DieOutOfOrder) {
  absl::optional<ScopedNSAutoreleasePool> pool1;
  absl::optional<ScopedNSAutoreleasePool> pool2;

  // Instantiate the pools in the order 1, then 2.
  pool1.emplace();
  pool2.emplace();

  // Destroy in the wrong order; ensure death.
  ASSERT_DEATH(pool1.reset(), "autorelease");
}
#endif

}  // namespace base::apple
