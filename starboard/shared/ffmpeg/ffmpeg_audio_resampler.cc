// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/shared/ffmpeg/ffmpeg_audio_resampler.h"

#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

namespace {

const int kMaxChannels = 8;
const int kMaxCachedSamples = 65536;

int GetChannelLayoutFromChannels(int channels) {
  if (channels == 1) {
    return AV_CH_LAYOUT_MONO;
  }
  if (channels == 2) {
    return AV_CH_LAYOUT_STEREO;
  }
  if (channels == 6) {
    return AV_CH_LAYOUT_5POINT1;
  }
  if (channels == 8) {
    return AV_CH_LAYOUT_7POINT1;
  }
  SB_NOTREACHED() << "Unsupported channel count: " << channels;
  return AV_CH_LAYOUT_STEREO;
}

int GetSampleFormatBySampleTypeAndStorageType(
    SbMediaAudioSampleType sample_type,
    SbMediaAudioFrameStorageType storage_type) {
  if (sample_type == kSbMediaAudioSampleTypeInt16 &&
      storage_type == kSbMediaAudioFrameStorageTypeInterleaved) {
    return AV_SAMPLE_FMT_S16;
  }
  if (sample_type == kSbMediaAudioSampleTypeFloat32 &&
      storage_type == kSbMediaAudioFrameStorageTypeInterleaved) {
    return AV_SAMPLE_FMT_FLT;
  }
  if (sample_type == kSbMediaAudioSampleTypeInt16 &&
      storage_type == kSbMediaAudioFrameStorageTypePlanar) {
    return AV_SAMPLE_FMT_S16P;
  }
  if (sample_type == kSbMediaAudioSampleTypeFloat32 &&
      storage_type == kSbMediaAudioFrameStorageTypePlanar) {
    return AV_SAMPLE_FMT_FLTP;
  }
  SB_NOTREACHED() << "Unsupported sample type (" << sample_type << ") and "
                  << " storage type (" << storage_type << ") combination";
  return AV_SAMPLE_FMT_FLT;
}

}  // namespace

AudioResampler::AudioResampler(
    SbMediaAudioSampleType source_sample_type,
    SbMediaAudioFrameStorageType source_storage_type,
    int source_sample_rate,
    SbMediaAudioSampleType destination_sample_type,
    SbMediaAudioFrameStorageType destination_storage_type,
    int destination_sample_rate,
    int channels)
    : source_sample_type_(source_sample_type),
      source_storage_type_(source_storage_type),
      destination_sample_type_(destination_sample_type),
      destination_storage_type_(destination_storage_type),
      destination_sample_rate_(destination_sample_rate),
      channels_(channels),
      start_pts_(-1),
      samples_returned_(0),
      eos_reached_(false) {
  SB_DCHECK(channels_ <= kMaxChannels);

  context_ = avresample_alloc_context();
  SB_DCHECK(context_ != NULL);

  av_opt_set_int(context_, "in_channel_layout",
                 GetChannelLayoutFromChannels(channels_), 0);
  av_opt_set_int(context_, "out_channel_layout",
                 GetChannelLayoutFromChannels(channels_), 0);
  av_opt_set_int(context_, "in_sample_rate", source_sample_rate, 0);
  av_opt_set_int(context_, "out_sample_rate", destination_sample_rate, 0);
  av_opt_set_int(context_, "in_sample_fmt",
                 GetSampleFormatBySampleTypeAndStorageType(source_sample_type,
                                                           source_storage_type),
                 0);
  av_opt_set_int(context_, "out_sample_fmt",
                 GetSampleFormatBySampleTypeAndStorageType(
                     destination_sample_type, destination_storage_type),
                 0);
  av_opt_set_int(context_, "internal_sample_fmt", AV_SAMPLE_FMT_S16P, 0);

  int result = avresample_open(context_);
  SB_DCHECK(!result);
}

AudioResampler::~AudioResampler() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  avresample_close(context_);
  av_freep(&context_);
}

scoped_refptr<AudioResampler::DecodedAudio> AudioResampler::Resample(
    const scoped_refptr<DecodedAudio>& audio_data) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(audio_data);
  SB_DCHECK(audio_data->sample_type() == source_sample_type_);
  SB_DCHECK(audio_data->storage_type() == source_storage_type_);
  SB_DCHECK(audio_data->channels() == channels_);
  SB_DCHECK(!eos_reached_);

  if (channels_ > kMaxChannels) {
    return new DecodedAudio;
  }

  if (eos_reached_) {
    return new DecodedAudio;
  }

  if (start_pts_ < 0) {
    start_pts_ = audio_data->pts();
  }

  uint8_t* input_buffers[kMaxChannels] = {
      const_cast<uint8_t*>(audio_data->buffer())};
  if (source_storage_type_ == kSbMediaAudioFrameStorageTypePlanar) {
    for (int i = 1; i < channels_; ++i) {
      input_buffers[i] = const_cast<uint8_t*>(
          audio_data->buffer() + audio_data->size() / channels_ * i);
    }
  }

  int result = avresample_convert(context_, NULL, 0, 0, input_buffers, 0,
                                  audio_data->frames());
  SB_DCHECK(result == 0);

  return RetrieveOutput();
}

scoped_refptr<AudioResampler::DecodedAudio> AudioResampler::WriteEndOfStream() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(!eos_reached_);

  eos_reached_ = true;
  int result = avresample_convert(context_, NULL, 0, 0, NULL, 0, 0);
  SB_DCHECK(result == 0);

  return RetrieveOutput();
}

scoped_refptr<AudioResampler::DecodedAudio> AudioResampler::RetrieveOutput() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  int frames_in_buffer = avresample_available(context_);
  SbMediaTime pts = start_pts_ + samples_returned_ * kSbMediaTimeSecond /
                                     destination_sample_rate_;
  int bytes_per_sample =
      starboard::media::GetBytesPerSample(destination_sample_type_);
  scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
      channels_, destination_sample_type_, destination_storage_type_, pts,
      frames_in_buffer * bytes_per_sample * channels_);
  samples_returned_ += frames_in_buffer;

  if (frames_in_buffer > 0) {
    uint8_t* output_buffers[kMaxChannels] = {
        const_cast<uint8_t*>(decoded_audio->buffer())};
    if (destination_storage_type_ == kSbMediaAudioFrameStorageTypePlanar) {
      for (int i = 1; i < channels_; ++i) {
        output_buffers[i] = const_cast<uint8_t*>(
            decoded_audio->buffer() + decoded_audio->size() / channels_ * i);
      }
    }

    int frames_read =
        avresample_read(context_, output_buffers, frames_in_buffer);
    SB_DCHECK(frames_read == frames_in_buffer);
  }

  return decoded_audio;
}

}  // namespace ffmpeg

namespace starboard {
namespace player {
namespace filter {

// static
scoped_ptr<AudioResampler> AudioResampler::Create(
    SbMediaAudioSampleType source_sample_type,
    SbMediaAudioFrameStorageType source_storage_type,
    int source_sample_rate,
    SbMediaAudioSampleType destination_sample_type,
    SbMediaAudioFrameStorageType destination_storage_type,
    int destination_sample_rate,
    int channels) {
  return scoped_ptr<AudioResampler>(new ffmpeg::AudioResampler(
      source_sample_type, source_storage_type, source_sample_rate,
      destination_sample_type, destination_storage_type,
      destination_sample_rate, channels));
}

}  // namespace filter
}  // namespace player
}  // namespace starboard

}  // namespace shared
}  // namespace starboard
