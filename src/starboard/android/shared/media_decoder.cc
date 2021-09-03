// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/shared/pthread/thread_create_priority.h"

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

const char* GetDecoderName(SbMediaType media_type) {
  return media_type == kSbMediaTypeAudio ? "audio_decoder" : "video_decoder";
}

}  // namespace

MediaDecoder::MediaDecoder(Host* host,
                           SbMediaAudioCodec audio_codec,
                           const SbMediaAudioSampleInfo& audio_sample_info,
                           SbDrmSystem drm_system)
    : media_type_(kSbMediaTypeAudio),
      host_(host),
      drm_system_(static_cast<DrmSystem*>(drm_system)),
      condition_variable_(mutex_),
      tunnel_mode_enabled_(false) {
  SB_DCHECK(host_);

  jobject j_media_crypto = drm_system_ ? drm_system_->GetMediaCrypto() : NULL;
  SB_DCHECK(!drm_system_ || j_media_crypto);
  media_codec_bridge_ = MediaCodecBridge::CreateAudioMediaCodecBridge(
      audio_codec, audio_sample_info, this, j_media_crypto);
  if (!media_codec_bridge_) {
    SB_LOG(ERROR) << "Failed to create audio media codec bridge.";
    return;
  }
  if (audio_sample_info.audio_specific_config_size > 0) {
    // |audio_sample_info.audio_specific_config| is guaranteed to be outlived
    // the decoder as it is stored in |FilterBasedPlayerWorkerHandler|.
    pending_tasks_.push_back(Event(
        static_cast<const int8_t*>(audio_sample_info.audio_specific_config),
        audio_sample_info.audio_specific_config_size));
    number_of_pending_tasks_.increment();
  }
}

MediaDecoder::MediaDecoder(Host* host,
                           SbMediaVideoCodec video_codec,
                           int width,
                           int height,
                           int fps,
                           jobject j_output_surface,
                           SbDrmSystem drm_system,
                           const SbMediaColorMetadata* color_metadata,
                           bool require_software_codec,
                           const FrameRenderedCB& frame_rendered_cb,
                           int tunnel_mode_audio_session_id,
                           bool force_big_endian_hdr_metadata,
                           std::string* error_message)
    : media_type_(kSbMediaTypeVideo),
      host_(host),
      drm_system_(static_cast<DrmSystem*>(drm_system)),
      frame_rendered_cb_(frame_rendered_cb),
      tunnel_mode_enabled_(tunnel_mode_audio_session_id != -1),
      condition_variable_(mutex_) {
  SB_DCHECK(frame_rendered_cb_);

  jobject j_media_crypto = drm_system_ ? drm_system_->GetMediaCrypto() : NULL;
  SB_DCHECK(!drm_system_ || j_media_crypto);
  media_codec_bridge_ = MediaCodecBridge::CreateVideoMediaCodecBridge(
      video_codec, width, height, fps, this, j_output_surface, j_media_crypto,
      color_metadata, require_software_codec, tunnel_mode_audio_session_id,
      force_big_endian_hdr_metadata, error_message);
  if (!media_codec_bridge_) {
    SB_LOG(ERROR) << "Failed to create video media codec bridge with error: "
                  << *error_message;
  }
}

MediaDecoder::~MediaDecoder() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  destroying_.store(true);
  condition_variable_.Signal();

  if (SbThreadIsValid(decoder_thread_)) {
    SbThreadJoin(decoder_thread_, NULL);
    decoder_thread_ = kSbThreadInvalid;
  }

  if (is_valid()) {
    host_->OnFlushing();
    // After |decoder_thread_| is ended and before |media_codec_bridge_| is
    // flushed, OnMediaCodecOutputBufferAvailable() would still be called.
    // So that, |dequeue_output_results_| may not be empty. As
    // DequeueOutputResult is consisted of plain data, it's fine to let
    // destructor delete |dequeue_output_results_|.
    jint status = media_codec_bridge_->Flush();
    if (status != MEDIA_CODEC_OK) {
      SB_LOG(ERROR) << "Failed to flush media codec.";
    }
    host_ = NULL;
  }
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

  if (stream_ended_.load()) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }

  if (!SbThreadIsValid(decoder_thread_)) {
    decoder_thread_ = SbThreadCreate(
        0,
        media_type_ == kSbMediaTypeAudio ? kSbThreadPriorityNormal
                                         : kSbThreadPriorityHigh,
        kSbThreadNoAffinity, true, GetDecoderName(media_type_),
        &MediaDecoder::DecoderThreadEntryPoint, this);
    SB_DCHECK(SbThreadIsValid(decoder_thread_));
  }

  ScopedLock scoped_lock(mutex_);
  pending_tasks_.push_back(Event(input_buffer));
  number_of_pending_tasks_.increment();
  if (pending_tasks_.size() == 1) {
    condition_variable_.Signal();
  }
}

void MediaDecoder::WriteEndOfStream() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  stream_ended_.store(true);
  ScopedLock scoped_lock(mutex_);
  pending_tasks_.push_back(Event(Event::kWriteEndOfStream));
  number_of_pending_tasks_.increment();
  if (pending_tasks_.size() == 1) {
    condition_variable_.Signal();
  }
}

void MediaDecoder::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(media_type_ == kSbMediaTypeVideo);
  SB_DCHECK(media_codec_bridge_);
  media_codec_bridge_->SetPlaybackRate(playback_rate);
}

// static
void* MediaDecoder::DecoderThreadEntryPoint(void* context) {
  SB_DCHECK(context);
  MediaDecoder* decoder = static_cast<MediaDecoder*>(context);
  decoder->DecoderThreadFunc();
  return NULL;
}

void MediaDecoder::DecoderThreadFunc() {
  SB_DCHECK(error_cb_);

  if (media_type_ == kSbMediaTypeAudio) {
    std::deque<Event> pending_tasks;
    std::vector<int> input_buffer_indices;

    while (!destroying_.load()) {
      std::vector<DequeueOutputResult> dequeue_output_results;
      {
        ScopedLock scoped_lock(mutex_);
        bool has_input = !pending_tasks.empty() || !pending_tasks_.empty();
        bool has_input_buffer =
            !input_buffer_indices.empty() || !input_buffer_indices_.empty();
        bool can_process_input =
            pending_queue_input_buffer_task_ || (has_input && has_input_buffer);
        if (dequeue_output_results_.empty() && !can_process_input) {
          if (!condition_variable_.WaitTimed(5 * kSbTimeSecond)) {
            SB_LOG_IF(ERROR, !stream_ended_.load())
                << GetDecoderName(media_type_) << ": Wait() hits timeout.";
          }
        }
        SB_DCHECK(dequeue_output_results.empty());
        CollectPendingData_Locked(&pending_tasks, &input_buffer_indices,
                                  &dequeue_output_results);
      }

      for (auto dequeue_output_result : dequeue_output_results) {
        if (dequeue_output_result.index < 0) {
          host_->RefreshOutputFormat(media_codec_bridge_.get());
        } else {
          host_->ProcessOutputBuffer(media_codec_bridge_.get(),
                                     dequeue_output_result);
        }
      }

      for (;;) {
        bool can_process_input =
            pending_queue_input_buffer_task_ ||
            (!pending_tasks.empty() && !input_buffer_indices.empty());
        if (!can_process_input) {
          break;
        }
        if (!ProcessOneInputBuffer(&pending_tasks, &input_buffer_indices)) {
          break;
        }
      }
    }
  } else {
    // While it is possible to consolidate the logic for audio and video
    // decoders, it is easy to fine tune the behavior of video decoder if they
    // are separated.
    std::deque<Event> pending_tasks;
    std::vector<int> input_buffer_indices;
    std::vector<DequeueOutputResult> dequeue_output_results;

    while (!destroying_.load()) {
      bool has_input =
          pending_queue_input_buffer_task_ ||
          (!pending_tasks.empty() && !input_buffer_indices.empty());
      bool has_output = !dequeue_output_results.empty();
      bool collect_pending_data = false;

      if (tunnel_mode_enabled_) {
        // We don't explicitly process output in tunnel mode.
        collect_pending_data = !has_input;
      } else {
        collect_pending_data = !has_input || !has_output;
      }

      if (collect_pending_data) {
        ScopedLock scoped_lock(mutex_);
        CollectPendingData_Locked(&pending_tasks, &input_buffer_indices,
                                  &dequeue_output_results);
      }

      if (!tunnel_mode_enabled_) {
        // Output is only processed when tunnel mode is disabled.
        if (!dequeue_output_results.empty()) {
          auto& dequeue_output_result = dequeue_output_results.front();
          if (dequeue_output_result.index < 0) {
            host_->RefreshOutputFormat(media_codec_bridge_.get());
          } else {
            host_->ProcessOutputBuffer(media_codec_bridge_.get(),
                                       dequeue_output_result);
          }
          dequeue_output_results.erase(dequeue_output_results.begin());
        }
        host_->Tick(media_codec_bridge_.get());
      }

      bool can_process_input =
          pending_queue_input_buffer_task_ ||
          (!pending_tasks.empty() && !input_buffer_indices.empty());
      if (can_process_input) {
        ProcessOneInputBuffer(&pending_tasks, &input_buffer_indices);
      }

      bool ticked = false;
      if (!tunnel_mode_enabled_) {
        // Output is only processed when tunnel mode is disabled.
        ticked = host_->Tick(media_codec_bridge_.get());
      }

      can_process_input =
          pending_queue_input_buffer_task_ ||
          (!pending_tasks.empty() && !input_buffer_indices.empty());
      if (!ticked && !can_process_input && dequeue_output_results.empty()) {
        ScopedLock scoped_lock(mutex_);
        CollectPendingData_Locked(&pending_tasks, &input_buffer_indices,
                                  &dequeue_output_results);
        can_process_input =
            !pending_tasks.empty() && !input_buffer_indices.empty();
        if (!can_process_input && dequeue_output_results.empty()) {
          condition_variable_.WaitTimed(kSbTimeMillisecond);
        }
      }
    }
  }

  SB_LOG(INFO) << "Destroying decoder thread.";
}

void MediaDecoder::CollectPendingData_Locked(
    std::deque<Event>* pending_tasks,
    std::vector<int>* input_buffer_indices,
    std::vector<DequeueOutputResult>* dequeue_output_results) {
  SB_DCHECK(pending_tasks);
  SB_DCHECK(input_buffer_indices);
  SB_DCHECK(dequeue_output_results);
  mutex_.DCheckAcquired();

  pending_tasks->insert(pending_tasks->end(), pending_tasks_.begin(),
                        pending_tasks_.end());
  pending_tasks_.clear();

  input_buffer_indices->insert(input_buffer_indices->end(),
                               input_buffer_indices_.begin(),
                               input_buffer_indices_.end());
  input_buffer_indices_.clear();

  dequeue_output_results->insert(dequeue_output_results->end(),
                                 dequeue_output_results_.begin(),
                                 dequeue_output_results_.end());
  dequeue_output_results_.clear();
}

bool MediaDecoder::ProcessOneInputBuffer(
    std::deque<Event>* pending_tasks,
    std::vector<int>* input_buffer_indices) {
  SB_DCHECK(media_codec_bridge_);

  // During secure playback, and only secure playback, it is possible that our
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
    dequeue_input_result.index = input_buffer_indices->front();
    input_buffer_indices->erase(input_buffer_indices->begin());
    event = pending_tasks->front();
    pending_tasks->pop_front();
    number_of_pending_tasks_.decrement();
  }

  SB_DCHECK(event.type == Event::kWriteCodecConfig ||
            event.type == Event::kWriteInputBuffer ||
            event.type == Event::kWriteEndOfStream);
  const scoped_refptr<InputBuffer>& input_buffer = event.input_buffer;
  if (event.type == Event::kWriteEndOfStream) {
    SB_DCHECK(pending_tasks->empty());
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
    if (byte_buffer.IsNull()) {
      SB_LOG(ERROR) << "Unable to write to MediaCodec buffer, |byte_buffer| is"
                    << " null.";
      // TODO: Stop the decoding loop and call error_cb_ on fatal error.
      return false;
    }
    if (byte_buffer.capacity() < size) {
      auto error_message = FormatString(
          "Unable to write to MediaCodec buffer, input buffer size (%d) is"
          " greater than |byte_buffer.capacity()| (%d).",
          size, static_cast<int>(byte_buffer.capacity()));
      SB_LOG(ERROR) << error_message;
      error_cb_(kSbPlayerErrorDecode, error_message);
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
    jlong pts_us = input_buffer->timestamp();
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
    host_->OnEndOfStreamWritten(media_codec_bridge_.get());
  }

  if (status != MEDIA_CODEC_OK) {
    HandleError("queue(Secure)?InputBuffer", status);
    // TODO: Stop the decoding loop and call error_cb_ on fatal error.
    SB_DCHECK(!pending_queue_input_buffer_task_);
    pending_queue_input_buffer_task_ = {dequeue_input_result, event};
    return false;
  }

  is_output_restricted_ = false;
  return true;
}

void MediaDecoder::HandleError(const char* action_name, jint status) {
  SB_DCHECK(status != MEDIA_CODEC_OK);

  bool retry = false;

  if (status != MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION) {
    is_output_restricted_ = false;
  }

  if (status == MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER) {
    // Don't bother logging a try again later status, it happens a lot.
    return;
  } else if (status == MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER) {
    // Don't bother logging a try again later status, it will happen a lot.
    return;
  } else if (status == MEDIA_CODEC_NO_KEY) {
    retry = true;
  } else if (status == MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION) {
    // TODO: Reduce the retry frequency when output is restricted, or when
    // queueSecureInputBuffer() is failed in general.
    if (is_output_restricted_) {
      return;
    }
    is_output_restricted_ = true;
    drm_system_->OnInsufficientOutputProtection();
  } else {
    if (media_type_ == kSbMediaTypeAudio) {
      error_cb_(kSbPlayerErrorDecode,
                FormatString("%s failed with status %d (audio).", action_name,
                             status));
    } else {
      error_cb_(kSbPlayerErrorDecode,
                FormatString("%s failed with status %d (video).", action_name,
                             status));
    }
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

void MediaDecoder::OnMediaCodecError(bool is_recoverable,
                                     bool is_transient,
                                     const std::string& diagnostic_info) {
  SB_LOG(WARNING) << "MediaDecoder encountered "
                  << (is_recoverable ? "recoverable, " : "unrecoverable, ")
                  << (is_transient ? "transient " : "intransient ")
                  << " error with message: " << diagnostic_info;

  if (!is_transient) {
    if (media_type_ == kSbMediaTypeAudio) {
      error_cb_(kSbPlayerErrorDecode,
                "OnMediaCodecError (audio): " + diagnostic_info +
                    (is_recoverable ? ", recoverable " : ", unrecoverable "));
    } else {
      error_cb_(kSbPlayerErrorDecode,
                "OnMediaCodecError (video): " + diagnostic_info +
                    (is_recoverable ? ", recoverable " : ", unrecoverable "));
    }
  }
}

void MediaDecoder::OnMediaCodecInputBufferAvailable(int buffer_index) {
  if (media_type_ == kSbMediaTypeVideo && first_call_on_handler_thread_) {
    // Set the thread priority of the Handler thread to dispatch the async
    // decoder callbacks to high.
    ::starboard::shared::pthread::ThreadSetPriority(kSbThreadPriorityHigh);
    first_call_on_handler_thread_ = false;
  }
  ScopedLock scoped_lock(mutex_);
  input_buffer_indices_.push_back(buffer_index);
  if (input_buffer_indices_.size() == 1) {
    condition_variable_.Signal();
  }
}

void MediaDecoder::OnMediaCodecOutputBufferAvailable(
    int buffer_index,
    int flags,
    int offset,
    int64_t presentation_time_us,
    int size) {
  SB_DCHECK(media_codec_bridge_);
  SB_DCHECK(buffer_index >= 0);

  DequeueOutputResult dequeue_output_result;
  dequeue_output_result.status = 0;
  dequeue_output_result.index = buffer_index;
  dequeue_output_result.flags = flags;
  dequeue_output_result.offset = offset;
  dequeue_output_result.presentation_time_microseconds = presentation_time_us;
  dequeue_output_result.num_bytes = size;

  ScopedLock scoped_lock(mutex_);
  dequeue_output_results_.push_back(dequeue_output_result);
  condition_variable_.Signal();
}

void MediaDecoder::OnMediaCodecOutputFormatChanged() {
  SB_DCHECK(media_codec_bridge_);

  DequeueOutputResult dequeue_output_result = {};
  dequeue_output_result.index = -1;

  ScopedLock scoped_lock(mutex_);
  dequeue_output_results_.push_back(dequeue_output_result);
  condition_variable_.Signal();
}

void MediaDecoder::OnMediaCodecFrameRendered(SbTime frame_timestamp) {
  SB_DCHECK(tunnel_mode_enabled_);
  frame_rendered_cb_(frame_timestamp);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
