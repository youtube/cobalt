// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::ScopedCOMInitializer;
using media::AudioDeviceNames;

class AudioInputVolumeTest : public ::testing::Test {
 protected:
  AudioInputVolumeTest()
      : audio_manager_(AudioManager::Create()),
        com_init_(ScopedCOMInitializer::kMTA) {
  }

  bool CanRunAudioTests() {
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
    // TODO(xians): Implement a generic HardwareChannelCount API to query
    // the number of channel for all the devices.
    ChannelLayout channel_layout =
        (media::GetAudioInputHardwareChannelCount() == 1) ?
            CHANNEL_LAYOUT_MONO : CHANNEL_LAYOUT_STEREO;
    int bits_per_sample = 16;
    int sample_rate =
        static_cast<int>(media::GetAudioInputHardwareSampleRate());
    int samples_per_packet = 0;
#if defined(OS_MACOSX)
    samples_per_packet = (sample_rate / 100);
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
    samples_per_packet = (sample_rate / 100);
#elif defined(OS_WIN)
    if (media::IsWASAPISupported()) {
      if (sample_rate == 44100)
        samples_per_packet = 448;
      else
        samples_per_packet = (sample_rate / 100);
    } else {
      samples_per_packet = 3 * (sample_rate / 100);
    }
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
  ScopedCOMInitializer com_init_;
};

TEST_F(AudioInputVolumeTest, InputVolumeTest) {
  if (!CanRunAudioTests())
    return;

  AudioDeviceNames device_names;
  audio_manager_->GetAudioInputDeviceNames(&device_names);
  DCHECK(!device_names.empty());

  for (AudioDeviceNames::const_iterator it = device_names.begin();
       it != device_names.end();
       ++it) {
    AudioInputStream* ais = CreateAndOpenStream(it->unique_id);
    if (!ais) {
      DLOG(WARNING) << "Failed to open stream for device " << it->unique_id;
      continue;
    }

    if ( !HasDeviceVolumeControl(ais)) {
      DLOG(WARNING) << "Device: " << it->unique_id
                    << ", does not have volume control";
      ais->Close();
      continue;
    }

    double max_volume = ais->GetMaxVolume();
    EXPECT_GT(max_volume, 0.0);

    // Notes that |original_volume| can be higher than |max_volume| on Linux.
    double original_volume = ais->GetVolume();
    EXPECT_GE(original_volume, 0.0);
#if defined(OS_WIN) || defined(OS_MACOSX)
    EXPECT_LE(original_volume, max_volume);
#endif

    // Tries to set the volume to |max_volume|.
    ais->SetVolume(max_volume);
    double current_volume = ais->GetVolume();
    EXPECT_EQ(max_volume, current_volume);

    // Tries to set the volume to zero.
    double new_volume = 0.0;
    ais->SetVolume(new_volume);
    current_volume = ais->GetVolume();
    EXPECT_EQ(new_volume, current_volume);

    // Tries to set the volume to the middle.
    new_volume = max_volume / 2;
    ais->SetVolume(new_volume);
    current_volume = ais->GetVolume();
    EXPECT_LT(current_volume, max_volume);
    EXPECT_GT(current_volume, 0);

    // Restores the volume to the original value.
    ais->SetVolume(original_volume);
    current_volume = ais->GetVolume();
    EXPECT_EQ(original_volume, current_volume);

    ais->Close();
  }
}
