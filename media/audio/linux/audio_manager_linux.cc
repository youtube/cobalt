// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/linux/audio_manager_linux.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/process_util.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/linux/alsa_input.h"
#include "media/audio/linux/alsa_output.h"
#include "media/audio/linux/alsa_wrapper.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

namespace {

// Maximum number of output streams that can be open simultaneously.
const size_t kMaxOutputStreams = 50;

const int kMaxInputChannels = 2;

}  // namespace

// Implementation of AudioManager.
bool AudioManagerLinux::HasAudioOutputDevices() {
  // TODO(ajwong): Make this actually query audio devices.
  return true;
}

bool AudioManagerLinux::HasAudioInputDevices() {
  // TODO(satish): Make this actually query audio devices.
  return true;
}

AudioOutputStream* AudioManagerLinux::MakeAudioOutputStream(
    AudioParameters params) {
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

  std::string device_name = AlsaPcmOutputStream::kAutoSelectDevice;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAlsaOutputDevice)) {
    device_name = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kAlsaOutputDevice);
  }
  AlsaPcmOutputStream* stream =
      new AlsaPcmOutputStream(device_name, params, wrapper_.get(), this,
                              GetMessageLoop());

  base::AutoLock l(lock_);
  active_streams_[stream] = scoped_refptr<AlsaPcmOutputStream>(stream);
  return stream;
}

AudioInputStream* AudioManagerLinux::MakeAudioInputStream(
    AudioParameters params) {
  if (!params.IsValid() || params.channels > kMaxInputChannels)
    return NULL;

  if (params.format == AudioParameters::AUDIO_MOCK) {
    return FakeAudioInputStream::MakeFakeStream(params);
  } else if (params.format != AudioParameters::AUDIO_PCM_LINEAR) {
    return NULL;
  }

  if (!initialized())
    return NULL;

  std::string device_name = AlsaPcmOutputStream::kAutoSelectDevice;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAlsaInputDevice)) {
    device_name = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kAlsaInputDevice);
  }

  AlsaPcmInputStream* stream = new AlsaPcmInputStream(
      device_name, params, wrapper_.get());

  return stream;
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

  // Free output dispatchers, closing all remaining open streams.
  output_dispatchers_.clear();

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
    base::AutoLock l(lock_);
    active_streams_.erase(stream);
  }
}

bool AudioManagerLinux::CanShowAudioInputSettings() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  base::nix::DesktopEnvironment desktop = base::nix::GetDesktopEnvironment(
      env.get());
  return (desktop == base::nix::DESKTOP_ENVIRONMENT_GNOME ||
          desktop == base::nix::DESKTOP_ENVIRONMENT_KDE3 ||
          desktop == base::nix::DESKTOP_ENVIRONMENT_KDE4);
}

void AudioManagerLinux::ShowAudioInputSettings() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  base::nix::DesktopEnvironment desktop = base::nix::GetDesktopEnvironment(
      env.get());
  std::string command((desktop == base::nix::DESKTOP_ENVIRONMENT_GNOME) ?
                      "gnome-volume-control" : "kmix");
  base::LaunchApp(CommandLine(FilePath(command)), false, false, NULL);
}

// static
AudioManager* AudioManager::CreateAudioManager() {
  return new AudioManagerLinux();
}
