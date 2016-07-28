// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "media/audio/win/core_audio_util_win.h"
#endif

namespace media {

class AudioInputVolumeTest : public ::testing::Test {
 protected:
  AudioInputVolumeTest()
      : audio_manager_(AudioManager::Create())
#if defined(OS_WIN)
       , com_init_(base::win::ScopedCOMInitializer::kMTA)
#endif
  {
  }

  bool CanRunAudioTests() {
#if defined(OS_WIN)
    // TODO(henrika): add support for volume control on Windows XP as well.
    // For now, we might as well signal false already here to avoid running
    // these tests on Windows XP.
    if (!CoreAudioUtil::IsSupported())
      return false;
#endif
    if (!audio_manager_.get())
      return false;

    return audio_manager_->HasAudioInputDevices();
  }

  // Helper method which checks if the stream has volume support.
  bool HasDeviceVolumeControl(AudioInputStream* stream) {
    if (!stream)
      return false;

    return (stream->GetMaxVolume() != 0.0);
  }

  AudioInputStream* CreateAndOpenStream(const std::string& device_id) {
    AudioParameters::Format format = AudioParameters::AUDIO_PCM_LOW_LATENCY;
    ChannelLayout channel_layout =
        media::GetAudioInputHardwareChannelLayout(device_id);
    int bits_per_sample = 16;
    int sample_rate =
        static_cast<int>(media::GetAudioInputHardwareSampleRate(device_id));
    int samples_per_packet = 0;
#if defined(OS_MACOSX)
    samples_per_packet = (sample_rate / 100);
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
    samples_per_packet = (sample_rate / 100);
#elif defined(OS_WIN)
    if (sample_rate == 44100)
      samples_per_packet = 448;
    else
      samples_per_packet = (sample_rate / 100);
#else
#error Unsupported platform
#endif
    AudioInputStream* ais = audio_manager_->MakeAudioInputStream(
        AudioParameters(format, channel_layout, sample_rate, bits_per_sample,
                        samples_per_packet),
        device_id);
    EXPECT_TRUE(NULL != ais);

#if defined(OS_LINUX) || defined(OS_OPENBSD)
    // Some linux devices do not support our settings, we may fail to open
    // those devices.
    if (!ais->Open()) {
      // Default device should always be able to be opened.
      EXPECT_TRUE(AudioManagerBase::kDefaultDeviceId != device_id);
      ais->Close();
      ais = NULL;
    }
#elif defined(OS_WIN) || defined(OS_MACOSX)
    EXPECT_TRUE(ais->Open());
#endif

    return ais;
  }

  scoped_ptr<AudioManager> audio_manager_;

#if defined(OS_WIN)
  base::win::ScopedCOMInitializer com_init_;
#endif
};

TEST_F(AudioInputVolumeTest, InputVolumeTest) {
  if (!CanRunAudioTests())
    return;

  // Retrieve a list of all available input devices.
  AudioDeviceNames device_names;
  audio_manager_->GetAudioInputDeviceNames(&device_names);
  if (device_names.empty()) {
    LOG(WARNING) << "Could not find any available input device";
    return;
  }

  // Scan all available input devices and repeat the same test for all of them.
  for (AudioDeviceNames::const_iterator it = device_names.begin();
       it != device_names.end();
       ++it) {
    AudioInputStream* ais = CreateAndOpenStream(it->unique_id);
    if (!ais) {
      DLOG(WARNING) << "Failed to open stream for device " << it->unique_id;
      continue;
    }

    if (!HasDeviceVolumeControl(ais)) {
      DLOG(WARNING) << "Device: " << it->unique_id
                    << ", does not have volume control.";
      ais->Close();
      continue;
    }

    double max_volume = ais->GetMaxVolume();
    EXPECT_GT(max_volume, 0.0);

    // Store the current input-device volume level.
    double original_volume = ais->GetVolume();
    EXPECT_GE(original_volume, 0.0);
#if defined(OS_WIN) || defined(OS_MACOSX)
    // Note that |original_volume| can be higher than |max_volume| on Linux.
    EXPECT_LE(original_volume, max_volume);
#endif

    // Set the volume to the maxiumum level..
    ais->SetVolume(max_volume);
    double current_volume = ais->GetVolume();
    EXPECT_EQ(max_volume, current_volume);

    // Set the volume to the mininum level (=0).
    double new_volume = 0.0;
    ais->SetVolume(new_volume);
    current_volume = ais->GetVolume();
    EXPECT_EQ(new_volume, current_volume);

    // Set the volume to the mid level (50% of max).
    // Verify that the absolute error is small enough.
    new_volume = max_volume / 2;
    ais->SetVolume(new_volume);
    current_volume = ais->GetVolume();
    EXPECT_LT(current_volume, max_volume);
    EXPECT_GT(current_volume, 0);
    EXPECT_NEAR(current_volume, new_volume, 0.25 * max_volume);

    // Restores the volume to the original value.
    ais->SetVolume(original_volume);
    current_volume = ais->GetVolume();
    EXPECT_EQ(original_volume, current_volume);

    ais->Close();
  }
}

}  // namespace media
