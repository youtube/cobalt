// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/wavein_input_win.h"
#endif

namespace media {

// Test fixture which allows us to override the default enumeration API on
// Windows.
class AudioInputDeviceTest
    : public ::testing::Test {
 protected:
  AudioInputDeviceTest()
      : audio_manager_(AudioManager::Create())
#if defined(OS_WIN)
      , com_init_(base::win::ScopedCOMInitializer::kMTA)
#endif
  {
  }

#if defined(OS_WIN)
  bool SetMMDeviceEnumeration() {
    AudioManagerWin* amw = static_cast<AudioManagerWin*>(audio_manager_.get());
    // Windows Wave is used as default if Windows XP was detected =>
    // return false since MMDevice is not supported on XP.
    if (amw->enumeration_type() == AudioManagerWin::kWaveEnumeration)
      return false;

    amw->SetEnumerationType(AudioManagerWin::kMMDeviceEnumeration);
    return true;
  }

  void SetWaveEnumeration() {
    AudioManagerWin* amw = static_cast<AudioManagerWin*>(audio_manager_.get());
    amw->SetEnumerationType(AudioManagerWin::kWaveEnumeration);
  }

  std::string GetDeviceIdFromPCMWaveInAudioInputStream(
      const std::string& device_id) {
    AudioManagerWin* amw = static_cast<AudioManagerWin*>(audio_manager_.get());
    AudioParameters parameters(
        AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
        AudioParameters::kAudioCDSampleRate, 16,
        1024);
    scoped_ptr<PCMWaveInAudioInputStream> stream(
        static_cast<PCMWaveInAudioInputStream*>(
            amw->CreatePCMWaveInAudioInputStream(parameters, device_id)));
    return stream.get() ? stream->device_id_ : std::string();
  }
#endif

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
    } else {
      // Log a warning so we can see the status on the build bots.  No need to
      // break the test though since this does successfully test the code and
      // some failure cases.
      LOG(WARNING) << "No input devices detected";
    }
  }

  bool CanRunAudioTest() {
    return audio_manager_->HasAudioInputDevices();
  }

  scoped_ptr<AudioManager> audio_manager_;

#if defined(OS_WIN)
  // The MMDevice API requires COM to be initialized on the current thread.
  base::win::ScopedCOMInitializer com_init_;
#endif
};

// Test that devices can be enumerated.
TEST_F(AudioInputDeviceTest, EnumerateDevices) {
  if (!CanRunAudioTest())
    return;

  AudioDeviceNames device_names;
  audio_manager_->GetAudioInputDeviceNames(&device_names);
  CheckDeviceNames(device_names);
}

// Run additional tests for Windows since enumeration can be done using
// two different APIs. MMDevice is default for Vista and higher and Wave
// is default for XP and lower.
#if defined(OS_WIN)

// Override default enumeration API and force usage of Windows MMDevice.
// This test will only run on Windows Vista and higher.
TEST_F(AudioInputDeviceTest, EnumerateDevicesWinMMDevice) {
  if (!CanRunAudioTest())
    return;

  AudioDeviceNames device_names;
  if (!SetMMDeviceEnumeration()) {
    // Usage of MMDevice will fail on XP and lower.
    LOG(WARNING) << "MM device enumeration is not supported.";
    return;
  }
  audio_manager_->GetAudioInputDeviceNames(&device_names);
  CheckDeviceNames(device_names);
}

// Override default enumeration API and force usage of Windows Wave.
// This test will run on Windows XP, Windows Vista and Windows 7.
TEST_F(AudioInputDeviceTest, EnumerateDevicesWinWave) {
  if (!CanRunAudioTest())
    return;

  AudioDeviceNames device_names;
  SetWaveEnumeration();
  audio_manager_->GetAudioInputDeviceNames(&device_names);
  CheckDeviceNames(device_names);
}

TEST_F(AudioInputDeviceTest, WinXPDeviceIdUnchanged) {
  if (!CanRunAudioTest())
    return;

  AudioDeviceNames xp_device_names;
  SetWaveEnumeration();
  audio_manager_->GetAudioInputDeviceNames(&xp_device_names);
  CheckDeviceNames(xp_device_names);

  // Device ID should remain unchanged, including the default device ID.
  for (AudioDeviceNames::iterator i = xp_device_names.begin();
       i != xp_device_names.end(); ++i) {
    EXPECT_EQ(i->unique_id,
              GetDeviceIdFromPCMWaveInAudioInputStream(i->unique_id));
  }
}

TEST_F(AudioInputDeviceTest, ConvertToWinXPDeviceId) {
  if (!CanRunAudioTest())
    return;

  if (!SetMMDeviceEnumeration()) {
    // Usage of MMDevice will fail on XP and lower.
    LOG(WARNING) << "MM device enumeration is not supported.";
    return;
  }

  AudioDeviceNames device_names;
  audio_manager_->GetAudioInputDeviceNames(&device_names);
  CheckDeviceNames(device_names);

  for (AudioDeviceNames::iterator i = device_names.begin();
       i != device_names.end(); ++i) {
    std::string converted_id =
        GetDeviceIdFromPCMWaveInAudioInputStream(i->unique_id);
    if (i == device_names.begin()) {
      // The first in the list is the default device ID, which should not be
      // changed when passed to PCMWaveInAudioInputStream.
      EXPECT_EQ(i->unique_id, converted_id);
    } else {
      // MMDevice-style device IDs should be converted to WaveIn-style device
      // IDs.
      EXPECT_NE(i->unique_id, converted_id);
    }
  }
}

#endif

}  // namespace media
