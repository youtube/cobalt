// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAST_SENDER_AUDIO_ENCODER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAST_SENDER_AUDIO_ENCODER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "third_party/chromium/media/base/audio_bus.h"
#include "third_party/chromium/media/cast/cast_environment.h"
#include "third_party/chromium/media/cast/constants.h"
#include "third_party/chromium/media/cast/sender/sender_encoded_frame.h"

namespace base {
class TimeTicks;
}

namespace media_m96 {
namespace cast {

class AudioEncoder {
 public:
  // Callback to deliver each SenderEncodedFrame, plus the number of audio
  // samples skipped since the last frame.
  using FrameEncodedCallback =
      base::RepeatingCallback<void(std::unique_ptr<SenderEncodedFrame>, int)>;

  AudioEncoder(const scoped_refptr<CastEnvironment>& cast_environment,
               int num_channels,
               int sampling_rate,
               int bitrate,
               Codec codec,
               FrameEncodedCallback frame_encoded_callback);

  AudioEncoder(const AudioEncoder&) = delete;
  AudioEncoder& operator=(const AudioEncoder&) = delete;

  virtual ~AudioEncoder();

  OperationalStatus InitializationResult() const;

  int GetSamplesPerFrame() const;
  base::TimeDelta GetFrameDuration() const;

  void InsertAudio(std::unique_ptr<AudioBus> audio_bus,
                   const base::TimeTicks& recorded_time);

 private:
  class ImplBase;
  class OpusImpl;
  class Pcm16Impl;
  class AppleAacImpl;

  const scoped_refptr<CastEnvironment> cast_environment_;
  scoped_refptr<ImplBase> impl_;

  // Used to ensure only one thread invokes InsertAudio().
  base::ThreadChecker insert_thread_checker_;
};

}  // namespace cast
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAST_SENDER_AUDIO_ENCODER_H_
