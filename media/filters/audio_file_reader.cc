// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_file_reader.h"

#include <string>
#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/time.h"
#include "media/base/audio_bus.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

AudioFileReader::AudioFileReader(FFmpegURLProtocol* protocol)
    : protocol_(protocol),
      format_context_(NULL),
      codec_context_(NULL) {
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
  return ConvertFromTimeBase(av_time_base, format_context_->duration);
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

  int result = avformat_open_input(&context, key.c_str(), NULL, NULL);

  // Remove our data reader from protocol list since avformat_open_input() setup
  // the AVFormatContext with the data reader.
  FFmpegGlue::GetInstance()->RemoveProtocol(protocol_);

  if (result) {
    DLOG(WARNING)
        << "AudioFileReader::Open() : error in avformat_open_input() -"
        << " result: " << result;
    return false;
  }

  DCHECK(context);
  format_context_ = context;

  // Get the codec context.
  codec_context_ = NULL;
  for (size_t i = 0; i < format_context_->nb_streams; ++i) {
    AVCodecContext* c = format_context_->streams[i]->codec;
    if (c->codec_type == AVMEDIA_TYPE_AUDIO) {
      codec_context_ = c;
      break;
    }
  }

  // Get the codec.
  if (!codec_context_)
    return false;

  avformat_find_stream_info(format_context_, NULL);
  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (codec) {
    if ((result = avcodec_open2(codec_context_, codec, NULL)) < 0) {
      DLOG(WARNING) << "AudioFileReader::Open() : could not open codec -"
          << " result: " << result;
      return false;
    }
  } else {
    DLOG(WARNING) << "AudioFileReader::Open() : could not find codec -"
        << " result: " << result;
    return false;
  }

  return true;
}

void AudioFileReader::Close() {
  if (codec_context_) {
    avcodec_close(codec_context_);
    codec_context_ = NULL;
  }

  if (format_context_) {
    avformat_close_input(&format_context_);
    format_context_ = NULL;
  }
}

bool AudioFileReader::Read(AudioBus* audio_bus) {
  DCHECK(format_context_ && codec_context_) <<
      "AudioFileReader::Read() : reader is not opened!";

  DCHECK_EQ(audio_bus->channels(), channels());
  if (audio_bus->channels() != channels())
    return false;

  size_t bytes_per_sample = av_get_bytes_per_sample(codec_context_->sample_fmt);

  // Holds decoded audio.
  scoped_ptr_malloc<AVFrame, ScopedPtrAVFree> av_frame(avcodec_alloc_frame());

  // Read until we hit EOF or we've read the requested number of frames.
  AVPacket packet;
  int result = 0;
  int current_frame = 0;

  while (current_frame < audio_bus->frames() &&
         (result = av_read_frame(format_context_, &packet)) >= 0) {
    avcodec_get_frame_defaults(av_frame.get());
    int frame_decoded = 0;
    int result = avcodec_decode_audio4(
        codec_context_, av_frame.get(), &frame_decoded, &packet);
    av_free_packet(&packet);

    if (result < 0) {
      DLOG(WARNING)
          << "AudioFileReader::Read() : error in avcodec_decode_audio3() -"
          << result;
      break;
    }

    if (!frame_decoded)
      continue;

    // Determine the number of sample-frames we just decoded.  Check overflow.
    int frames_read = av_frame->nb_samples;
    if (frames_read < 0)
      break;

    // Truncate, if necessary, if the destination isn't big enough.
    if (current_frame + frames_read > audio_bus->frames())
      frames_read = audio_bus->frames() - current_frame;

    // Deinterleave each channel and convert to 32bit floating-point
    // with nominal range -1.0 -> +1.0.
    audio_bus->FromInterleavedPartial(
        av_frame->data[0], current_frame, frames_read, bytes_per_sample);

    current_frame += frames_read;
  }

  // Zero any remaining frames.
  audio_bus->ZeroFramesPartial(
      current_frame, audio_bus->frames() - current_frame);

  // Fail if nothing has been decoded, otherwise return partial data.
  return current_frame > 0;
}

}  // namespace media
