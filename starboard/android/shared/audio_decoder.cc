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

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/audio_sink.h"
#include "starboard/log.h"
#include "starboard/memory.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

const jlong kDequeueTimeout = kSecondInMicroseconds;

SbMediaAudioSampleType GetSupportedSampleType() {
  SB_DCHECK(
      SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeInt16));
  return kSbMediaAudioSampleTypeInt16;
}

void* IncrementPointerByBytes(void* pointer, int offset) {
  return static_cast<uint8_t*>(pointer) + offset;
}

}  // namespace

AudioDecoder::AudioDecoder(SbMediaAudioCodec audio_codec,
                           const SbMediaAudioHeader& audio_header)
    : stream_ended_(false),
      audio_header_(audio_header),
      sample_type_(GetSupportedSampleType()) {
  SB_DCHECK(audio_codec == kSbMediaAudioCodecAac);
  if (!InitializeCodec()) {
    SB_LOG(ERROR) << "Failed to initialize audio decoder.";
    TeardownCodec();
  }
}

AudioDecoder::~AudioDecoder() {
  TeardownCodec();
}

void AudioDecoder::Decode(const InputBuffer& input_buffer) {
  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }

  ProcessOneInputBuffer(input_buffer);
  ProcessOneOutputBuffer();
}

void AudioDecoder::WriteEndOfStream() {
  // AAC has no dependent frames so we needn't flush the decoder.  Set the flag
  // to ensure that Decode() is not called when the stream is ended.
  stream_ended_ = true;
  // Put EOS into the queue.
  decoded_audios_.push(new DecodedAudio());
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
  jint status = media_codec_bridge_->Flush();
  if (status != MEDIA_CODEC_OK) {
    SB_LOG(ERROR) << "|flush| failed with status: " << status;
  }

  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }
}

bool AudioDecoder::InitializeCodec() {
  media_codec_bridge_ = MediaCodecBridge::CreateAudioMediaCodecBridge(
      kMimeTypeAac, audio_header_);
  if (!media_codec_bridge_) {
    return false;
  }

  if (audio_header_.audio_specific_config_size > 0 &&
      !ProcessOneInputRawData(audio_header_.audio_specific_config,
                              audio_header_.audio_specific_config_size, 0,
                              BUFFER_FLAG_CODEC_CONFIG)) {
    return false;
  }

  return true;
}

void AudioDecoder::TeardownCodec() {
  media_codec_bridge_.reset();
}

bool AudioDecoder::ProcessOneInputBuffer(const InputBuffer& input_buffer) {
  return ProcessOneInputRawData(input_buffer.data(), input_buffer.size(),
                                input_buffer.pts(), 0);
}

bool AudioDecoder::ProcessOneInputRawData(const void* data,
                                          int size,
                                          SbMediaTime pts,
                                          jint flags) {
  SB_CHECK(media_codec_bridge_);
  DequeueInputResult dequeue_input_result =
      media_codec_bridge_->DequeueInputBuffer(kDequeueTimeout);
  if (dequeue_input_result.index < 0) {
    SB_LOG(ERROR) << "|dequeueInputBuffer| failed with status: "
                  << dequeue_input_result.status;
    return false;
  }
  ScopedJavaByteBuffer byte_buffer(
      media_codec_bridge_->GetInputBuffer(dequeue_input_result.index));
  if (byte_buffer.IsNull() || byte_buffer.capacity() < size) {
    SB_LOG(ERROR) << "Unable to write to MediaCodec input buffer.";
    return false;
  }

  byte_buffer.CopyInto(data, size);

  jint status = media_codec_bridge_->QueueInputBuffer(
      dequeue_input_result.index, /*offset=*/0, size,
      ConvertSbMediaTimeToMicroseconds(pts), flags);
  if (status != MEDIA_CODEC_OK) {
    SB_LOG(ERROR) << "|queueInputBuffer| failed with status: " << status;
    return false;
  }

  return true;
}

bool AudioDecoder::ProcessOneOutputBuffer() {
  SB_CHECK(media_codec_bridge_);
  DequeueOutputResult dequeue_output_result =
      media_codec_bridge_->DequeueOutputBuffer(kDequeueTimeout);
  if (dequeue_output_result.index < 0) {
    SB_LOG(ERROR) << "|dequeueOutputBuffer| failed with status: "
                  << dequeue_output_result.status;
    return false;
  }

  ScopedJavaByteBuffer byte_buffer(
      media_codec_bridge_->GetOutputBuffer(dequeue_output_result.index));
  SB_DCHECK(!byte_buffer.IsNull());

  if (dequeue_output_result.num_bytes > 0) {
    scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
        ConvertMicrosecondsToSbMediaTime(
            dequeue_output_result.presentation_time_microseconds),
        dequeue_output_result.num_bytes);
    SbMemoryCopy(decoded_audio->buffer(),
                 IncrementPointerByBytes(byte_buffer.address(),
                                         dequeue_output_result.offset),
                 dequeue_output_result.num_bytes);
    decoded_audios_.push(decoded_audio);
  }

  media_codec_bridge_->ReleaseOutputBuffer(dequeue_output_result.index, false);

  return true;
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
AudioDecoder* AudioDecoder::Create(const Parameters& parameters) {
  ::starboard::android::shared::AudioDecoder* decoder =
      new ::starboard::android::shared::AudioDecoder(parameters.audio_codec,
                                                     parameters.audio_header);
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
