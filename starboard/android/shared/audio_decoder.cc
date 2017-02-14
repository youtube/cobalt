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

#include "starboard/android/shared/audio_decoder.h"

#include "starboard/audio_sink.h"
#include "starboard/log.h"
#include "starboard/memory.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

const int64_t kSecondInMicroseconds = 1 * 1000 * 1000;
const int64_t kDequeueTimeout = kSecondInMicroseconds;
const char kMimeTypeAac[] = "audio/mp4a-latm";
const char kCodecSpecificDataKey[] = "csd-0";

SbMediaTime ConvertFromMicroseconds(const int64_t time_in_microseconds) {
  return time_in_microseconds * kSbMediaTimeSecond / kSecondInMicroseconds;
}

int64_t ConvertToMicroseconds(const SbMediaTime media_time) {
  return media_time * kSecondInMicroseconds / kSbMediaTimeSecond;
}

SbMediaAudioSampleType GetSupportedSampleType() {
  SB_DCHECK(
      SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeInt16));
  return kSbMediaAudioSampleTypeInt16;
}

}  // namespace

AudioDecoder::AudioDecoder(SbMediaAudioCodec audio_codec,
                           const SbMediaAudioHeader& audio_header)
    : stream_ended_(false),
      audio_header_(audio_header),
      sample_type_(GetSupportedSampleType()),
      media_codec_(NULL),
      media_format_(NULL) {
  SB_DCHECK(audio_codec == kSbMediaAudioCodecAac);

  InitializeCodec();
}

AudioDecoder::~AudioDecoder() {
  TeardownCodec();
}

void AudioDecoder::Decode(const InputBuffer& input_buffer) {
  // See "Synchronous Processing using Buffers" at
  // https://developer.android.com/reference/android/media/MediaCodec.html for
  // details regarding logic of this function.
  SB_CHECK(media_codec_);

  if (stream_ended_) {
    SB_LOG(INFO) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }

  const uint8_t* data = input_buffer.data();
  size_t input_size = input_buffer.size();

  ssize_t index = AMediaCodec_dequeueInputBuffer(media_codec_, kDequeueTimeout);
  if (index < 0) {
    SB_LOG(ERROR) << "|AMediaCodec_dequeueInputBuffer| failed with status: "
                  << index;
    return;
  }

  size_t buffer_size;
  uint8_t* mc_input_buffer =
      AMediaCodec_getInputBuffer(media_codec_, index, &buffer_size);
  SB_DCHECK(mc_input_buffer);
  if (input_size > buffer_size) {
    SB_LOG(ERROR) << "Media codec input buffer too small to write to.";
    return;
  }
  SbMemoryCopy(mc_input_buffer, data, input_size);

  int64_t pts_us = ConvertToMicroseconds(input_buffer.pts());
  media_status_t status = AMediaCodec_queueInputBuffer(media_codec_, index, 0,
                                                       input_size, pts_us, 0);
  if (status != AMEDIA_OK) {
    SB_LOG(ERROR) << "|AMediaCodec_queueInputBuffer| failed with status: "
                  << status;
    return;
  }

  AMediaCodecBufferInfo info;
  ssize_t out_index =
      AMediaCodec_dequeueOutputBuffer(media_codec_, &info, kDequeueTimeout);
  if (out_index < 0) {
    SB_LOG(INFO) << "|AMediaCodec_dequeueOutputBuffer| failed with status: "
                 << out_index << ", at pts: " << input_buffer.pts();
    return;
  }

  buffer_size = -1;
  uint8_t* output_buffer =
      AMediaCodec_getOutputBuffer(media_codec_, out_index, &buffer_size);
  // |size| is the capacity of the entire allocated buffer, not the amount
  // filled with relevant decoded data. We must use |info.size| for the
  // actual size filled by the decoder. See:
  // https://android.googlesource.com/platform/frameworks/av/+/47734c/media/ndk/NdkMediaCodec.cpp#288
  SB_DCHECK(info.size <= buffer_size);

  if (output_buffer && buffer_size > 0) {
    scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
        ConvertFromMicroseconds(info.presentationTimeUs), info.size);
    SbMemoryCopy(decoded_audio->buffer(), output_buffer + info.offset,
                 info.size);
    decoded_audios_.push(decoded_audio);
  }

  status = AMediaCodec_releaseOutputBuffer(media_codec_, out_index, false);
  if (status != AMEDIA_OK) {
    SB_LOG(ERROR) << "|AMediaCodec_releaseOutputBuffer| failed with status: "
                  << status;
    return;
  }
}

void AudioDecoder::WriteEndOfStream() {
  // AAC has no dependent frames so we needn't flush the decoder.  Set the flag
  // to ensure that Decode() is not called when the stream is ended.
  stream_ended_ = true;
  // Put EOS into the queue.
  decoded_audios_.push(new DecodedAudio);
}

scoped_refptr<AudioDecoder::DecodedAudio> AudioDecoder::Read() {
  scoped_refptr<DecodedAudio> result;
  if (!decoded_audios_.empty()) {
    result = decoded_audios_.front();
    decoded_audios_.pop();
  }
  return result;
}

void AudioDecoder::Reset() {
  stream_ended_ = false;
  media_status_t status = AMediaCodec_flush(media_codec_);
  if (status != AMEDIA_OK) {
    SB_LOG(ERROR) << "|AMediaCodec_flush| failed with status: " << status;
  }

  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }
}

void AudioDecoder::InitializeCodec() {
  const char* mime_type = kMimeTypeAac;
  media_codec_ = AMediaCodec_createDecoderByType(mime_type);
  if (!media_codec_) {
    SB_LOG(ERROR) << "Failed to create MediaCodec for mime_type: " << mime_type;
    return;
  }

  media_format_ = AMediaFormat_new();
  AMediaFormat_setString(media_format_, AMEDIAFORMAT_KEY_MIME, mime_type);
  AMediaFormat_setInt32(media_format_, AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                        audio_header_.number_of_channels);
  AMediaFormat_setInt32(media_format_, AMEDIAFORMAT_KEY_SAMPLE_RATE,
                        audio_header_.samples_per_second);
  AMediaFormat_setInt32(media_format_, AMEDIAFORMAT_KEY_IS_ADTS, 1);
  AMediaFormat_setBuffer(media_format_, kCodecSpecificDataKey,
                         audio_header_.audio_specific_config,
                         audio_header_.audio_specific_config_size);
  media_status_t status =
      AMediaCodec_configure(media_codec_, media_format_, NULL, NULL, 0);
  if (status != AMEDIA_OK) {
    SB_LOG(ERROR) << "Failed to configure |media_codec_|, status: " << status;
    return;
  }
  status = AMediaCodec_start(media_codec_);
  if (status != AMEDIA_OK) {
    SB_LOG(ERROR) << "Failed to start |media_codec_|, status: " << status;
    return;
  }
}

void AudioDecoder::TeardownCodec() {
  if (media_codec_) {
    AMediaCodec_delete(media_codec_);
  }
  if (media_format_) {
    AMediaFormat_delete(media_format_);
  }
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// static
AudioDecoder* AudioDecoder::Create(const Options& options) {
  ::starboard::android::shared::AudioDecoder* decoder =
      new ::starboard::android::shared::AudioDecoder(options.audio_codec,
                                                     options.audio_header);
  if (!decoder->is_valid()) {
    delete decoder;
    return NULL;
  }
  return decoder;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
