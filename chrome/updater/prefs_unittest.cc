// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/prefs_impl.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/test_scope.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(PrefsTest, PrefsCommitPendingWrites) {
  base::test::TaskEnvironment task_environment;
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata =
      base::MakeRefCounted<PersistedData>(GetTestScope(), pref.get());

  // Writes something to prefs.
  metadata->SetBrandCode("someappid", "brand");
  EXPECT_STREQ(metadata->GetBrandCode("someappid").c_str(), "brand");

  // Tests writing to storage completes.
  PrefsCommitPendingWrites(pref.get());
}

}  // namespace updater
