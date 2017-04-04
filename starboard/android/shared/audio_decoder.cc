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

const jlong kDequeueTimeout = 0;
const SbTimeMonotonic kUpdateInterval = 10 * kSbTimeMillisecond;

const jint kNoOffset = 0;
const jlong kNoPts = 0;
const jint kNoSize = 0;
const jint kNoBufferFlags = 0;

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
                           const SbMediaAudioHeader& audio_header,
                           JobQueue* job_queue,
                           SbDrmSystem drm_system)
    : stream_ended_(false),
      audio_codec_(audio_codec),
      audio_header_(audio_header),
      job_queue_(job_queue),
      drm_system_(static_cast<DrmSystem*>(drm_system)),
      sample_type_(GetSupportedSampleType()) {
  SB_DCHECK(job_queue_);
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  if (!InitializeCodec()) {
    SB_LOG(ERROR) << "Failed to initialize audio decoder.";
    TeardownCodec();
    return;
  }

  update_closure_ =
      ::starboard::shared::starboard::player::Bind(&AudioDecoder::Update, this);
  job_queue_->Schedule(update_closure_, kUpdateInterval);
}

AudioDecoder::~AudioDecoder() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  TeardownCodec();
  job_queue_->Remove(update_closure_);
}

void AudioDecoder::Decode(const InputBuffer& input_buffer) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }

  pending_work_.push(Event(input_buffer));
}

void AudioDecoder::WriteEndOfStream() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  pending_work_.push(Event(Event::kWriteEndOfStream));
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

  while (!pending_work_.empty()) {
    pending_work_.pop();
  }

  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }

  TeardownCodec();
  if (!InitializeCodec()) {
    // TODO: Communicate this failure to our clients somehow.
    SB_LOG(ERROR) << "Failed to initialize codec after reset.";
  }
}

bool AudioDecoder::InitializeCodec() {
  jobject j_media_crypto = drm_system_ ? drm_system_->GetMediaCrypto() : NULL;
  SB_DCHECK(!drm_system_ || j_media_crypto);
  media_codec_bridge_ = MediaCodecBridge::CreateAudioMediaCodecBridge(
      audio_codec_, audio_header_, j_media_crypto);
  if (!media_codec_bridge_) {
    return false;
  }
  if (audio_header_.audio_specific_config_size > 0) {
    pending_work_.push(Event(Event::kWriteCodecConfig));
  }
  return true;
}

void AudioDecoder::TeardownCodec() {
  media_codec_bridge_.reset();
}

bool AudioDecoder::ProcessOneInputBuffer() {
  if (pending_work_.empty()) {
    return false;
  }

  const Event& event = pending_work_.front();
  const void* data = NULL;
  int size = 0;
  if (event.type == Event::kWriteCodecConfig) {
    data = audio_header_.audio_specific_config;
    size = audio_header_.audio_specific_config_size;
  } else if (event.type == Event::kWriteInputBuffer) {
    const InputBuffer& input_buffer = event.input_buffer;
    data = input_buffer.data();
    size = input_buffer.size();
  } else if (event.type == Event::kWriteEndOfStream) {
    data = NULL;
    size = 0;
  } else {
    SB_NOTREACHED();
  }

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

  if (data) {
    byte_buffer.CopyInto(data, size);
  }

  jint status;
  if (event.type == Event::kWriteCodecConfig) {
    status = media_codec_bridge_->QueueInputBuffer(dequeue_input_result.index,
                                                   kNoOffset, size, kNoPts,
                                                   BUFFER_FLAG_CODEC_CONFIG);
  } else if (event.type == Event::kWriteInputBuffer) {
    jlong pts_us = ConvertSbMediaTimeToMicroseconds(event.input_buffer.pts());
    if (drm_system_ && event.input_buffer.drm_info()) {
      status = media_codec_bridge_->QueueSecureInputBuffer(
          dequeue_input_result.index, kNoOffset, *event.input_buffer.drm_info(),
          pts_us);

      if (status == MEDIA_CODEC_NO_KEY) {
        SB_DLOG(INFO) << "|queueSecureInputBuffer| failed with status: "
                         "MEDIA_CODEC_NO_KEY, will try again later.";
        // We will try this input buffer again later after |drm_system_| tries
        // to take care of our key.  We still need to return this input buffer
        // to media codec, though.
        status = media_codec_bridge_->QueueInputBuffer(
            dequeue_input_result.index, kNoOffset, kNoSize, pts_us,
            kNoBufferFlags);
        if (status != MEDIA_CODEC_OK) {
          SB_LOG(ERROR) << "|queueInputBuffer| failed with status: " << status;
        }
        return false;
      }

    } else {
      status = media_codec_bridge_->QueueInputBuffer(
          dequeue_input_result.index, kNoOffset, size, pts_us, kNoBufferFlags);
    }
  } else if (event.type == Event::kWriteEndOfStream) {
    status = media_codec_bridge_->QueueInputBuffer(dequeue_input_result.index,
                                                   kNoOffset, size, kNoPts,
                                                   BUFFER_FLAG_END_OF_STREAM);
  } else {
    SB_NOTREACHED();
  }

  if (status != MEDIA_CODEC_OK) {
    SB_LOG(ERROR) << "|queue(Secure)?InputBuffer| failed with status: "
                  << status;
    return false;
  }

  pending_work_.pop();
  return true;
}

bool AudioDecoder::ProcessOneOutputBuffer() {
  SB_CHECK(media_codec_bridge_);
  DequeueOutputResult dequeue_output_result =
      media_codec_bridge_->DequeueOutputBuffer(kDequeueTimeout);

  if (dequeue_output_result.flags & BUFFER_FLAG_END_OF_STREAM) {
    if (dequeue_output_result.index >= 0) {
      media_codec_bridge_->ReleaseOutputBuffer(dequeue_output_result.index,
                                               false);
    }
    stream_ended_ = true;
    decoded_audios_.push(new DecodedAudio());
    return false;
  }

  if (dequeue_output_result.index < 0) {
    if (dequeue_output_result.status !=
        MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER) {
      SB_LOG(ERROR) << "|dequeueOutputBuffer| failed with status: "
                    << dequeue_output_result.status;
    }
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

void AudioDecoder::Update() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  bool did_input = true;
  bool did_output = true;
  while (did_input || did_output) {
    did_input = ProcessOneInputBuffer();
    did_output = ProcessOneOutputBuffer();
  }
  job_queue_->Schedule(update_closure_, kUpdateInterval);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
