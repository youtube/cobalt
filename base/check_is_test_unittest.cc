// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_is_test.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "base/test/allow_check_is_test_for_testing.h"

// Note: `base::AllowCheckIsTestForTesting` is being called in
// `base/test/launcher/unit_test_launcher.cc` before this test is run.
//
// Thus, `CHECK_IS_TEST()` will succeed.
TEST(CheckIsTest, Usage) {
  base::test::AllowCheckIsTestForTesting();
  CHECK_IS_TEST();
}
