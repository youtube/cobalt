// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#if defined(OS_WIN)
#include "media/audio/win/audio_manager_win.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"

using base::win::ScopedCOMInitializer;
using media::AudioDeviceNames;

// Test fixture which allows us to override the default enumeration API on
// Windows.
class AudioInputDeviceTest
    : public ::testing::Test {
 protected:
#if defined(OS_WIN)
  // Store current device-enumeration type to ensure that it can be restored
  // after last test in this test suite.
  static void SetUpTestCase() {
    enumeration_type_ = static_cast<AudioManagerWin*>(
        AudioManager::GetAudioManager())->enumeration_type();
  }

  // Restore pre-test state of device-enumeration type.
  static void TearDownTestCase() {
    static_cast<AudioManagerWin*>(
      AudioManager::GetAudioManager())->SetEnumerationType(enumeration_type_);
  }

  bool SetMMDeviceEnumeration(AudioManager* audio_manager) {
    AudioManagerWin* amw = static_cast<AudioManagerWin*>(audio_manager);
    // Windows Wave is used as default if Windows XP was detected =>
    // return false since MMDevice is not supported on XP.
    if (amw->enumeration_type() == AudioManagerWin::kWaveEnumeration)
      return false;

    amw->SetEnumerationType(AudioManagerWin::kMMDeviceEnumeration);
    return true;
  }

  void SetWaveEnumeration(AudioManager* audio_manager) {
    AudioManagerWin* amw = static_cast<AudioManagerWin*>(audio_manager);
    amw->SetEnumerationType(AudioManagerWin::kWaveEnumeration);
  }

  // Stores the pre-test state representing the device-enumeration type.
  static AudioManagerWin::EnumerationType enumeration_type_;
#endif
};

#if defined(OS_WIN)
AudioManagerWin::EnumerationType AudioInputDeviceTest::enumeration_type_ =
    AudioManagerWin::kUninitializedEnumeration;
#endif

// Convenience method which ensures that we are not running on the build
// bots which lacks audio device support.
static bool CanRunAudioTests() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (env->HasVar("CHROME_HEADLESS"))
    return false;
  return true;
}

// Helper method which verifies that the device list starts with a valid
// default record followed by non-default device names.
static void CheckDeviceNames(const AudioDeviceNames& device_names) {
  if (!device_names.empty()) {
    AudioDeviceNames::const_iterator it = device_names.begin();

    // The first device in the list should always be the default device.
    EXPECT_EQ(std::string(AudioManagerBase::kDefaultDeviceName),
      it->device_name);
    EXPECT_EQ(std::string(AudioManagerBase::kDefaultDeviceId), it->unique_id);
    ++it;

    // Other devices should have non-empty name and id and should not contain
    // default name or id.
    while (it != device_names.end()) {
      EXPECT_FALSE(it->device_name.empty());
      EXPECT_FALSE(it->unique_id.empty());
      EXPECT_NE(std::string(AudioManagerBase::kDefaultDeviceName),
                it->device_name);
      EXPECT_NE(std::string(AudioManagerBase::kDefaultDeviceId),
                it->unique_id);
      ++it;
    }
  }
}

// Test that devices can be enumerated.
TEST_F(AudioInputDeviceTest, EnumerateDevices) {
  if (!CanRunAudioTests())
    return;
  // The MMDevice API requires a correct COM environment.
  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);
  AudioDeviceNames device_names;
  AudioManager* audio_man = AudioManager::GetAudioManager();
  EXPECT_TRUE(audio_man != NULL);
  audio_man->GetAudioInputDeviceNames(&device_names);
  CheckDeviceNames(device_names);
}

// Run additional tests for Windows since enumeration can be done using
// two different APIs. MMDevice is default for Vista and higher and Wave
// is default for XP and lower.
#if defined(OS_WIN)

// Override default enumeration API and force usage of Windows MMDevice.
// This test will only run on Windows Vista and higher.
TEST_F(AudioInputDeviceTest, EnumerateDevicesWinMMDevice) {
  if (!CanRunAudioTests())
    return;
  // The MMDevice API requires a correct COM environment.
  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);
  AudioDeviceNames device_names;
  AudioManager* audio_man = AudioManager::GetAudioManager();
  if (!SetMMDeviceEnumeration(audio_man)) {
    // Usage of MMDevice will fail on XP and lower.
    return;
  }
  audio_man->GetAudioInputDeviceNames(&device_names);
  CheckDeviceNames(device_names);
}

// Override default enumeration API and force usage of Windows Wave.
// This test will run on Windows XP, Windows Vista and Windows 7.
TEST_F(AudioInputDeviceTest, EnumerateDevicesWinWave) {
  if (!CanRunAudioTests())
    return;
  AudioDeviceNames device_names;
  AudioManager* audio_man = AudioManager::GetAudioManager();
  EXPECT_TRUE(audio_man != NULL);
  SetWaveEnumeration(audio_man);
  audio_man->GetAudioInputDeviceNames(&device_names);
  CheckDeviceNames(device_names);
}

#endif
