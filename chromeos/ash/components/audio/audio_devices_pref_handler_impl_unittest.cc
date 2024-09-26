// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_devices_pref_handler_impl.h"

#include <stdint.h>

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "base/memory/ref_counted.h"
#include "base/time/time_override.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using testing::Values;

const uint64_t kPresetInputId = 10001;
const uint64_t kHeadphoneId = 10002;
const uint64_t kInternalMicId = 10003;
const uint64_t kPresetOutputId = 10004;
const uint64_t kUSBMicId = 10005;
const uint64_t kHDMIOutputId = 10006;
const uint64_t kOtherTypeOutputId = 90001;
const uint64_t kOtherTypeInputId = 90002;

const char kPresetInputDeprecatedPrefKey[] = "10001 : 1";
const char kPresetOutputDeprecatedPrefKey[] = "10004 : 0";

const struct {
  bool active;
  bool activate_by_user;
  double sound_level;
  bool mute;
} kPresetState = {true, true, 25.2, true};

struct AudioNodeInfo {
  bool is_input;
  uint64_t id;
  const char* const device_name;
  const char* const type;
  const char* const name;
};

const AudioNodeInfo kPresetInput = {true, kPresetInputId, "Fake input",
                                    "INTERNAL_MIC", "Preset fake input"};

const AudioNodeInfo kInternalMic = {true, kInternalMicId, "Fake Mic",
                                    "INTERNAL_MIC", "Internal Mic"};

const AudioNodeInfo kUSBMic = {true, kUSBMicId, "Fake USB Mic", "USB",
                               "USB Microphone"};

const AudioNodeInfo kPresetOutput = {false, kPresetOutputId, "Fake output",
                                     "HEADPHONE", "Preset fake output"};

const AudioNodeInfo kHeadphone = {false, kHeadphoneId, "Fake Headphone",
                                  "HEADPHONE", "Headphone"};

const AudioNodeInfo kHDMIOutput = {false, kHDMIOutputId, "HDMI output", "HDMI",
                                   "HDMI output"};

const AudioNodeInfo kInputDeviceWithSpecialCharacters = {
    true, kOtherTypeInputId, "Fake ~!@#$%^&*()_+`-=<>?,./{}|[]\\\\Mic",
    "SOME_OTHER_TYPE", "Other Type Input Device"};

const AudioNodeInfo kOutputDeviceWithSpecialCharacters = {
    false, kOtherTypeOutputId, "Fake ~!@#$%^&*()_+`-=<>?,./{}|[]\\\\Headphone",
    "SOME_OTHER_TYPE", "Other Type Output Device"};

const uint32_t kInputMaxSupportedChannels = 1;
const uint32_t kOutputMaxSupportedChannels = 2;

const uint32_t kInputAudioEffect = 1;
const uint32_t kOutputAudioEffect = 0;
// Does not support getting input step now.
const int32_t kInputNumberOfVolumeSteps = 0;
const int32_t kOutputNumberOfVolumeSteps = 25;

AudioDevice CreateAudioDevice(const AudioNodeInfo& info, int version) {
  return AudioDevice(AudioNode(
      info.is_input, info.id, version == 2, info.id /* stable_device_id_v1 */,
      version == 1 ? 0 : info.id ^ 0xFF /* stable_device_id_v2 */,
      info.device_name, info.type, info.name, false, 0,
      info.is_input ? kInputMaxSupportedChannels : kOutputMaxSupportedChannels,
      info.is_input ? kInputAudioEffect : kOutputAudioEffect,
      info.is_input ? kInputNumberOfVolumeSteps : kOutputNumberOfVolumeSteps));
}

// Test param determines whether the test should test input or output devices
// true -> input devices
// false -> output_devices
class AudioDevicesPrefHandlerTest : public testing::TestWithParam<bool> {
 public:
  AudioDevicesPrefHandlerTest() = default;

  AudioDevicesPrefHandlerTest(const AudioDevicesPrefHandlerTest&) = delete;
  AudioDevicesPrefHandlerTest& operator=(const AudioDevicesPrefHandlerTest&) =
      delete;

  ~AudioDevicesPrefHandlerTest() override = default;

  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    AudioDevicesPrefHandlerImpl::RegisterPrefs(pref_service_->registry());

    // Set the preset pref values directly, to ensure it doesn't depend on pref
    // handler implementation.
    // This has to be done before audio_pref_hander_ is created, so the values
    // are set when pref value sets up its internal state.
    std::string preset_key = GetPresetDeviceDeprecatedPrefKey();
    {
      ScopedDictPrefUpdate update(pref_service_.get(),
                                  prefs::kAudioDevicesState);
      base::Value::Dict& pref = update.Get();
      base::Value::Dict state;
      state.Set("active", kPresetState.active);
      state.Set("activate_by_user", kPresetState.activate_by_user);
      pref.Set(preset_key, std::move(state));
    }

    {
      ScopedDictPrefUpdate update(pref_service_.get(),
                                  prefs::kAudioDevicesVolumePercent);
      base::Value::Dict& pref = update.Get();
      pref.Set(preset_key, kPresetState.sound_level);
    }

    {
      ScopedDictPrefUpdate update(pref_service_.get(),
                                  prefs::kAudioDevicesMute);
      base::Value::Dict& pref = update.Get();
      pref.Set(preset_key, static_cast<int>(kPresetState.mute));
    }

    audio_pref_handler_ = new AudioDevicesPrefHandlerImpl(pref_service_.get());
  }

  void TearDown() override { audio_pref_handler_.reset(); }

 protected:
  void ReloadPrefHandler() {
    audio_pref_handler_ = new AudioDevicesPrefHandlerImpl(pref_service_.get());
  }

  AudioDevice GetDeviceWithVersion(int version) {
    return CreateAudioDevice(IsInputTest() ? kInternalMic : kHeadphone,
                             version);
  }

  std::string GetPresetDeviceDeprecatedPrefKey() {
    return IsInputTest() ? kPresetInputDeprecatedPrefKey
                         : kPresetOutputDeprecatedPrefKey;
  }

  AudioDevice GetPresetDeviceWithVersion(int version) {
    return CreateAudioDevice(IsInputTest() ? kPresetInput : kPresetOutput,
                             version);
  }

  AudioDevice GetSecondaryDeviceWithVersion(int version) {
    return CreateAudioDevice(IsInputTest() ? kUSBMic : kHDMIOutput, version);
  }

  AudioDevice GetDeviceWithSpecialCharactersWithVersion(int version) {
    return CreateAudioDevice(IsInputTest() ? kInputDeviceWithSpecialCharacters
                                           : kOutputDeviceWithSpecialCharacters,
                             version);
  }

  double GetSoundLevelValue(const AudioDevice& device) {
    return IsInputTest() ? audio_pref_handler_->GetInputGainValue(&device)
                         : audio_pref_handler_->GetOutputVolumeValue(&device);
  }

  int GetUserPriority(const AudioDevice& device) {
    return audio_pref_handler_->GetUserPriority(device);
  }

  void SetSoundLevelValue(const AudioDevice& device, double value) {
    return audio_pref_handler_->SetVolumeGainValue(device, value);
  }

  void SetDeviceState(const AudioDevice& device,
                      bool active,
                      bool activated_by_user) {
    audio_pref_handler_->SetDeviceActive(device, active, activated_by_user);
  }

  bool DeviceStateExists(const AudioDevice& device) {
    bool unused;
    return audio_pref_handler_->GetDeviceActive(device, &unused, &unused);
  }

  void ExpectDeviceStateEquals(const AudioDevice& device,
                               bool expect_active,
                               bool expect_activated_by_user) {
    bool active = false;
    bool activated_by_user = false;
    ASSERT_TRUE(audio_pref_handler_->GetDeviceActive(device, &active,
                                                     &activated_by_user))
        << " value for device " << device.id << " not found.";
    EXPECT_EQ(expect_active, active) << " device " << device.id;
    EXPECT_EQ(expect_activated_by_user, activated_by_user)
        << " device " << device.id;
  }

  bool GetMute(const AudioDevice& device) {
    return audio_pref_handler_->GetMuteValue(device);
  }

  void SetMute(const AudioDevice& device, bool value) {
    audio_pref_handler_->SetMuteValue(device, value);
  }

  double GetDefaultSoundLevelValue() {
    return IsInputTest() ? AudioDevicesPrefHandler::kDefaultInputGainPercent
                         : AudioDevicesPrefHandler::kDefaultOutputVolumePercent;
  }

  bool IsInputTest() const { return GetParam(); }

  scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

INSTANTIATE_TEST_SUITE_P(Input, AudioDevicesPrefHandlerTest, Values(true));
INSTANTIATE_TEST_SUITE_P(Output, AudioDevicesPrefHandlerTest, Values(false));

TEST_P(AudioDevicesPrefHandlerTest, TestDefaultValuesV1) {
  AudioDevice device = GetDeviceWithVersion(1);
  AudioDevice secondary_device = GetSecondaryDeviceWithVersion(1);

  // TODO(rkc): Once the bug with default preferences is fixed, fix this test
  // also. http://crbug.com/442489
  EXPECT_EQ(GetDefaultSoundLevelValue(), GetSoundLevelValue(device));
  EXPECT_EQ(GetDefaultSoundLevelValue(), GetSoundLevelValue(secondary_device));

  EXPECT_FALSE(DeviceStateExists(device));
  EXPECT_FALSE(DeviceStateExists(secondary_device));

  EXPECT_FALSE(GetMute(device));
  EXPECT_FALSE(GetMute(secondary_device));

  EXPECT_EQ(0, GetUserPriority(device));
  EXPECT_EQ(0, GetUserPriority(secondary_device));
}

TEST_P(AudioDevicesPrefHandlerTest, TestDefaultValuesV2) {
  AudioDevice device = GetDeviceWithVersion(2);
  AudioDevice secondary_device = GetSecondaryDeviceWithVersion(2);

  // TODO(rkc): Once the bug with default preferences is fixed, fix this test
  // also. http://crbug.com/442489
  EXPECT_EQ(GetDefaultSoundLevelValue(), GetSoundLevelValue(device));
  EXPECT_EQ(GetDefaultSoundLevelValue(), GetSoundLevelValue(secondary_device));

  EXPECT_FALSE(DeviceStateExists(device));
  EXPECT_FALSE(DeviceStateExists(secondary_device));

  EXPECT_FALSE(GetMute(device));
  EXPECT_FALSE(GetMute(secondary_device));

  EXPECT_EQ(0, GetUserPriority(device));
  EXPECT_EQ(0, GetUserPriority(secondary_device));
}

TEST_P(AudioDevicesPrefHandlerTest, PrefsRegistered) {
  // The standard audio prefs are registered.
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioDevicesVolumePercent));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioDevicesMute));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioOutputAllowed));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioVolumePercent));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioMute));
  EXPECT_TRUE(pref_service_->FindPreference(prefs::kAudioDevicesState));
  EXPECT_TRUE(
      pref_service_->FindPreference(prefs::kAudioInputDevicesUserPriority));
  EXPECT_TRUE(
      pref_service_->FindPreference(prefs::kAudioOutputDevicesUserPriority));
}

TEST_P(AudioDevicesPrefHandlerTest, SoundLevel) {
  AudioDevice device = GetDeviceWithVersion(2);
  SetSoundLevelValue(device, 13.37);
  EXPECT_EQ(13.37, GetSoundLevelValue(device));
}

TEST_P(AudioDevicesPrefHandlerTest, SoundLevelMigratedFromV1StableId) {
  if (IsInputTest()) {
    // Input gain does not need to be migrated since a new pref is used.
    return;
  }

  AudioDevice device_v1 = GetPresetDeviceWithVersion(1);
  AudioDevice device_v2 = GetPresetDeviceWithVersion(2);

  // Sanity check for test params - preset state should be different than the
  // default one in order for this test to make sense.
  ASSERT_NE(GetDefaultSoundLevelValue(), kPresetState.sound_level);

  EXPECT_EQ(kPresetState.sound_level, GetSoundLevelValue(device_v1));
  EXPECT_EQ(kPresetState.sound_level, GetSoundLevelValue(device_v2));
  // Test that v1 entry does not exist after migration - the method should
  // return default value.
  EXPECT_EQ(GetDefaultSoundLevelValue(), GetSoundLevelValue(device_v1));

  // Test that values are persisted when audio pref handler is reset.
  ReloadPrefHandler();
  EXPECT_EQ(GetDefaultSoundLevelValue(), GetSoundLevelValue(device_v1));
  EXPECT_EQ(kPresetState.sound_level, GetSoundLevelValue(device_v2));
}

TEST_P(AudioDevicesPrefHandlerTest, SettingV2DeviceSoundLevelRemovesV1Entry) {
  if (IsInputTest()) {
    // Input gain does not need to be migrated since a new pref is used.
    return;
  }
  AudioDevice device_v1 = GetDeviceWithVersion(1);
  AudioDevice device_v2 = GetDeviceWithVersion(2);

  SetSoundLevelValue(device_v1, 13.37);
  EXPECT_EQ(13.37, GetSoundLevelValue(device_v1));

  SetSoundLevelValue(device_v2, 13.38);
  EXPECT_EQ(GetDefaultSoundLevelValue(), GetSoundLevelValue(device_v1));
  EXPECT_EQ(13.38, GetSoundLevelValue(device_v2));

  // Test that values are persisted when audio pref handler is reset.
  ReloadPrefHandler();
  EXPECT_EQ(GetDefaultSoundLevelValue(), GetSoundLevelValue(device_v1));
  EXPECT_EQ(13.38, GetSoundLevelValue(device_v2));
}

TEST_P(AudioDevicesPrefHandlerTest, MigrateFromGlobalSoundLevelPref) {
  if (IsInputTest()) {
    // For any new input devices, the default volume is set from a constant
    // rather than the default kAudioVolumePercent.
    return;
  }
  pref_service_->SetDouble(prefs::kAudioVolumePercent, 13.37);

  // For devices with v1 stable device id.
  EXPECT_EQ(13.37, GetSoundLevelValue(GetDeviceWithVersion(1)));
  EXPECT_EQ(13.37, GetSoundLevelValue(GetDeviceWithVersion(2)));

  // For devices with v2 stable id.
  EXPECT_EQ(13.37, GetSoundLevelValue(GetSecondaryDeviceWithVersion(2)));
}

TEST_P(AudioDevicesPrefHandlerTest, Mute) {
  AudioDevice device = GetDeviceWithVersion(2);
  SetMute(device, true);
  EXPECT_TRUE(GetMute(device));

  SetMute(device, false);
  EXPECT_FALSE(GetMute(device));
}

TEST_P(AudioDevicesPrefHandlerTest, MuteMigratedFromV1StableId) {
  AudioDevice device_v1 = GetPresetDeviceWithVersion(1);
  AudioDevice device_v2 = GetPresetDeviceWithVersion(2);

  // Sanity check for test params - preset state should be different than the
  // default one (mute = false) in order for this test to make sense.
  ASSERT_TRUE(kPresetState.mute);

  EXPECT_EQ(kPresetState.mute, GetMute(device_v1));
  EXPECT_EQ(kPresetState.mute, GetMute(device_v2));
  // Test that v1 entry does not exist after migration - the method should
  // return default value
  EXPECT_FALSE(GetMute(device_v1));

  // Test that values are persisted when audio pref handler is reset.
  ReloadPrefHandler();
  EXPECT_FALSE(GetMute(device_v1));
  EXPECT_EQ(kPresetState.mute, GetMute(device_v2));
}

TEST_P(AudioDevicesPrefHandlerTest, SettingV2DeviceMuteRemovesV1Entry) {
  AudioDevice device_v1 = GetDeviceWithVersion(1);
  AudioDevice device_v2 = GetDeviceWithVersion(2);

  SetMute(device_v1, true);
  EXPECT_TRUE(GetMute(device_v1));

  SetMute(device_v2, true);
  EXPECT_FALSE(GetMute(device_v1));
  EXPECT_TRUE(GetMute(device_v2));

  // Test that values are persisted when audio pref handler is reset.
  ReloadPrefHandler();
  EXPECT_FALSE(GetMute(device_v1));
  EXPECT_TRUE(GetMute(device_v2));
}

TEST_P(AudioDevicesPrefHandlerTest, MigrateFromGlobalMutePref) {
  pref_service_->SetInteger(prefs::kAudioMute, true);

  // For devices with v1 stable device id.
  EXPECT_TRUE(GetMute(GetDeviceWithVersion(1)));
  EXPECT_TRUE(GetMute(GetDeviceWithVersion(2)));

  // For devices with v2 stable id.
  EXPECT_TRUE(GetMute(GetSecondaryDeviceWithVersion(2)));
}

TEST_P(AudioDevicesPrefHandlerTest, TestSpecialCharactersInDeviceNames) {
  AudioDevice device = GetDeviceWithSpecialCharactersWithVersion(2);
  SetSoundLevelValue(device, 73.31);
  EXPECT_EQ(73.31, GetSoundLevelValue(device));

  SetMute(device, true);
  EXPECT_TRUE(GetMute(device));

  SetDeviceState(device, true, true);
  ExpectDeviceStateEquals(device, true, true);
}

TEST_P(AudioDevicesPrefHandlerTest, TestDeviceStates) {
  AudioDevice device = GetDeviceWithVersion(2);
  SetDeviceState(device, true, true);
  ExpectDeviceStateEquals(device, true, true);

  SetDeviceState(device, true, false);
  ExpectDeviceStateEquals(device, true, false);

  SetDeviceState(device, false, false);
  ExpectDeviceStateEquals(device, false, false);

  AudioDevice secondary_device = GetSecondaryDeviceWithVersion(2);
  EXPECT_FALSE(DeviceStateExists(secondary_device));
}

TEST_P(AudioDevicesPrefHandlerTest, TestDeviceStatesMigrateFromV1StableId) {
  AudioDevice device_v1 = GetPresetDeviceWithVersion(1);
  AudioDevice device_v2 = GetPresetDeviceWithVersion(2);

  ExpectDeviceStateEquals(device_v1, kPresetState.active,
                          kPresetState.activate_by_user);
  ExpectDeviceStateEquals(device_v2, kPresetState.active,
                          kPresetState.activate_by_user);
  EXPECT_FALSE(DeviceStateExists(device_v1));

  // Test that values are persisted when audio pref handler is reset.
  ReloadPrefHandler();
  EXPECT_FALSE(DeviceStateExists(device_v1));
  ExpectDeviceStateEquals(device_v2, kPresetState.active,
                          kPresetState.activate_by_user);
}

TEST_P(AudioDevicesPrefHandlerTest, TestSettingV2DeviceStateRemovesV1Entry) {
  AudioDevice device_v1 = GetDeviceWithVersion(1);
  AudioDevice device_v2 = GetDeviceWithVersion(2);

  SetDeviceState(device_v1, true, true);
  ExpectDeviceStateEquals(device_v1, true, true);

  SetDeviceState(device_v2, false, false);
  EXPECT_FALSE(DeviceStateExists(device_v1));
  ExpectDeviceStateEquals(device_v2, false, false);

  // Test that values are persisted when audio pref handler is reset.
  ReloadPrefHandler();
  EXPECT_FALSE(DeviceStateExists(device_v1));
  ExpectDeviceStateEquals(device_v2, false, false);
}

TEST_P(AudioDevicesPrefHandlerTest, InputNoiseCancellationPrefRegistered) {
  EXPECT_FALSE(audio_pref_handler_->GetNoiseCancellationState());
  audio_pref_handler_->SetNoiseCancellationState(true);
  EXPECT_TRUE(audio_pref_handler_->GetNoiseCancellationState());
}

TEST_P(AudioDevicesPrefHandlerTest, UserPriority) {
  AudioDevice device = GetDeviceWithVersion(2);
  EXPECT_EQ(kUserPriorityNone, GetUserPriority(device));

  AudioDevice device2 = GetSecondaryDeviceWithVersion(2);
  audio_pref_handler_->SetUserPriorityHigherThan(device2, &device);
  EXPECT_EQ(kUserPriorityNone, GetUserPriority(device));
  EXPECT_EQ(kUserPriorityMin, GetUserPriority(device2));

  audio_pref_handler_->SetUserPriorityHigherThan(device, &device2);
  EXPECT_EQ(2, GetUserPriority(device));
  EXPECT_EQ(kUserPriorityMin, GetUserPriority(device2));

  AudioDevice device3 = GetDeviceWithSpecialCharactersWithVersion(2);

  audio_pref_handler_->SetUserPriorityHigherThan(device3, &device2);
  EXPECT_EQ(2, GetUserPriority(device3));
  EXPECT_EQ(3, GetUserPriority(device));
  EXPECT_EQ(kUserPriorityMin, GetUserPriority(device2));

  audio_pref_handler_->SetUserPriorityHigherThan(device, &device3);
  EXPECT_EQ(2, GetUserPriority(device3));
  EXPECT_EQ(3, GetUserPriority(device));
  EXPECT_EQ(kUserPriorityMin, GetUserPriority(device2));

  audio_pref_handler_->SetUserPriorityHigherThan(device3, &device);
  EXPECT_EQ(3, GetUserPriority(device3));
  EXPECT_EQ(2, GetUserPriority(device));
  EXPECT_EQ(kUserPriorityMin, GetUserPriority(device2));
}

TEST_P(AudioDevicesPrefHandlerTest, UserPrioritySingle) {
  AudioDevice device = GetDeviceWithVersion(2);
  AudioDevice device2 = GetSecondaryDeviceWithVersion(2);
  audio_pref_handler_->SetUserPriorityHigherThan(device, nullptr);
  EXPECT_EQ(kUserPriorityMin, GetUserPriority(device));

  audio_pref_handler_->SetUserPriorityHigherThan(device2, nullptr);
  EXPECT_LT(GetUserPriority(device2), GetUserPriority(device));
  EXPECT_NE(kUserPriorityNone, GetUserPriority(device2));
  EXPECT_NE(kUserPriorityNone, GetUserPriority(device));
}

TEST_P(AudioDevicesPrefHandlerTest, DropLeastRecentlySeenDevices) {
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        static int i = 0;
        i++;
        return base::Time::FromDoubleT(i);
      },
      nullptr, nullptr);

  AudioDevice d[3] = {
      GetDeviceWithVersion(2),
      GetSecondaryDeviceWithVersion(2),
      GetDeviceWithSpecialCharactersWithVersion(2),
  };

  audio_pref_handler_->SetUserPriorityHigherThan(d[0], nullptr);
  audio_pref_handler_->SetUserPriorityHigherThan(d[1], &d[0]);
  audio_pref_handler_->SetUserPriorityHigherThan(d[2], &d[1]);

  // All devices should have priorities assigned.
  ASSERT_NE(kUserPriorityNone, GetUserPriority(d[0]));
  ASSERT_NE(kUserPriorityNone, GetUserPriority(d[1]));
  ASSERT_NE(kUserPriorityNone, GetUserPriority(d[2]));

  audio_pref_handler_->DropLeastRecentlySeenDevices(
      /*connected_devices=*/{d[0], d[1], d[2]}, 3);
  // Keep 2 devices. Only the most recently seen d[0], d[2] should be left.
  audio_pref_handler_->DropLeastRecentlySeenDevices(
      /*connected_devices=*/{d[0], d[2]}, 2);
  EXPECT_NE(kUserPriorityNone, GetUserPriority(d[0]));
  EXPECT_EQ(kUserPriorityNone, GetUserPriority(d[1]));
  EXPECT_NE(kUserPriorityNone, GetUserPriority(d[2]));

  // Request to keep 1 device. But connected devices are always kept.
  audio_pref_handler_->DropLeastRecentlySeenDevices(
      /*connected_devices=*/{d[0], d[2]}, 1);
  EXPECT_NE(kUserPriorityNone, GetUserPriority(d[0]));
  EXPECT_EQ(kUserPriorityNone, GetUserPriority(d[1]));
  EXPECT_NE(kUserPriorityNone, GetUserPriority(d[2]));

  // Keep 1 devices. Only the most recently seen d[2] should be left.
  audio_pref_handler_->DropLeastRecentlySeenDevices(
      /*connected_devices=*/{d[2]}, 1);
  EXPECT_EQ(kUserPriorityNone, GetUserPriority(d[0]));
  EXPECT_EQ(kUserPriorityNone, GetUserPriority(d[1]));
  EXPECT_NE(kUserPriorityNone, GetUserPriority(d[2]));
}

}  // namespace ash
