// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/cloned_install_detector.h"

#include "components/metrics/machine_id_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

const std::string kTestRawId = "test";
// Hashed machine id for |kTestRawId|.
const int kTestHashedId = 2216819;

}  // namespace

// TODO(jwd): Change these test to test the full flow and histogram outputs. It
// should also remove the need to make the test a friend of
// ClonedInstallDetector.
TEST(ClonedInstallDetectorTest, SaveId) {
  TestingPrefServiceSimple prefs;
  ClonedInstallDetector::RegisterPrefs(prefs.registry());

  ClonedInstallDetector detector;
  detector.SaveMachineId(&prefs, kTestRawId);

  EXPECT_EQ(kTestHashedId, prefs.GetInteger(prefs::kMetricsMachineId));
}

TEST(ClonedInstallDetectorTest, DetectClone) {
  TestingPrefServiceSimple prefs;
  MetricsStateManager::RegisterPrefs(prefs.registry());

  // Save a machine id that will cause a clone to be detected.
  prefs.SetInteger(prefs::kMetricsMachineId, kTestHashedId + 1);

  ClonedInstallDetector detector;
  detector.SaveMachineId(&prefs, kTestRawId);

  EXPECT_TRUE(prefs.GetBoolean(prefs::kMetricsResetIds));
}

}  // namespace metrics
