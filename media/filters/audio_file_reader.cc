// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_file_reader.h"

#include "base/logging.h"
#include "base/time.h"
#include "media/base/audio_bus.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

AudioFileReader::AudioFileReader(FFmpegURLProtocol* protocol)
    : codec_context_(NULL),
      stream_index_(0),
      protocol_(protocol) {
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

  // Add one microsecond to avoid rounding-down errors which can occur when
  // |duration| has been calculated from an exact number of sample-frames.
  // One microsecond is much less than the time of a single sample-frame
  // at any real-world sample-rate.
  return ConvertFromTimeBase(
      av_time_base, glue_->format_context()->duration + 1);
}

int64 AudioFileReader::number_of_frames() const {
  return static_cast<int64>(duration().InSecondsF() * sample_rate());
}

bool AudioFileReader::Open() {
  glue_.reset(new FFmpegGlue(protocol_));
  AVFormatContext* format_context = glue_->format_context();

  // Open FFmpeg AVFormatContext.
  if (!glue_->OpenContext()) {
    DLOG(WARNING) << "AudioFileReader::Open() : error in avformat_open_input()";
    return false;
  }

  // Get the codec context.
  codec_context_ = NULL;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVCodecContext* c = format_context->streams[i]->codec;
    if (c->codec_type == AVMEDIA_TYPE_AUDIO) {
      codec_context_ = c;
      stream_index_ = i;
      break;
    }
  }

  // Get the codec.
  if (!codec_context_)
    return false;

  int result = avformat_find_stream_info(format_context, NULL);
  if (result < 0) {
    DLOG(WARNING)
        << "AudioFileReader::Open() : error in avformat_find_stream_info()";
    return false;
  }

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
}

int AudioFileReader::Read(AudioBus* audio_bus) {
  DCHECK(glue_.get() && codec_context_) <<
      "AudioFileReader::Read() : reader is not opened!";

  DCHECK_EQ(audio_bus->channels(), channels());
  if (audio_bus->channels() != channels())
    return 0;

  size_t bytes_per_sample = av_get_bytes_per_sample(codec_context_->sample_fmt);

  // Holds decoded audio.
  scoped_ptr_malloc<AVFrame, ScopedPtrAVFree> av_frame(avcodec_alloc_frame());

  // Read until we hit EOF or we've read the requested number of frames.
  AVPacket packet;
  int current_frame = 0;
  bool continue_decoding = true;

  while (current_frame < audio_bus->frames() && continue_decoding &&
         av_read_frame(glue_->format_context(), &packet) >= 0 &&
         av_dup_packet(&packet) >= 0) {
    // Skip packets from other streams.
    if (packet.stream_index != stream_index_) {
      av_free_packet(&packet);
      continue;
    }

    // Make a shallow copy of packet so we can slide packet.data as frames are
    // decoded from the packet; otherwise av_free_packet() will corrupt memory.
    AVPacket packet_temp = packet;
    do {
      avcodec_get_frame_defaults(av_frame.get());
      int frame_decoded = 0;
      int result = avcodec_decode_audio4(
          codec_context_, av_frame.get(), &frame_decoded, &packet_temp);

      if (result < 0) {
        DLOG(WARNING)
            << "AudioFileReader::Read() : error in avcodec_decode_audio4() -"
            << result;
        continue_decoding = false;
        break;
      }

      // Update packet size and data pointer in case we need to call the decoder
      // with the remaining bytes from this packet.
      packet_temp.size -= result;
      packet_temp.data += result;

      if (!frame_decoded)
        continue;

      // Determine the number of sample-frames we just decoded.  Check overflow.
      int frames_read = av_frame->nb_samples;
      if (frames_read < 0) {
        continue_decoding = false;
        break;
      }

      // Truncate, if necessary, if the destination isn't big enough.
      if (current_frame + frames_read > audio_bus->frames())
        frames_read = audio_bus->frames() - current_frame;

      // Deinterleave each channel and convert to 32bit floating-point
      // with nominal range -1.0 -> +1.0.
      audio_bus->FromInterleavedPartial(
          av_frame->data[0], current_frame, frames_read, bytes_per_sample);

      current_frame += frames_read;
    } while (packet_temp.size > 0);
    av_free_packet(&packet);
  }

  // Zero any remaining frames.
  audio_bus->ZeroFramesPartial(
      current_frame, audio_bus->frames() - current_frame);

  // Returns the actual number of sample-frames decoded.
  // Ideally this represents the "true" exact length of the file.
  return current_frame;
}

}  // namespace media
