// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WAV_AUDIO_HANDLER_H_
#define MEDIA_AUDIO_WAV_AUDIO_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

class AudioBus;

// This class provides the input from wav file format.  See
// https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
class MEDIA_EXPORT WavAudioHandler {
 public:
  // Supported audio format.
  enum class AudioFormat : uint16_t {
    kAudioFormatPCM = 0x0001,
    kAudioFormatFloat = 0x0003,
    kAudioFormatExtensible = 0xfffe
  };

  WavAudioHandler(const WavAudioHandler&) = delete;
  WavAudioHandler& operator=(const WavAudioHandler&) = delete;

  virtual ~WavAudioHandler();

  // Create a WavAudioHandler using |wav_data|. If |wav_data| cannot be parsed
  // correctly, the returned scoped_ptr will be null. The underlying memory for
  // wav_data must survive for the lifetime of this class.
  static std::unique_ptr<WavAudioHandler> Create(
      const base::StringPiece wav_data);

  // Returns true when cursor points to the end of the track.
  bool AtEnd(size_t cursor) const;

  // Copies the audio data to |bus| starting from the |cursor| and in
  // the case of success stores the number of written bytes in
  // |bytes_written|. |bytes_written| should not be NULL. Returns false if the
  // operation was unsuccessful. Returns true otherwise.
  bool CopyTo(AudioBus* bus, size_t cursor, size_t* bytes_written) const;

  // Accessors.
  const base::StringPiece& data() const { return data_; }
  int num_channels() const { return static_cast<int>(num_channels_); }
  int sample_rate() const { return static_cast<int>(sample_rate_); }
  int bits_per_sample() const { return static_cast<int>(bits_per_sample_); }
  AudioFormat audio_format() const { return audio_format_; }
  int total_frames() const { return static_cast<int>(total_frames_); }

  // Returns the duration of the entire audio chunk.
  base::TimeDelta GetDuration() const;

 private:
  // Note: It is preferred to pass |audio_data| by value here.
  WavAudioHandler(base::StringPiece audio_data,
                  uint16_t num_channels,
                  uint32_t sample_rate,
                  uint16_t bits_per_sample,
                  AudioFormat audio_format);

  // Data part of the |wav_data_|.
  const base::StringPiece data_;
  const uint16_t num_channels_;
  const uint32_t sample_rate_;
  const uint16_t bits_per_sample_;
  const AudioFormat audio_format_;
  uint32_t total_frames_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WAV_AUDIO_HANDLER_H_
