// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_ENCODER_H_

#include <memory>

#include "base/threading/thread_checker.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

// Base interface for an AudioTrackEncoder. This class and its subclasses are
// used by AudioTrackRecorder to encode audio before output. These are private
// classes and should not be used outside of AudioTrackRecorder.
//
// AudioTrackEncoder is created on the ATR's main thread (usually the main
// render thread) but is otherwise operated entirely on |encoder_thread_|,
// which is owned by AudioTrackRecorder.
class AudioTrackEncoder {
 public:
  using OnEncodedAudioCB =
      base::RepeatingCallback<void(const media::AudioParameters& params,
                                   std::string encoded_data,
                                   base::TimeTicks capture_time)>;

  explicit AudioTrackEncoder(OnEncodedAudioCB on_encoded_audio_cb);
  virtual ~AudioTrackEncoder() = default;

  AudioTrackEncoder(const AudioTrackEncoder&) = delete;
  AudioTrackEncoder& operator=(const AudioTrackEncoder&) = delete;

  virtual void OnSetFormat(const media::AudioParameters& params) = 0;
  virtual void EncodeAudio(std::unique_ptr<media::AudioBus> audio_bus,
                           base::TimeTicks capture_time) = 0;

  void set_paused(bool paused) { paused_ = paused; }

 protected:
  bool paused_;

  const OnEncodedAudioCB on_encoded_audio_cb_;

  // The original input audio parameters.
  media::AudioParameters input_params_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_ENCODER_H_
