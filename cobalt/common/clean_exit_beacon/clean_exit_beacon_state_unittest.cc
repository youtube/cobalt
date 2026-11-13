// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/common/clean_exit_beacon/clean_exit_beacon_state.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_command_line.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

class CleanExitBeaconStateTest : public testing::Test {
 protected:
  void SetUp() override { ResetCleanExitBeaconFlaggedForTesting(); }
  void TearDown() override { ResetCleanExitBeaconFlaggedForTesting(); }

  base::test::ScopedCommandLine scoped_command_line_;
};

TEST_F(CleanExitBeaconStateTest, DefaultsToNotFlagged) {
  EXPECT_FALSE(IsCleanExitBeaconFlagged());
}

TEST_F(CleanExitBeaconStateTest, ReadsForwardedCommandLineSwitchWhenUnset) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      kCleanExitBeaconFlaggedSwitch);
  EXPECT_TRUE(IsCleanExitBeaconFlagged());
}

TEST_F(CleanExitBeaconStateTest,
       SetFlaggedUpdatesStateWithoutMutatingCommandLineAndResetsCleanly) {
  SetCleanExitBeaconFlagged(true);
  EXPECT_TRUE(IsCleanExitBeaconFlagged());
  EXPECT_FALSE(scoped_command_line_.GetProcessCommandLine()->HasSwitch(
      kCleanExitBeaconFlaggedSwitch));

  ResetCleanExitBeaconFlaggedForTesting();
  EXPECT_FALSE(IsCleanExitBeaconFlagged());
}

TEST_F(CleanExitBeaconStateTest, SetUnflaggedOverridesPreexistingSwitch) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      kCleanExitBeaconFlaggedSwitch);
  SetCleanExitBeaconFlagged(false);
  EXPECT_FALSE(IsCleanExitBeaconFlagged());
}

TEST_F(CleanExitBeaconStateTest,
       StartupSnapshotPersistsAcrossCleanExitBeaconDisarmWrites) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  TestingPrefServiceSimple prefs;
  metrics::CleanExitBeacon::RegisterPrefs(prefs.registry());

  const base::FilePath beacon_file_path =
      temp_dir.GetPath().Append(metrics::kCleanExitBeaconFilename);
  ASSERT_TRUE(base::WriteFile(
      beacon_file_path,
      metrics::CleanExitBeacon::CreateBeaconFileContentsForTesting(
          /*exited_cleanly=*/false, /*crash_streak=*/1)));

  metrics::CleanExitBeacon beacon(L"", temp_dir.GetPath(), &prefs);
  beacon.Initialize();
  SetCleanExitBeaconFlagged(!beacon.exited_cleanly());
  EXPECT_TRUE(IsCleanExitBeaconFlagged());

  // Arming for the current session (`false`) and later disarming via
  // `SetExperimentState` / clean shutdown (`true`) must not mutate the startup
  // snapshot read by JS.
  beacon.WriteBeaconValue(/*exited_cleanly=*/false);
  beacon.WriteBeaconValue(/*exited_cleanly=*/true);
  EXPECT_FALSE(beacon.exited_cleanly());
  EXPECT_TRUE(IsCleanExitBeaconFlagged());

  metrics::CleanExitBeacon::ResetStabilityExitedCleanlyForTesting(&prefs);
}

}  // namespace
}  // namespace cobalt
