// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/linux/audio_manager_linux.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/linux/alsa_output.h"
#include "media/audio/linux/alsa_wrapper.h"
#include "media/base/media_switches.h"

// Implementation of AudioManager.
bool AudioManagerLinux::HasAudioOutputDevices() {
  // TODO(ajwong): Make this actually query audio devices.
  return true;
}

bool AudioManagerLinux::HasAudioInputDevices() {
  // TODO(satish): implement.
  return false;
}

AudioOutputStream* AudioManagerLinux::MakeAudioOutputStream(
    AudioParameters params) {
  // Early return for testing hook.  Do this before checking for
  // |initialized_|.
  if (params.format == AudioParameters::AUDIO_MOCK) {
    return FakeAudioOutputStream::MakeFakeStream();
  }

  if (!initialized()) {
    return NULL;
  }

  std::string device_name = AlsaPcmOutputStream::kAutoSelectDevice;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAlsaDevice)) {
    device_name = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kAlsaDevice);
  }
  AlsaPcmOutputStream* stream =
      new AlsaPcmOutputStream(device_name, params, wrapper_.get(), this,
                              GetMessageLoop());

  AutoLock l(lock_);
  active_streams_[stream] = scoped_refptr<AlsaPcmOutputStream>(stream);
  return stream;
}

AudioInputStream* AudioManagerLinux::MakeAudioInputStream(
    AudioParameters params, uint32 samples_per_packet) {
  if (params.format == AudioParameters::AUDIO_MOCK) {
    return FakeAudioInputStream::MakeFakeStream(params, samples_per_packet);
  }
  // TODO(satish): implement.
  return NULL;
}

AudioManagerLinux::AudioManagerLinux() {
}

AudioManagerLinux::~AudioManagerLinux() {
  // Make sure we stop the thread first. If we let the default destructor to
  // destruct the members, we may destroy audio streams before stopping the
  // thread, resulting an unexpected behavior.
  // This way we make sure activities of the audio streams are all stopped
  // before we destroy them.
  audio_thread_.Stop();
  active_streams_.clear();
}

void AudioManagerLinux::Init() {
  AudioManagerBase::Init();
  wrapper_.reset(new AlsaWrapper());
}

void AudioManagerLinux::MuteAll() {
  NOTIMPLEMENTED();
}

void AudioManagerLinux::UnMuteAll() {
  NOTIMPLEMENTED();
}

void AudioManagerLinux::ReleaseOutputStream(AlsaPcmOutputStream* stream) {
  if (stream) {
    AutoLock l(lock_);
    active_streams_.erase(stream);
  }
}

// static
AudioManager* AudioManager::CreateAudioManager() {
  return new AudioManagerLinux();
}
