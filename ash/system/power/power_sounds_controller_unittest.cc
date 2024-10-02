// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_sounds_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/system_notification_controller.h"
#include "ash/system/test_system_sounds_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"

using power_manager::PowerSupplyProperties;

namespace ash {

class PowerSoundsControllerTest : public AshTestBase {
 public:
  PowerSoundsControllerTest() = default;

  PowerSoundsControllerTest(const PowerSoundsControllerTest&) = delete;
  PowerSoundsControllerTest& operator=(const PowerSoundsControllerTest&) =
      delete;

  ~PowerSoundsControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_.InitAndEnableFeature(features::kSystemSounds);
    AshTestBase::SetUp();
    SetInitialPowerStatus();
  }

  TestSystemSoundsDelegate* GetSystemSoundsDelegate() const {
    return static_cast<TestSystemSoundsDelegate*>(
        Shell::Get()->system_sounds_delegate());
  }

  bool VerifySounds(const std::vector<Sound>& expected_sounds) const {
    const auto& actual_sounds =
        GetSystemSoundsDelegate()->last_played_sound_keys();

    if (actual_sounds.size() != expected_sounds.size())
      return false;

    for (size_t i = 0; i < expected_sounds.size(); ++i) {
      if (expected_sounds[i] != actual_sounds[i])
        return false;
    }

    return true;
  }

  // Returns true if the lid is closed.
  bool SetLidState(bool closed) {
    chromeos::FakePowerManagerClient::Get()->SetLidState(
        closed ? chromeos::PowerManagerClient::LidState::CLOSED
               : chromeos::PowerManagerClient::LidState::OPEN,
        base::TimeTicks::Now());
    return Shell::Get()
               ->system_notification_controller()
               ->power_sounds_->lid_state_ ==
           chromeos::PowerManagerClient::LidState::CLOSED;
  }

  void SetPowerStatus(int battery_level, bool line_power_connected) {
    ASSERT_GE(battery_level, 0);
    ASSERT_LE(battery_level, 100);

    const bool old_line_power_connected = is_line_power_connected_;
    is_line_power_connected_ = line_power_connected;

    PowerSupplyProperties proto;
    proto.set_external_power(
        line_power_connected
            ? power_manager::PowerSupplyProperties_ExternalPower_AC
            : power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
    proto.set_battery_percent(battery_level);
    proto.set_battery_time_to_empty_sec(3 * 60 * 60);
    proto.set_battery_time_to_full_sec(2 * 60 * 60);
    proto.set_is_calculating_battery_time(false);

    chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(proto);

    // Records the battery level only when it's a plugin or unplug event.
    if (old_line_power_connected != is_line_power_connected_) {
      if (is_line_power_connected_)
        plugged_in_levels_samples_[battery_level]++;
      else
        unplugged_levels_samples_[battery_level]++;
    }
  }

  void SetInitialPowerStatus() {
    // The default status for power is connected with a charger and the battery
    // level is 1%. We set the initial power status for each unit test to
    // disconnected with a charger and 5% battery level.
    is_line_power_connected_ = true;
    SetPowerStatus(5, /*line_power_connected=*/false);
    EXPECT_FALSE(SetLidState(/*closed=*/false));
  }

 protected:
  base::HistogramTester histogram_tester_;

  base::flat_map</*battery_level=*/int, /*sample_count=*/int>
      plugged_in_levels_samples_;
  base::flat_map</*battery_level=*/int, /*sample_count=*/int>
      unplugged_levels_samples_;

 private:
  base::test::ScopedFeatureList scoped_feature_;
  bool is_line_power_connected_;
};

// Tests if sounds are played correctedly when the device is plugged at three
// different battery ranges.
TEST_F(PowerSoundsControllerTest, PlaySoundsForCharging) {
  // Expect no sounds at the initial status when a device has a battery level of
  // 5%.
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // When the battery level is in low range, from 0% to 15%, the sound for
  // plugging in should be `Sound::kChargeLowBattery`.
  SetPowerStatus(5, /*line_power_connected=*/true);
  EXPECT_TRUE(VerifySounds({Sound::kChargeLowBattery}));

  // We should reset the sound key if the sound played successfully each time
  // for not affecting the next sound key.
  GetSystemSoundsDelegate()->reset();

  // Unplug the line power when battery level reaches out to 50%.
  SetPowerStatus(50, /*line_power_connected=*/false);

  // When the battery level is in medium range, from 16% to 79%, the sound for
  // plugging in should be `Sound::kChargeMediumBattery`.
  SetPowerStatus(50, /*line_power_connected=*/true);
  EXPECT_TRUE(VerifySounds({Sound::kChargeMediumBattery}));
  GetSystemSoundsDelegate()->reset();

  // Unplug the line power when battery level reaches out to 90%.
  SetPowerStatus(90, /*line_power_connected=*/false);

  // When the battery level is in high range, from 80% to 100%, the sound for
  // plugging in should be `Sound::kChargeHighBattery`.
  SetPowerStatus(90, /*line_power_connected=*/true);
  EXPECT_TRUE(VerifySounds({Sound::kChargeHighBattery}));
}

// Tests if the warning sound can be played when the battery level drops below
// 15% at the first time.
TEST_F(PowerSoundsControllerTest, PlaySoundForLowBatteryWarning) {
  // Don't play warning sound if the battery level is no less than 15%.
  SetPowerStatus(16, /*line_power_connected=*/false);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // When the battery drops below 15% at the first time, e.g. 15%, the device
  // should play the sound for warning.
  SetPowerStatus(15, /*line_power_connected=*/false);
  EXPECT_TRUE(VerifySounds({Sound::kNoChargeLowBattery}));
  GetSystemSoundsDelegate()->reset();

  // When the battery level keeps dropping, the device shouldn't play sound for
  // warning.
  SetPowerStatus(14, /*line_power_connected=*/false);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());
}

// Tests a very edge case we charge the device at the same time the warning
// sound is triggered.
TEST_F(PowerSoundsControllerTest, PlayTwoSoundsSimultaneously) {
  // Don't play warning sound if the battery level is no less than 15%.
  SetPowerStatus(16, /*line_power_connected=*/false);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // Charge the device at the moment when the low battery warning sound is
  // played. The device should play two sounds.
  SetPowerStatus(15, /*line_power_connected=*/true);
  EXPECT_TRUE(
      VerifySounds({Sound::kChargeLowBattery, Sound::kNoChargeLowBattery}));
}

// Tests that the recording when the device is plugged in or Unplugged are
// recorded correctly.
TEST_F(PowerSoundsControllerTest,
       RecordingBatteryLevelWhenPluggedInOrUnplugged) {
  SetPowerStatus(5, /*line_power_connected=*/true);

  SetPowerStatus(50, /*line_power_connected=*/false);
  SetPowerStatus(50, /*line_power_connected=*/true);

  SetPowerStatus(100, /*line_power_connected=*/false);
  SetPowerStatus(100, /*line_power_connected=*/true);

  SetPowerStatus(100, /*line_power_connected=*/false);
  SetPowerStatus(100, /*line_power_connected=*/true);

  SetPowerStatus(100, /*line_power_connected=*/false);

  // Compare the number of samples for battery level from 0% to 100%.
  for (int i = 0; i <= 100; i++) {
    histogram_tester_.ExpectBucketCount(
        PowerSoundsController::kPluggedInBatteryLevelHistogramName, i,
        plugged_in_levels_samples_[i]);

    histogram_tester_.ExpectBucketCount(
        PowerSoundsController::kUnpluggedBatteryLevelHistogramName, i,
        unplugged_levels_samples_[i]);
  }
}

// Tests that the sounds can only be played if the lid is opened; otherwise, we
// don't play any sounds.
TEST_F(PowerSoundsControllerTest, PlaySoundsOnlyIfLidIsOpened) {
  // When the lid is closed, plugging in a line power, the device don't play any
  // sound.
  EXPECT_TRUE(SetLidState(/*closed=*/true));
  SetPowerStatus(5, /*line_power_connected=*/true);
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // When we open the lid, it doesn't play the delayed sound.
  EXPECT_FALSE(SetLidState(/*closed=*/false));
  EXPECT_TRUE(GetSystemSoundsDelegate()->empty());

  // Under the condition of the lid opening, the device will play a sound when
  // charging it.
  SetPowerStatus(10, /*line_power_connected=*/false);
  SetPowerStatus(5, /*line_power_connected=*/true);
  EXPECT_TRUE(VerifySounds({Sound::kChargeLowBattery}));
}

}  // namespace ash