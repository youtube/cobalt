// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_AUDIO_BUFFER_CONVERTER_H_
#define COBALT_MEDIA_BASE_AUDIO_BUFFER_CONVERTER_H_

#include <deque>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "cobalt/media/base/audio_converter.h"
#include "cobalt/media/base/audio_parameters.h"
#include "cobalt/media/base/audio_timestamp_helper.h"
#include "cobalt/media/base/media_export.h"

namespace media {

class AudioBuffer;
class AudioBus;

// Takes AudioBuffers in any format and uses an AudioConverter to convert them
// to a common format (usually the hardware output format).
class MEDIA_EXPORT AudioBufferConverter : public AudioConverter::InputCallback {
 public:
  explicit AudioBufferConverter(const AudioParameters& output_params);
  ~AudioBufferConverter() OVERRIDE;

  void AddInput(const scoped_refptr<AudioBuffer>& buffer);

  // Is an output buffer available via GetNextBuffer()?
  bool HasNextBuffer();

  // This should only be called this is HasNextBuffer() returns true.
  scoped_refptr<AudioBuffer> GetNextBuffer();

  // Reset internal state.
  void Reset();

  // Reset internal timestamp state. Upon the next AddInput() call, our base
  // timestamp will be set to match the input buffer.
  void ResetTimestampState();

  int input_buffer_size_for_testing() const {
    return input_params_.frames_per_buffer();
  }
  int input_frames_left_for_testing() const { return input_frames_; }

 private:
  // Callback to provide data to the AudioConverter
  double ProvideInput(AudioBus* audio_bus, uint32_t frames_delayed) OVERRIDE;

  // Reset the converter in response to a configuration change.
  void ResetConverter(const scoped_refptr<AudioBuffer>& input_buffer);

  // Perform conversion if we have enough data.
  void ConvertIfPossible();

  // Flush remaining output
  void Flush();

  // The output parameters.
  AudioParameters output_params_;

  // The current input parameters (we cache these to detect configuration
  // changes, so we know when to reset the AudioConverter).
  AudioParameters input_params_;

  typedef std::deque<scoped_refptr<AudioBuffer> > BufferQueue;

  // Queued up inputs (there will never be all that much data stored here, as
  // soon as there's enough here to produce an output buffer we will do so).
  BufferQueue queued_inputs_;

  // Offset into the front element of |queued_inputs_|. A ProvideInput() call
  // doesn't necessarily always consume an entire buffer.
  int last_input_buffer_offset_;

  // Buffer of output frames, to be returned by GetNextBuffer().
  BufferQueue queued_outputs_;

  // How many frames of input we have in |queued_inputs_|.
  int input_frames_;

  // Input frames in the AudioConverter's internal buffers.
  double buffered_input_frames_;

  // Ratio of sample rates, in/out.
  double io_sample_rate_ratio_;

  // Computes timestamps in terms of the output sample rate.
  AudioTimestampHelper timestamp_helper_;

  // Are we flushing everything, without regard for providing AudioConverter
  // full AudioBuses in ProvideInput()?
  bool is_flushing_;

  // The AudioConverter which does the real work here.
  std::unique_ptr<AudioConverter> audio_converter_;
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_AUDIO_BUFFER_CONVERTER_H_
