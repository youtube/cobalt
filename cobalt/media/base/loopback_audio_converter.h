// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_LOOPBACK_AUDIO_CONVERTER_H_
#define COBALT_MEDIA_BASE_LOOPBACK_AUDIO_CONVERTER_H_

#include "base/basictypes.h"
#include "cobalt/media/base/audio_converter.h"

namespace cobalt {
namespace media {

// LoopbackAudioConverter works similar to AudioConverter and converts input
// streams to different audio parameters. Then, the LoopbackAudioConverter can
// be used as an input to another AudioConverter. This allows us to
// use converted audio from AudioOutputStreams as input to an AudioConverter.
// For example, this allows converting multiple streams into a common format and
// using the converted audio as input to another AudioConverter (i.e. a mixer).
class LoopbackAudioConverter : public AudioConverter::InputCallback {
 public:
  LoopbackAudioConverter(const AudioParameters& input_params,
                         const AudioParameters& output_params,
                         bool disable_fifo);

  ~LoopbackAudioConverter() OVERRIDE;

  void AddInput(AudioConverter::InputCallback* input) {
    audio_converter_.AddInput(input);
  }

  void RemoveInput(AudioConverter::InputCallback* input) {
    audio_converter_.RemoveInput(input);
  }

  bool empty() { return audio_converter_.empty(); }

 private:
  double ProvideInput(AudioBus* audio_bus, uint32_t frames_delayed) OVERRIDE;

  AudioConverter audio_converter_;

  DISALLOW_COPY_AND_ASSIGN(LoopbackAudioConverter);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_LOOPBACK_AUDIO_CONVERTER_H_
