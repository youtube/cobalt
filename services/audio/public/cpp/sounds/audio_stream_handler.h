// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_AUDIO_STREAM_HANDLER_H_
#define SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_AUDIO_STREAM_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "media/base/audio_codecs.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"

namespace audio {

// This class sends a sound to the audio output device.
class COMPONENT_EXPORT(AUDIO_PUBLIC_CPP) AudioStreamHandler {
 public:
  // `resource_id` for the corresponding audio (WAV or FLAC) data which will be
  // sent to the audio output device.
  AudioStreamHandler(SoundsManager::StreamFactoryBinder stream_factory_binder,
                     int resource_id,
                     media::AudioCodec codec,
                     bool loop = false);

  AudioStreamHandler(const AudioStreamHandler&) = delete;
  AudioStreamHandler& operator=(const AudioStreamHandler&) = delete;

  virtual ~AudioStreamHandler();

  // Returns true iff AudioStreamHandler is correctly initialized;
  bool IsInitialized() const;

  // Plays sound.  Volume level will be set according to current settings
  // and won't be changed during playback. Returns true iff new playback
  // was successfully started.
  //
  // NOTE: if current playback isn't at end of stream, playback request
  // is dropped, but true is returned.
  bool Play();

  // Stops current playback.
  void Stop();

  // Pauses current playback. Returns true iff playback was successfully paused.
  bool Pause();

  // Get the duration of the WAV data passed in.
  base::TimeDelta duration() const;

 private:
  class AudioStreamContainer;

  base::TimeDelta duration_;
  base::SequenceBound<AudioStreamContainer> stream_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_AUDIO_STREAM_HANDLER_H_
