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

#include "starboard/android/shared/media_decoder.h"

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/audio_sink.h"
#include "starboard/log.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

const jlong kDequeueTimeout = 0;

const jint kNoOffset = 0;
const jlong kNoPts = 0;
const jint kNoSize = 0;
const jint kNoBufferFlags = 0;

const char* GetNameForMediaCodecStatus(jint status) {
  switch (status) {
    case MEDIA_CODEC_OK:
      return "MEDIA_CODEC_OK";
    case MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER:
      return "MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER";
    case MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER:
      return "MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER";
    case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
      return "MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED";
    case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED:
      return "MEDIA_CODEC_OUTPUT_FORMAT_CHANGED";
    case MEDIA_CODEC_INPUT_END_OF_STREAM:
      return "MEDIA_CODEC_INPUT_END_OF_STREAM";
    case MEDIA_CODEC_OUTPUT_END_OF_STREAM:
      return "MEDIA_CODEC_OUTPUT_END_OF_STREAM";
    case MEDIA_CODEC_NO_KEY:
      return "MEDIA_CODEC_NO_KEY";
    case MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION:
      return "MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION";
    case MEDIA_CODEC_ABORT:
      return "MEDIA_CODEC_ABORT";
    case MEDIA_CODEC_ERROR:
      return "MEDIA_CODEC_ERROR";
    default:
      SB_NOTREACHED();
      return "MEDIA_CODEC_ERROR_UNKNOWN";
  }
}

}  // namespace

MediaDecoder::MediaDecoder(Host* host,
                           SbMediaAudioCodec audio_codec,
                           const SbMediaAudioHeader& audio_header,
                           SbDrmSystem drm_system)
    : media_type_(kSbMediaTypeAudio),
      host_(host),
      decoder_thread_(kSbThreadInvalid),
      media_codec_bridge_(NULL),
      stream_ended_(false),
      drm_system_(static_cast<DrmSystem*>(drm_system)) {
  SB_DCHECK(host_);

  jobject j_media_crypto = drm_system_ ? drm_system_->GetMediaCrypto() : NULL;
  SB_DCHECK(!drm_system_ || j_media_crypto);
  media_codec_bridge_ = MediaCodecBridge::CreateAudioMediaCodecBridge(
      audio_codec, audio_header, j_media_crypto);
  if (!media_codec_bridge_) {
    SB_LOG(ERROR) << "Failed to create audio media codec bridge.";
    return;
  }
  if (audio_header.audio_specific_config_size > 0) {
    // |audio_header.audio_specific_config| is guaranteed to be outlived the
    // decoder as it is stored in |FilterBasedPlayerWorkerHandler|.
    event_queue_.PushBack(Event(
        static_cast<const int8_t*>(audio_header.audio_specific_config),
        audio_header.audio_specific_config_size));
  }
}

MediaDecoder::MediaDecoder(Host* host,
                           SbMediaVideoCodec video_codec,
                           int width,
                           int height,
                           jobject j_output_surface,
                           SbDrmSystem drm_system,
                           const SbMediaColorMetadata* color_metadata)
    : media_type_(kSbMediaTypeVideo),
      host_(host),
      stream_ended_(false),
      drm_system_(static_cast<DrmSystem*>(drm_system)),
      decoder_thread_(kSbThreadInvalid),
      media_codec_bridge_(NULL) {
  jobject j_media_crypto = drm_system_ ? drm_system_->GetMediaCrypto() : NULL;
  SB_DCHECK(!drm_system_ || j_media_crypto);
  media_codec_bridge_ = MediaCodecBridge::CreateVideoMediaCodecBridge(
      video_codec, width, height, j_output_surface, j_media_crypto,
      color_metadata);
  if (!media_codec_bridge_) {
    SB_LOG(ERROR) << "Failed to create video media codec bridge.";
  }
}

MediaDecoder::~MediaDecoder() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  JoinOnDecoderThread();
}

void MediaDecoder::Initialize(const ErrorCB& error_cb) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  error_cb_ = error_cb;
}

void MediaDecoder::WriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(input_buffer);

  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }

  if (!SbThreadIsValid(decoder_thread_)) {
    decoder_thread_ = SbThreadCreate(
        0, kSbThreadPriorityNormal, kSbThreadNoAffinity, true,
        media_type_ == kSbMediaTypeAudio ? "audio_decoder" : "audio_decoder",
        &MediaDecoder::ThreadEntryPoint, this);
    SB_DCHECK(SbThreadIsValid(decoder_thread_));
  }

  event_queue_.PushBack(Event(input_buffer));
}

void MediaDecoder::WriteEndOfStream() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  stream_ended_ = true;
  event_queue_.PushBack(Event(Event::kWriteEndOfStream));
}

// static
void* MediaDecoder::ThreadEntryPoint(void* context) {
  SB_DCHECK(context);
  MediaDecoder* decoder = static_cast<MediaDecoder*>(context);
  decoder->DecoderThreadFunc();
  return NULL;
}

void MediaDecoder::DecoderThreadFunc() {
  SB_DCHECK(error_cb_);

  // TODO: Replace |pending_work| with a single object instead of using a deque.
  std::deque<Event> pending_work;

  while (!destroying_.load()) {
    if (pending_work.empty()) {
      Event event = event_queue_.PollFront();

      if (event.type == Event::kWriteInputBuffer ||
          event.type == Event::kWriteEndOfStream ||
          event.type == Event::kWriteCodecConfig) {
        pending_work.push_back(event);
      }
    }

    bool did_work = false;
    did_work |= ProcessOneInputBuffer(&pending_work);
    did_work |= host_->Tick(media_codec_bridge_.get());
    did_work |= DequeueAndProcessOutputBuffer();
    did_work |= host_->Tick(media_codec_bridge_.get());

    if (pending_work.empty() && !did_work) {
      SbThreadSleep(kSbTimeMillisecond);
      continue;
    }
  }

  SB_LOG(INFO) << "Destroying decoder thread.";
  host_->OnFlushing();
  jint status = media_codec_bridge_->Flush();
  if (status != MEDIA_CODEC_OK) {
    SB_LOG(ERROR) << "Failed to flush media codec.";
  }
}

void MediaDecoder::JoinOnDecoderThread() {
  if (!SbThreadIsValid(decoder_thread_)) {
    return;
  }
  destroying_.store(true);
  SbThreadJoin(decoder_thread_, NULL);
  event_queue_.Clear();
  decoder_thread_ = kSbThreadInvalid;
}

bool MediaDecoder::ProcessOneInputBuffer(std::deque<Event>* pending_work) {
  SB_DCHECK(pending_work);
  if (pending_work->empty()) {
    return false;
  }

  SB_CHECK(media_codec_bridge_);

  // During secure playback, and only secure playback, is is possible that our
  // attempt to enqueue an input buffer will be rejected by MediaCodec because
  // we do not have a key yet.  In this case, we hold on to the input buffer
  // that we have already set up, and repeatedly attempt to enqueue it until
  // it works.  Ideally, we would just wait until MediaDrm was ready, however
  // the shared starboard player framework assumes that it is possible to
  // perform decryption and decoding as separate steps, so from its
  // perspective, having made it to this point implies that we ready to
  // decode.  It is not possible to do them as separate steps on Android. From
  // the perspective of user application, decryption and decoding are one
  // atomic step.
  DequeueInputResult dequeue_input_result;
  Event event;
  bool input_buffer_already_written = false;
  if (pending_queue_input_buffer_task_) {
    dequeue_input_result =
        pending_queue_input_buffer_task_->dequeue_input_result;
    SB_DCHECK(dequeue_input_result.index >= 0);
    event = pending_queue_input_buffer_task_->event;
    pending_queue_input_buffer_task_ = nullopt_t();
    input_buffer_already_written = true;
  } else {
    dequeue_input_result =
        media_codec_bridge_->DequeueInputBuffer(kDequeueTimeout);
    event = pending_work->front();
    if (dequeue_input_result.index < 0) {
      HandleError("dequeueInputBuffer", dequeue_input_result.status);
      return false;
    }
    pending_work->pop_front();
  }

  SB_DCHECK(event.type == Event::kWriteCodecConfig ||
            event.type == Event::kWriteInputBuffer ||
            event.type == Event::kWriteEndOfStream);
  const scoped_refptr<InputBuffer>& input_buffer = event.input_buffer;
  if (event.type == Event::kWriteEndOfStream) {
    SB_DCHECK(pending_work->empty());
  }
  const void* data = NULL;
  int size = 0;
  if (event.type == Event::kWriteCodecConfig) {
    SB_DCHECK(media_type_ == kSbMediaTypeAudio);
    data = event.codec_config;
    size = event.codec_config_size;
  } else if (event.type == Event::kWriteInputBuffer) {
    data = input_buffer->data();
    size = input_buffer->size();
  } else if (event.type == Event::kWriteEndOfStream) {
    data = NULL;
    size = 0;
  }

  // Don't bother rewriting the same data if we already did it last time we
  // were called and had it stored in |pending_queue_input_buffer_task_|.
  if (!input_buffer_already_written && event.type != Event::kWriteEndOfStream) {
    ScopedJavaByteBuffer byte_buffer(
        media_codec_bridge_->GetInputBuffer(dequeue_input_result.index));
    if (byte_buffer.IsNull() || byte_buffer.capacity() < size) {
      SB_LOG(ERROR) << "Unable to write to MediaCodec input buffer.";
      return false;
    }
    byte_buffer.CopyInto(data, size);
  }

  jint status;
  if (event.type == Event::kWriteCodecConfig) {
    status = media_codec_bridge_->QueueInputBuffer(dequeue_input_result.index,
                                                   kNoOffset, size, kNoPts,
                                                   BUFFER_FLAG_CODEC_CONFIG);
  } else if (event.type == Event::kWriteInputBuffer) {
    jlong pts_us = ConvertSbMediaTimeToMicroseconds(input_buffer->pts());
    if (drm_system_ && input_buffer->drm_info()) {
      status = media_codec_bridge_->QueueSecureInputBuffer(
          dequeue_input_result.index, kNoOffset, *input_buffer->drm_info(),
          pts_us);
    } else {
      status = media_codec_bridge_->QueueInputBuffer(
          dequeue_input_result.index, kNoOffset, size, pts_us, kNoBufferFlags);
    }
  } else {
    status = media_codec_bridge_->QueueInputBuffer(dequeue_input_result.index,
                                                   kNoOffset, size, kNoPts,
                                                   BUFFER_FLAG_END_OF_STREAM);
  }

  if (status != MEDIA_CODEC_OK) {
    HandleError("queue(Secure)?InputBuffer", status);
    // TODO: Stop the decoding loop on fatal error.
    SB_DCHECK(!pending_queue_input_buffer_task_);
    pending_queue_input_buffer_task_ = {dequeue_input_result, event};
    return false;
  }

  return true;
}

bool MediaDecoder::DequeueAndProcessOutputBuffer() {
  SB_CHECK(media_codec_bridge_);

  DequeueOutputResult dequeue_output_result =
      media_codec_bridge_->DequeueOutputBuffer(kDequeueTimeout);

  // Note that if the |index| field of |DequeueOutputResult| is negative, then
  // all fields other than |status| and |index| are invalid.  This is
  // especially important, as the Java side of |MediaCodecBridge| will reuse
  // objects for returned results behind the scenes.
  if (dequeue_output_result.index < 0) {
    if (dequeue_output_result.status == MEDIA_CODEC_OUTPUT_FORMAT_CHANGED) {
      host_->RefreshOutputFormat(media_codec_bridge_.get());
      return true;
    }

    if (dequeue_output_result.status == MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED) {
      SB_DLOG(INFO) << "Output buffers changed, trying to dequeue again.";
      return true;
    }

    HandleError("dequeueOutputBuffer", dequeue_output_result.status);
    return false;
  }

  host_->ProcessOutputBuffer(media_codec_bridge_.get(), dequeue_output_result);
  return true;
}

void MediaDecoder::HandleError(const char* action_name, jint status) {
  SB_DCHECK(status != MEDIA_CODEC_OK);

  bool retry = false;
  if (status == MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER) {
    // Don't bother logging a try again later status, it happens a lot.
    return;
  } else if (status == MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER) {
    // Don't bother logging a try again later status, it will happen a lot.
    return;
  } else if (status == MEDIA_CODEC_NO_KEY) {
    retry = true;
  } else if (status == MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION) {
    drm_system_->OnInsufficientOutputProtection();
  } else {
    error_cb_();
  }

  if (retry) {
    SB_LOG(INFO) << "|" << action_name << "| failed with status: "
                 << GetNameForMediaCodecStatus(status)
                 << ", will try again after a delay.";
  } else {
    SB_LOG(ERROR) << "|" << action_name << "| failed with status: "
                  << GetNameForMediaCodecStatus(status) << ".";
  }
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
