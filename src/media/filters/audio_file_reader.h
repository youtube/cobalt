// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_AUDIO_FILE_READER_H_
#define MEDIA_FILTERS_AUDIO_FILE_READER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"

struct AVCodecContext;

namespace base { class TimeDelta; }

namespace media {

class AudioBus;
class FFmpegGlue;
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
  // Returns the number of sample-frames actually read which will always be
  // <= audio_bus->frames()
  int Read(AudioBus* audio_bus);

  // These methods can be called once Open() has been called.
  int channels() const;
  int sample_rate() const;

  // Please note that duration() and number_of_frames() attempt to be accurate,
  // but are only estimates.  For some encoded formats, the actual duration
  // of the file can only be determined once all the file data has been read.
  // The Read() method returns the actual number of sample-frames it has read.
  base::TimeDelta duration() const;
  int64 number_of_frames() const;

 private:
  scoped_ptr<FFmpegGlue> glue_;
  AVCodecContext* codec_context_;
  int stream_index_;
  FFmpegURLProtocol* protocol_;

  DISALLOW_COPY_AND_ASSIGN(AudioFileReader);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_FILE_READER_H_
