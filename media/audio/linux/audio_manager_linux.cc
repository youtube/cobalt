// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/linux/audio_manager_linux.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/linux/alsa_input.h"
#include "media/audio/linux/alsa_output.h"
#include "media/audio/linux/alsa_wrapper.h"
#if defined(USE_PULSEAUDIO)
#include "media/audio/linux/pulse_output.h"
#endif
#include "media/base/limits.h"
#include "media/base/media_switches.h"

// Maximum number of output streams that can be open simultaneously.
static const size_t kMaxOutputStreams = 50;

static const int kMaxInputChannels = 2;

// Since "default", "pulse" and "dmix" devices are virtual devices mapped to
// real devices, we remove them from the list to avoiding duplicate counting.
// In addition, note that we support no more than 2 channels for recording,
// hence surround devices are not stored in the list.
static const char* kInvalidAudioInputDevices[] = {
  "default",
  "null",
  "pulse",
  "dmix",
  "surround",
};

static bool IsValidAudioInputDevice(const char* device_name) {
  if (!device_name)
    return false;

  for (size_t i = 0; i < arraysize(kInvalidAudioInputDevices); ++i) {
    if (strcmp(kInvalidAudioInputDevices[i], device_name) == 0)
      return false;
  }

  return true;
}

// Implementation of AudioManager.
bool AudioManagerLinux::HasAudioOutputDevices() {
  // TODO(ajwong): Make this actually query audio devices.
  return true;
}

bool AudioManagerLinux::HasAudioInputDevices() {
  if (!initialized()) {
    return false;
  }

  // Constants specified by the ALSA API for device hints.
  static const char kPcmInterfaceName[] = "pcm";
  bool has_device = false;
  void** hints = NULL;
  int card = -1;

  // Loop through the sound cards to get Alsa device hints.
  // Don't use snd_device_name_hint(-1,..) since there is a access violation
  // inside this ALSA API with libasound.so.2.0.0.
  while (!wrapper_->CardNext(&card) && (card >= 0) && !has_device) {
    int error = wrapper_->DeviceNameHint(card,
                                         kPcmInterfaceName,
                                         &hints);
    if (error == 0) {
      has_device = HasAnyValidAudioInputDevice(hints);

      // Destroy the hints now that we're done with it.
      wrapper_->DeviceNameFreeHint(hints);
      hints = NULL;
    } else {
      LOG(ERROR) << "Unable to get device hints: " << wrapper_->StrError(error);
    }
  }

  return has_device;
}

AudioOutputStream* AudioManagerLinux::MakeAudioOutputStream(
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
  } else {
#endif
    std::string device_name = AlsaPcmOutputStream::kAutoSelectDevice;
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAlsaOutputDevice)) {
      device_name = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAlsaOutputDevice);
    }
    stream = new AlsaPcmOutputStream(device_name, params, wrapper_.get(), this,
                                     GetMessageLoop());
#if defined(USE_PULSEAUDIO)
  }
#endif
  active_streams_.insert(stream);
  return stream;
}

AudioInputStream* AudioManagerLinux::MakeAudioInputStream(
    const AudioParameters& params) {
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

void AudioManagerLinux::ReleaseOutputStream(AudioOutputStream* stream) {
  if (stream) {
    active_streams_.erase(stream);
    delete stream;
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
  base::LaunchProcess(CommandLine(FilePath(command)), base::LaunchOptions(),
                      NULL);
}

void AudioManagerLinux::GetAudioInputDeviceNames(
    media::AudioDeviceNames* device_names) {
  // TODO(xians): query a full list of valid devices.
  if (HasAudioInputDevices()) {
    // Add the default device to the list.
    // We use index 0 to make up the unique_id to identify the
    // default devices.
    media::AudioDeviceName name;
    name.device_name = AudioManagerBase::kDefaultDeviceName;
    name.unique_id = "0";
    device_names->push_back(name);
  }
}

bool AudioManagerLinux::HasAnyValidAudioInputDevice(void** hints) {
  static const char kIoHintName[] = "IOID";
  static const char kNameHintName[] = "NAME";
  static const char kOutputDevice[] = "Output";

  for (void** hint_iter = hints; *hint_iter != NULL; hint_iter++) {
    // Only examine devices that are input capable.  Valid values are
    // "Input", "Output", and NULL which means both input and output.
    scoped_ptr_malloc<char> io(wrapper_->DeviceNameGetHint(*hint_iter,
                                                           kIoHintName));
    if (io != NULL && strcmp(kOutputDevice, io.get()) == 0)
      continue;

    scoped_ptr_malloc<char> hint_device_name(
        wrapper_->DeviceNameGetHint(*hint_iter, kNameHintName));
    if (IsValidAudioInputDevice(hint_device_name.get()))
      return true;
  }

  return false;
}

// static
AudioManager* AudioManager::CreateAudioManager() {
  return new AudioManagerLinux();
}
