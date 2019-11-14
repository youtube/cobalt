// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/util.h"

#include <windows.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(UpdaterTestUtil, HRESULTFromLastError) {
  ::SetLastError(ERROR_ACCESS_DENIED);
  EXPECT_EQ(E_ACCESSDENIED, HRESULTFromLastError());
  ::SetLastError(ERROR_SUCCESS);
  EXPECT_EQ(E_FAIL, HRESULTFromLastError());
}

}  // namespace updater
