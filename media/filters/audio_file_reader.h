// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_AUDIO_FILE_READER_H_
#define MEDIA_FILTERS_AUDIO_FILE_READER_H_

#include <vector>
#include "media/filters/ffmpeg_glue.h"

struct AVCodecContext;
struct AVFormatContext;

namespace base { class TimeDelta; }

namespace media {

class AudioBus;
class FFmpegURLProtocol;

class MEDIA_EXPORT AudioFileReader {
 public:
  // Audio file data will be read using the given protocol.
  // The AudioFileReader does not take ownership of |protocol| and
  // simply maintains a weak reference to it.
  explicit AudioFileReader(FFmpegURLProtocol* protocol);
  virtual ~AudioFileReader();

  // Open() reads the audio data format so that the sample_rate(),
  // channels(), duration(), and number_of_frames() methods can be called.
  // It returns |true| on success.
  bool Open();
  void Close();

  // After a call to Open(), attempts to fully fill |audio_bus| with decoded
  // audio data.  Any unfilled frames will be zeroed out.
  // |audio_data| must be of the same size as channels().
  // The audio data will be decoded as floating-point linear PCM with
  // a nominal range of -1.0 -> +1.0.
  // Returns |true| on success.
  bool Read(AudioBus* audio_bus);

  // These methods can be called once Open() has been called.
  int channels() const;
  int sample_rate() const;
  base::TimeDelta duration() const;
  int64 number_of_frames() const;

 private:
  FFmpegURLProtocol* protocol_;
  AVFormatContext* format_context_;
  AVCodecContext* codec_context_;
  int stream_index_;

  DISALLOW_COPY_AND_ASSIGN(AudioFileReader);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_FILE_READER_H_
