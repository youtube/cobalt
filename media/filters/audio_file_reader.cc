// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_file_reader.h"

#include <string>
#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/time.h"
#include "media/audio/audio_util.h"
#include "media/base/filters.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

AudioFileReader::AudioFileReader(FFmpegURLProtocol* protocol)
    : protocol_(protocol),
      format_context_(NULL),
      codec_context_(NULL),
      codec_(NULL) {
}

AudioFileReader::~AudioFileReader() {
  Close();
}

int AudioFileReader::channels() const {
  return codec_context_->channels;
}

int AudioFileReader::sample_rate() const {
  return codec_context_->sample_rate;
}

base::TimeDelta AudioFileReader::duration() const {
  const AVRational av_time_base = {1, AV_TIME_BASE};
  return ConvertTimestamp(av_time_base, format_context_->duration);
}

int64 AudioFileReader::number_of_frames() const {
  return static_cast<int64>(duration().InSecondsF() * sample_rate());
}

bool AudioFileReader::Open() {
  // Add our data reader to the protocol list and get our unique key.
  std::string key = FFmpegGlue::GetInstance()->AddProtocol(protocol_);

  // Open FFmpeg AVFormatContext.
  DCHECK(!format_context_);
  AVFormatContext* context = NULL;

  int result = av_open_input_file(&context, key.c_str(), NULL, 0, NULL);

  // Remove our data reader from protocol list since av_open_input_file() setup
  // the AVFormatContext with the data reader.
  FFmpegGlue::GetInstance()->RemoveProtocol(protocol_);

  if (result) {
    DLOG(WARNING)
        << "AudioFileReader::Open() : error in av_open_input_file() -"
        << " result: " << result;
    return false;
  }

  DCHECK(context);
  format_context_ = context;

  // Get the codec context.
  codec_context_ = NULL;
  for (size_t i = 0; i < format_context_->nb_streams; ++i) {
    AVCodecContext* c = format_context_->streams[i]->codec;
    if (c->codec_type == CODEC_TYPE_AUDIO) {
      codec_context_ = c;
      break;
    }
  }

  // Get the codec.
  if (!codec_context_)
    return false;

  av_find_stream_info(format_context_);
  codec_ = avcodec_find_decoder(codec_context_->codec_id);
  if (codec_) {
    if ((result = avcodec_open(codec_context_, codec_)) < 0) {
      DLOG(WARNING) << "AudioFileReader::Open() : could not open codec -"
          << " result: " << result;
      return false;
    }

    result = av_seek_frame(format_context_, 0, 0, 0);
  }

  return true;
}

void AudioFileReader::Close() {
  if (codec_context_ && codec_)
    avcodec_close(codec_context_);

  codec_context_ = NULL;
  codec_ = NULL;

  if (format_context_) {
    av_close_input_file(format_context_);
    format_context_ = NULL;
  }
}

bool AudioFileReader::Read(const std::vector<float*>& audio_data,
                           size_t number_of_frames) {
  size_t channels = this->channels();
  DCHECK_EQ(audio_data.size(), channels);
  if (audio_data.size() != channels)
    return false;

  DCHECK(format_context_ && codec_context_);
  if (!format_context_ || !codec_context_) {
    DLOG(WARNING) << "AudioFileReader::Read() : reader is not opened!";
    return false;
  }

  scoped_ptr_malloc<int16, ScopedPtrAVFree> output_buffer(
      static_cast<int16*>(av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE)));

  // Read until we hit EOF or we've read the requested number of frames.
  AVPacket avpkt;
  av_init_packet(&avpkt);

  int result = 0;
  size_t current_frame = 0;

  while (current_frame < number_of_frames &&
      (result = av_read_frame(format_context_, &avpkt)) >= 0) {
    int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    result = avcodec_decode_audio3(codec_context_,
                                   output_buffer.get(),
                                   &out_size,
                                   &avpkt);

    if (result < 0) {
      DLOG(WARNING)
          << "AudioFileReader::Read() : error in avcodec_decode_audio3() -"
          << result;
      return false;
    }

    // Determine the number of sample-frames we just decoded.
    size_t bytes_per_sample =
        av_get_bits_per_sample_fmt(codec_context_->sample_fmt) >> 3;
    size_t frames_read = out_size / (channels * bytes_per_sample);

    // Truncate, if necessary, if the destination isn't big enough.
    if (current_frame + frames_read > number_of_frames)
      frames_read = number_of_frames - current_frame;

    // Deinterleave each channel and convert to 32bit floating-point
    // with nominal range -1.0 -> +1.0.
    for (size_t channel_index = 0; channel_index < channels;
         ++channel_index) {
      if (!DeinterleaveAudioChannel(output_buffer.get(),
                                    audio_data[channel_index] + current_frame,
                                    channels,
                                    channel_index,
                                    bytes_per_sample,
                                    frames_read)) {
        DLOG(WARNING)
            << "AudioFileReader::Read() : Unsupported sample format : "
            << codec_context_->sample_fmt
            << " codec_->id : " << codec_->id;
        return false;
      }
    }

    current_frame += frames_read;
  }

  return true;
}

InMemoryDataReader::InMemoryDataReader(const char* data, int64 size)
    : data_(data),
      size_(size),
      position_(0) {
}

int InMemoryDataReader::Read(int size, uint8* data) {
  if (size < 0)
    return -1;

  int available_bytes = static_cast<int>(size_ - position_);
  if (size > available_bytes)
    size = available_bytes;

  memcpy(data, data_ + position_, size);
  position_ += size;
  return size;
}

bool InMemoryDataReader::GetPosition(int64* position_out) {
  if (position_out)
    *position_out = position_;
  return true;
}

bool InMemoryDataReader::SetPosition(int64 position) {
  if (position >= size_)
    return false;
  position_ = position;
  return true;
}

bool InMemoryDataReader::GetSize(int64* size_out) {
  if (size_out)
    *size_out = size_;
  return true;
}

bool InMemoryDataReader::IsStreaming() {
  return false;
}

}  // namespace media
