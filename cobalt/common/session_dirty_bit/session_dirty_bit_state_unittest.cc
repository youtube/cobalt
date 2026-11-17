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

#include "cobalt/common/session_dirty_bit/session_dirty_bit_state.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

class SessionDirtyBitStateTest : public testing::Test {
 protected:
  void SetUp() override {
    ResetSessionDirtyBitForTesting();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override { ResetSessionDirtyBitForTesting(); }

  base::ScopedTempDir temp_dir_;
};

TEST_F(SessionDirtyBitStateTest, DefaultsToCleanAndArmsEarlyWhenVisible) {
  InitializeSessionDirtyBit(temp_dir_.GetPath(), /*is_initially_visible=*/true);

  EXPECT_EQ(SessionDirtyBitStage::kClean, GetPreviousSessionDirtyBitStage());
  EXPECT_EQ(SessionDirtyBitStage::kEarly,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));
}

TEST_F(SessionDirtyBitStateTest, PreloadPreservesPriorStageWithoutArmingEarly) {
  WriteSessionDirtyBitFileForTesting(temp_dir_.GetPath(),
                                     SessionDirtyBitStage::kLate);

  InitializeSessionDirtyBit(temp_dir_.GetPath(),
                            /*is_initially_visible=*/false);

  // Prior crash is visible to JS and preserved on disk if the preload is killed
  // before JS consumes it.
  EXPECT_EQ(SessionDirtyBitStage::kLate, GetPreviousSessionDirtyBitStage());
  EXPECT_EQ(SessionDirtyBitStage::kLate,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));

  // Once preload enters foreground focus, it arms kEarly (until JS promotes).
  OnSessionDirtyBitPlatformForeground();
  EXPECT_EQ(SessionDirtyBitStage::kEarly,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));
  EXPECT_EQ(SessionDirtyBitStage::kLate, GetPreviousSessionDirtyBitStage());
}

TEST_F(SessionDirtyBitStateTest, PreloadCleanStartDoesNotDirtyDisk) {
  InitializeSessionDirtyBit(temp_dir_.GetPath(),
                            /*is_initially_visible=*/false);

  EXPECT_EQ(SessionDirtyBitStage::kClean, GetPreviousSessionDirtyBitStage());
  EXPECT_EQ(SessionDirtyBitStage::kClean,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));
}

TEST_F(SessionDirtyBitStateTest, EarlyCrashFollowedByPromotionAndBlurResume) {
  // Simulate previous session dying in kEarly.
  InitializeSessionDirtyBit(temp_dir_.GetPath(), /*is_initially_visible=*/true);
  ASSERT_EQ(SessionDirtyBitStage::kEarly,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));

  // Next cold start snapshots kEarly and re-arms kEarly on disk.
  ResetSessionDirtyBitForTesting();
  InitializeSessionDirtyBit(temp_dir_.GetPath(), /*is_initially_visible=*/true);
  EXPECT_EQ(SessionDirtyBitStage::kEarly, GetPreviousSessionDirtyBitStage());
  EXPECT_EQ(SessionDirtyBitStage::kEarly,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));

  // A transient platform blur/pause BEFORE JS promotes must NOT drop the
  // unconsumed prior-session kEarly crash from memory or disk.
  OnSessionDirtyBitPlatformDisarm(/*reset_promotion=*/false);
  EXPECT_EQ(SessionDirtyBitStage::kEarly, GetPreviousSessionDirtyBitStage());
  EXPECT_EQ(SessionDirtyBitStage::kEarly,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));
  OnSessionDirtyBitPlatformForeground();
  EXPECT_EQ(SessionDirtyBitStage::kEarly, GetPreviousSessionDirtyBitStage());

  // Web bootstrap promotes on-disk dirty bit to kLate while preserving the
  // prior-session kEarly snapshot for current-session readers.
  PromoteSessionDirtyBit();
  EXPECT_EQ(SessionDirtyBitStage::kEarly, GetPreviousSessionDirtyBitStage());
  EXPECT_EQ(SessionDirtyBitStage::kLate,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));

  // Platform blur/pause disarms disk to kClean, clears the prior-session
  // snapshot, and notifies any registered listener callback.
  int callback_count = 0;
  SetPreviousSessionDirtyBitClearedCallback(
      base::BindLambdaForTesting([&]() { ++callback_count; }));
  OnSessionDirtyBitPlatformDisarm(/*reset_promotion=*/false);
  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(SessionDirtyBitStage::kClean, GetPreviousSessionDirtyBitStage());
  EXPECT_EQ(SessionDirtyBitStage::kClean,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));

  // Foreground focus resumes directly to kLate because the document already
  // promoted earlier in this session.
  OnSessionDirtyBitPlatformForeground();
  EXPECT_EQ(SessionDirtyBitStage::kClean, GetPreviousSessionDirtyBitStage());
  EXPECT_EQ(SessionDirtyBitStage::kLate,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));

  // Window teardown (`reset_promotion = true`) followed by warm relaunch in the
  // same PID starts back at kEarly until the new document promotes.
  OnSessionDirtyBitPlatformDisarm(/*reset_promotion=*/true);
  EXPECT_EQ(SessionDirtyBitStage::kClean,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));
  OnSessionDirtyBitPlatformForeground();
  EXPECT_EQ(SessionDirtyBitStage::kEarly,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));
}

TEST_F(SessionDirtyBitStateTest, ExplicitClearAndReloadNavigationFlow) {
  WriteSessionDirtyBitFileForTesting(temp_dir_.GetPath(),
                                     SessionDirtyBitStage::kLate);
  InitializeSessionDirtyBit(temp_dir_.GetPath(), /*is_initially_visible=*/true);
  PromoteSessionDirtyBit();
  ASSERT_EQ(SessionDirtyBitStage::kLate, GetPreviousSessionDirtyBitStage());
  ASSERT_EQ(SessionDirtyBitStage::kLate,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));

  // Intentional reload (`onReloadApp`) clears both disk and prior snapshot.
  ClearSessionDirtyBit();
  EXPECT_EQ(SessionDirtyBitStage::kClean, GetPreviousSessionDirtyBitStage());
  EXPECT_EQ(SessionDirtyBitStage::kClean,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));

  // Committed reload navigation re-arms kEarly while keeping prior snapshot
  // clean.
  OnSessionDirtyBitMainFrameNavigationCommitted();
  EXPECT_EQ(SessionDirtyBitStage::kClean, GetPreviousSessionDirtyBitStage());
  EXPECT_EQ(SessionDirtyBitStage::kEarly,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));
}

TEST_F(SessionDirtyBitStateTest, RendererCommandLineFallbackAndPreInitSafety) {
  base::CommandLine saved_cmd = *base::CommandLine::ForCurrentProcess();
  base::CommandLine::Reset();

  // Querying before CommandLine::Init must return kClean without caching.
  EXPECT_EQ(SessionDirtyBitStage::kClean, GetPreviousSessionDirtyBitStage());

  base::CommandLine::Init(0, nullptr);
  *base::CommandLine::ForCurrentProcess() = saved_cmd;

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kPreviousSessionDirtyBitStageSwitch, "late");
  EXPECT_EQ(SessionDirtyBitStage::kLate, GetPreviousSessionDirtyBitStage());
}

TEST_F(SessionDirtyBitStateTest, CorruptBeaconFileDefaultsToClean) {
  ASSERT_TRUE(base::WriteFile(
      temp_dir_.GetPath().Append(kSessionDirtyBitFilename), "corrupt"));
  InitializeSessionDirtyBit(temp_dir_.GetPath(), /*is_initially_visible=*/true);
  EXPECT_EQ(SessionDirtyBitStage::kClean, GetPreviousSessionDirtyBitStage());
  EXPECT_EQ(SessionDirtyBitStage::kEarly,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));
}

TEST_F(SessionDirtyBitStateTest,
       MendelControlArmConsumesPriorCrashOnMainReadyAndDisarmsCleanly) {
  WriteSessionDirtyBitFileForTesting(temp_dir_.GetPath(),
                                     SessionDirtyBitStage::kEarly);
  InitializeSessionDirtyBit(temp_dir_.GetPath(), /*is_initially_visible=*/true);
  ASSERT_EQ(SessionDirtyBitStage::kEarly, GetPreviousSessionDirtyBitStage());

  // Even when JS does not call promoteSessionDirtyBit() (e.g. Mendel Control
  // arm), hideSplashScreen() / NotifyMainReady() marks the prior crash consumed
  // so subsequent clean exits disarm disk to kClean instead of leaving 'E'
  // permanently sticky.
  MarkPreviousSessionDirtyBitConsumed();
  OnSessionDirtyBitPlatformDisarm(/*reset_promotion=*/true);
  EXPECT_EQ(SessionDirtyBitStage::kClean, GetPreviousSessionDirtyBitStage());
  EXPECT_EQ(SessionDirtyBitStage::kClean,
            ReadSessionDirtyBitFileForTesting(temp_dir_.GetPath()));
}

}  // namespace
}  // namespace cobalt
