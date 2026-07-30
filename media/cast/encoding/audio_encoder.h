// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_AUDIO_ENCODER_H_
#define MEDIA_CAST_ENCODING_AUDIO_ENCODER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/audio_bus.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/constants.h"

namespace base {
class TimeTicks;
}

namespace media {

enum class AudioCodec;

namespace cast {

class AudioEncoder {
 public:
  // Callback to deliver each SenderEncodedFrame, plus the number of audio
  // samples skipped since the last frame.
  using FrameEncodedCallback =
      base::RepeatingCallback<void(std::unique_ptr<SenderEncodedFrame>, int)>;

  using EncodeCallback =
      base::RepeatingCallback<void(std::unique_ptr<AudioBus> audio_bus,
                                   base::TimeTicks recorded_time)>;

  AudioEncoder(const scoped_refptr<CastEnvironment>& cast_environment,
               int num_channels,
               int sampling_rate,
               uint32_t bitrate,
               AudioCodec codec,
               FrameEncodedCallback frame_encoded_callback);

  AudioEncoder(const AudioEncoder&) = delete;
  AudioEncoder& operator=(const AudioEncoder&) = delete;

  virtual ~AudioEncoder();

  OperationalStatus InitializationResult() const;

  int GetSamplesPerFrame() const;
  base::TimeDelta GetFrameDuration() const;
  uint32_t GetBitrate() const;
  void InsertAudio(std::unique_ptr<AudioBus> audio_bus,
                   base::TimeTicks recorded_time);

  // Returns a callback that can be safely called from any thread.
  // It automatically hops the payload to the kAudio thread.
  EncodeCallback GetAsynchronousEncodeCallback();

 private:
  class ImplBase;
  class OpusImpl;
  class Pcm16Impl;
  class AppleAacImpl;

  const scoped_refptr<CastEnvironment> cast_environment_;
  scoped_refptr<ImplBase> impl_;

  // Used to ensure only one thread invokes InsertAudio().
  THREAD_CHECKER(insert_thread_checker_);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_ENCODING_AUDIO_ENCODER_H_
