// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/openbsd/audio_manager_openbsd.h"

#include "base/command_line.h"
#include "base/stl_util.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#if defined(USE_PULSEAUDIO)
#include "media/audio/pulse/pulse_output.h"
#endif
#include "media/base/limits.h"
#include "media/base/media_switches.h"

#include <fcntl.h>

// Maximum number of output streams that can be open simultaneously.
static const size_t kMaxOutputStreams = 50;

// Implementation of AudioManager.
static bool HasAudioHardware() {
  int fd;
  const char *file;

  if ((file = getenv("AUDIOCTLDEVICE")) == 0 || *file == '\0')
    file = "/dev/audioctl";

  if ((fd = open(file, O_RDONLY)) < 0)
    return false;

  close(fd);
  return true;
}

bool AudioManagerOpenBSD::HasAudioOutputDevices() {
  return HasAudioHardware();
}

bool AudioManagerOpenBSD::HasAudioInputDevices() {
  return HasAudioHardware();
}

AudioOutputStream* AudioManagerOpenBSD::MakeAudioOutputStream(
    const AudioParameters& params) {
  // Early return for testing hook.  Do this before checking for
  // |initialized_|.
  if (params.format == AudioParameters::AUDIO_MOCK) {
    return FakeAudioOutputStream::MakeFakeStream(params);
  }

  if (!initialized()) {
    return NULL;
  }

  // Don't allow opening more than |kMaxOutputStreams| streams.
  if (active_streams_.size() >= kMaxOutputStreams) {
    return NULL;
  }

  AudioOutputStream* stream;
#if defined(USE_PULSEAUDIO)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUsePulseAudio)) {
    stream = new PulseAudioOutputStream(params, this, GetMessageLoop());
    active_streams_.insert(stream);
    return stream;
  }
#endif

  NOTIMPLEMENTED();
  return NULL;
}

AudioInputStream* AudioManagerOpenBSD::MakeAudioInputStream(
    const AudioParameters& params, const std::string& device_id) {
  NOTIMPLEMENTED();
  return NULL;
}

AudioManagerOpenBSD::AudioManagerOpenBSD() {
}

AudioManagerOpenBSD::~AudioManagerOpenBSD() {
  // Make sure we stop the thread first. If we allow the default destructor to
  // destroy the members, we may destroy audio streams before stopping the
  // thread, resulting an unexpected behavior.
  // This way we make sure activities of the audio streams are all stopped
  // before we destroy them.
  audio_thread_.Stop();

  // Free output dispatchers, closing all remaining open streams.
  output_dispatchers_.clear();

  // Delete all the streams. Have to do it manually, we don't have ScopedSet<>,
  // and we are not using ScopedVector<> because search there is slow.
  STLDeleteElements(&active_streams_);
}

void AudioManagerOpenBSD::Init() {
  AudioManagerBase::Init();
}

void AudioManagerOpenBSD::MuteAll() {
  NOTIMPLEMENTED();
}

void AudioManagerOpenBSD::UnMuteAll() {
  NOTIMPLEMENTED();
}

void AudioManagerOpenBSD::ReleaseOutputStream(AudioOutputStream* stream) {
  if (stream) {
    active_streams_.erase(stream);
    delete stream;
  }
}

// static
AudioManager* AudioManager::CreateAudioManager() {
  return new AudioManagerOpenBSD();
}
