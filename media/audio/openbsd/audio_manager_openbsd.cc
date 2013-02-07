// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/openbsd/audio_manager_openbsd.h"

#include "base/command_line.h"
#include "base/stl_util.h"
#include "media/audio/audio_output_dispatcher.h"
#if defined(USE_PULSEAUDIO)
#include "media/audio/pulse/pulse_output.h"
#endif
#include "media/base/limits.h"
#include "media/base/media_switches.h"

#include <fcntl.h>

namespace media {

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

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

AudioManagerOpenBSD::AudioManagerOpenBSD() {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerOpenBSD::~AudioManagerOpenBSD() {
  Shutdown();
}

AudioOutputStream* AudioManagerOpenBSD::MakeLinearOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format);
  return MakeOutputStream(params);
}

AudioOutputStream* AudioManagerOpenBSD::MakeLowLatencyOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format);
  return MakeOutputStream(params);
}

AudioInputStream* AudioManagerOpenBSD::MakeLinearInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format);
  NOTIMPLEMENTED();
  return NULL;
}

AudioInputStream* AudioManagerOpenBSD::MakeLowLatencyInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format);
  NOTIMPLEMENTED();
  return NULL;
}

AudioOutputStream* AudioManagerOpenBSD::MakeOutputStream(
    const AudioParameters& params) {
#if defined(USE_PULSEAUDIO)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUsePulseAudio)) {
    return new PulseAudioOutputStream(params, this);
  }
#endif

  NOTIMPLEMENTED();
  return NULL;
}

// static
AudioManager* CreateAudioManager() {
  return new AudioManagerOpenBSD();
}

}  // namespace media
