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

#include <sched.h>
#include <unistd.h>

#include "base/android/jni_android.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_state.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/shared/pthread/thread_create_priority.h"

namespace starboard::android::shared {

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AttachCurrentThread;

namespace {

const jlong kDequeueTimeout = 0;

const jint kNoOffset = 0;
const jlong kNoPts = 0;
const jint kNoSize = 0;
const jint kNoBufferFlags = 0;

// Delay to use after a retryable error has been encountered.
const int64_t kErrorRetryDelay = 50'000;  // 50ms

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
      SB_NOTREACHED() << "Unknown status value: " << status;
      return "MEDIA_CODEC_ERROR_UNKNOWN";
  }
}

const char* GetDecoderName(SbMediaType media_type) {
  return media_type == kSbMediaTypeAudio ? "audio_decoder" : "video_decoder";
}

}  // namespace

MediaDecoder::MediaDecoder(Host* host,
                           const AudioStreamInfo& audio_stream_info,
                           SbDrmSystem drm_system)
    : media_type_(kSbMediaTypeAudio),
      host_(host),
      drm_system_(static_cast<DrmSystem*>(drm_system)),
      tunnel_mode_enabled_(false),
      flush_delay_usec_(0),
      decoder_state_tracker_(nullptr),
      condition_variable_(mutex_) {
  SB_CHECK(host_);

  jobject j_media_crypto = drm_system_ ? drm_system_->GetMediaCrypto() : NULL;
  SB_DCHECK(!drm_system_ || j_media_crypto);
  media_codec_bridge_ = MediaCodecBridge::CreateAudioMediaCodecBridge(
      audio_stream_info, this, j_media_crypto);
  if (!media_codec_bridge_) {
    SB_LOG(ERROR) << "Failed to create audio media codec bridge.";
    return;
  }
  // When |audio_stream_info.codec| == kSbMediaAudioCodecOpus, we instead send
  // the audio specific configuration when we create the MediaCodec object in
  // the call to MediaCodecBridge::CreateAudioMediaCodecBridge() above.
  // TODO: Determine if we should send the audio specific configuration here
  // only when |audio_codec| == kSbMediaAudioCodecAac.
  if (audio_stream_info.codec != kSbMediaAudioCodecOpus &&
      !audio_stream_info.audio_specific_config.empty()) {
    pending_inputs_.emplace_back(audio_stream_info.audio_specific_config);
    ++number_of_pending_inputs_;
  }
}

MediaDecoder::MediaDecoder(
    Host* host,
    SbMediaVideoCodec video_codec,
    int width_hint,
    int height_hint,
    std::optional<int> max_width,
    std::optional<int> max_height,
    int fps,
    jobject j_output_surface,
    SbDrmSystem drm_system,
    const SbMediaColorMetadata* color_metadata,
    bool require_software_codec,
    const FrameRenderedCB& frame_rendered_cb,
    const FirstTunnelFrameReadyCB& first_tunnel_frame_ready_cb,
    int tunnel_mode_audio_session_id,
    bool force_big_endian_hdr_metadata,
    int max_video_input_size,
    int64_t flush_delay_usec,
    std::optional<int> max_frames_in_decoder,
    std::string* error_message)
    : media_type_(kSbMediaTypeVideo),
      host_(host),
      drm_system_(static_cast<DrmSystem*>(drm_system)),
      frame_rendered_cb_(frame_rendered_cb),
      first_tunnel_frame_ready_cb_(first_tunnel_frame_ready_cb),
      tunnel_mode_enabled_(tunnel_mode_audio_session_id != -1),
      flush_delay_usec_(flush_delay_usec),
      decoder_state_tracker_(max_frames_in_decoder
                                 ? std::make_unique<DecoderStateTracker>(
                                       *max_frames_in_decoder,
                                       [this] { condition_variable_.Signal(); })
                                 : nullptr),
      condition_variable_(mutex_) {
  SB_DCHECK(frame_rendered_cb_);
  SB_DCHECK(first_tunnel_frame_ready_cb_);

  jobject j_media_crypto = drm_system_ ? drm_system_->GetMediaCrypto() : NULL;
  const bool require_secured_decoder =
      drm_system_ && drm_system_->require_secured_decoder();
  SB_DCHECK(!drm_system_ || j_media_crypto);
  media_codec_bridge_ = MediaCodecBridge::CreateVideoMediaCodecBridge(
      video_codec, width_hint, height_hint, fps, max_width, max_height, this,
      j_output_surface, j_media_crypto, color_metadata, require_secured_decoder,
      require_software_codec, tunnel_mode_audio_session_id,
      force_big_endian_hdr_metadata, max_video_input_size, error_message);
  if (!media_codec_bridge_) {
    SB_LOG(ERROR) << "Failed to create video media codec bridge with error: "
                  << *error_message;
  }
}

MediaDecoder::~MediaDecoder() {
  SB_CHECK(thread_checker_.CalledOnValidThread());

  TerminateDecoderThread();

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
    // Call stop() here to notify MediaCodecBridge to not invoke any callbacks.
    media_codec_bridge_->Stop();
  }
}

void MediaDecoder::Initialize(const ErrorCB& error_cb) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  error_cb_ = error_cb;

  if (error_occurred_) {
    Schedule(std::bind(error_cb_, error_, error_message_));
  }
}

void MediaDecoder::WriteInputBuffers(const InputBuffers& input_buffers) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  if (stream_ended_.load()) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }
  if (input_buffers.empty()) {
    SB_LOG(ERROR) << "No input buffer to decode.";
    SB_DCHECK(!input_buffers.empty());
    return;
  }

  if (decoder_thread_ == 0) {
    pthread_create(&decoder_thread_, nullptr,
                   &MediaDecoder::DecoderThreadEntryPoint, this);
    SB_DCHECK_NE(decoder_thread_, 0);
  }

  ScopedLock scoped_lock(mutex_);
  bool need_signal = pending_inputs_.empty();
  for (const auto& input_buffer : input_buffers) {
    pending_inputs_.emplace_back(input_buffer);
    ++number_of_pending_inputs_;
  }
  if (need_signal) {
    condition_variable_.Signal();
  }
}

void MediaDecoder::WriteEndOfStream() {
  SB_CHECK(thread_checker_.CalledOnValidThread());

  stream_ended_.store(true);
  ScopedLock scoped_lock(mutex_);
  pending_inputs_.emplace_back(PendingInput::kWriteEndOfStream);
  ++number_of_pending_inputs_;
  if (pending_inputs_.size() == 1) {
    condition_variable_.Signal();
  }
}

void MediaDecoder::SetPlaybackRate(double playback_rate) {
  SB_DCHECK_EQ(media_type_, kSbMediaTypeVideo);
  SB_DCHECK(media_codec_bridge_);
  media_codec_bridge_->SetPlaybackRate(playback_rate);
}

// static
void* MediaDecoder::DecoderThreadEntryPoint(void* context) {
  SB_CHECK(context);
  MediaDecoder* decoder = static_cast<MediaDecoder*>(context);
  pthread_setname_np(pthread_self(), GetDecoderName(decoder->media_type_));
  if (decoder->media_type_ == kSbMediaTypeAudio) {
    ::starboard::shared::pthread::ThreadSetPriority(kSbThreadPriorityNormal);
  } else {
    ::starboard::shared::pthread::ThreadSetPriority(kSbThreadPriorityHigh);
  }

  decoder->DecoderThreadFunc();
  JNIState::GetVM()->DetachCurrentThread();
  return NULL;
}

void MediaDecoder::DecoderThreadFunc() {
  SB_DCHECK(error_cb_);

  if (media_type_ == kSbMediaTypeAudio) {
    std::deque<PendingInput> pending_inputs;
    std::vector<int> input_buffer_indices;

    while (!destroying_.load()) {
      std::vector<DequeueOutputResult> dequeue_output_results;
      {
        ScopedLock scoped_lock(mutex_);
        bool has_pending_input =
            !pending_inputs.empty() || !pending_inputs_.empty();
        bool has_input_buffer_indices =
            !input_buffer_indices.empty() || !input_buffer_indices_.empty();
        bool can_process_input =
            pending_input_to_retry_ ||
            (has_pending_input && has_input_buffer_indices);
        if (dequeue_output_results_.empty() && !can_process_input) {
          if (!condition_variable_.WaitTimed(5'000'000LL)) {
            SB_LOG_IF(ERROR, !stream_ended_.load())
                << GetDecoderName(media_type_) << ": Wait() hits timeout.";
          }
        }
        SB_DCHECK(dequeue_output_results.empty());
        if (destroying_.load()) {
          break;
        }
        CollectPendingData_Locked(&pending_inputs, &input_buffer_indices,
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
            pending_input_to_retry_ ||
            (!pending_inputs.empty() && !input_buffer_indices.empty());
        if (!can_process_input) {
          break;
        }
        if (!ProcessOneInputBuffer(&pending_inputs, &input_buffer_indices)) {
          break;
        }
      }
    }
  } else {
    // While it is possible to consolidate the logic for audio and video
    // decoders, it is easy to fine tune the behavior of video decoder if they
    // are separated.
    std::deque<PendingInput> pending_inputs;
    std::vector<int> input_buffer_indices;
    std::vector<DequeueOutputResult> dequeue_output_results;

    while (!destroying_.load()) {
      // TODO(b/329686979): access to `ending_input_to_retry_` should be
      //                    synchronized.
      bool has_input =
          pending_input_to_retry_ ||
          (!pending_inputs.empty() && !input_buffer_indices.empty());
      bool has_output = !dequeue_output_results.empty();
      bool collect_pending_data = false;

      if (tunnel_mode_enabled_) {
        // We don't explicitly process output in tunnel mode.
        collect_pending_data = !has_input;
      } else {
        collect_pending_data = !has_input || !has_output;
      }

      if (destroying_.load()) {
        break;
      }
      if (collect_pending_data) {
        ScopedLock scoped_lock(mutex_);
        CollectPendingData_Locked(&pending_inputs, &input_buffer_indices,
                                  &dequeue_output_results);
      }

      if (!dequeue_output_results.empty()) {
        auto& dequeue_output_result = dequeue_output_results.front();
        if (dequeue_output_result.index < 0) {
          host_->RefreshOutputFormat(media_codec_bridge_.get());
        } else {
          SB_DCHECK(!tunnel_mode_enabled_);
          host_->ProcessOutputBuffer(media_codec_bridge_.get(),
                                     dequeue_output_result);
        }
        dequeue_output_results.erase(dequeue_output_results.begin());
      }
      if (!tunnel_mode_enabled_) {
        host_->Tick(media_codec_bridge_.get());
      }

      bool can_process_input =
          pending_input_to_retry_ ||
          (!pending_inputs.empty() && !input_buffer_indices.empty());
      if (decoder_state_tracker_ && !decoder_state_tracker_->CanAcceptMore()) {
        can_process_input = false;
      }

      if (can_process_input) {
        ProcessOneInputBuffer(&pending_inputs, &input_buffer_indices);
      }

      bool ticked = false;
      if (!tunnel_mode_enabled_) {
        // Output is only processed when tunnel mode is disabled.
        ticked = host_->Tick(media_codec_bridge_.get());
      }

      can_process_input =
          pending_input_to_retry_ ||
          (!pending_inputs.empty() && !input_buffer_indices.empty());
      if (!ticked && !can_process_input && dequeue_output_results.empty()) {
        ScopedLock scoped_lock(mutex_);
        CollectPendingData_Locked(&pending_inputs, &input_buffer_indices,
                                  &dequeue_output_results);
        can_process_input =
            !pending_inputs.empty() && !input_buffer_indices.empty();
        if (!can_process_input && dequeue_output_results.empty()) {
          condition_variable_.WaitTimed(1000);
        }
      }
    }
  }

  SB_LOG(INFO) << "Destroying decoder thread.";
}

void MediaDecoder::TerminateDecoderThread() {
  SB_CHECK(thread_checker_.CalledOnValidThread());

  destroying_.store(true);

  {
    ScopedLock scoped_lock(mutex_);
    condition_variable_.Signal();
  }

  if (decoder_thread_ != 0) {
    SB_CHECK_EQ(pthread_join(decoder_thread_, nullptr), 0);
    decoder_thread_ = 0;
  }
}

void MediaDecoder::CollectPendingData_Locked(
    std::deque<PendingInput>* pending_inputs,
    std::vector<int>* input_buffer_indices,
    std::vector<DequeueOutputResult>* dequeue_output_results) {
  SB_DCHECK(pending_inputs);
  SB_DCHECK(input_buffer_indices);
  SB_DCHECK(dequeue_output_results);

  pending_inputs->insert(pending_inputs->end(), pending_inputs_.begin(),
                         pending_inputs_.end());
  pending_inputs_.clear();

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
    std::deque<PendingInput>* pending_inputs,
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
  PendingInput pending_input;
  bool input_buffer_already_written = false;
  if (pending_input_to_retry_) {
    dequeue_input_result = pending_input_to_retry_->dequeue_input_result;
    SB_DCHECK_GE(dequeue_input_result.index, 0);
    pending_input = pending_input_to_retry_->pending_input;
    pending_input_to_retry_ = std::nullopt;
    input_buffer_already_written = true;
  } else {
    dequeue_input_result.index = input_buffer_indices->front();
    input_buffer_indices->erase(input_buffer_indices->begin());
    pending_input = pending_inputs->front();
    pending_inputs->pop_front();
    --number_of_pending_inputs_;
  }

  SB_DCHECK(pending_input.type == PendingInput::kWriteCodecConfig ||
            pending_input.type == PendingInput::kWriteInputBuffer ||
            pending_input.type == PendingInput::kWriteEndOfStream);
  const scoped_refptr<InputBuffer>& input_buffer = pending_input.input_buffer;
  if (pending_input.type == PendingInput::kWriteEndOfStream) {
    SB_DCHECK(pending_inputs->empty());
  }
  const void* data = NULL;
  int size = 0;
  if (pending_input.type == PendingInput::kWriteCodecConfig) {
    SB_DCHECK_EQ(media_type_, kSbMediaTypeAudio);
    data = pending_input.codec_config.data();
    size = pending_input.codec_config.size();
  } else if (pending_input.type == PendingInput::kWriteInputBuffer) {
    data = input_buffer->data();
    size = input_buffer->size();
  } else if (pending_input.type == PendingInput::kWriteEndOfStream) {
    data = NULL;
    size = 0;
  }

  // Don't bother rewriting the same data if we already did it last time we
  // were called and had it stored in |pending_input_to_retry_|.
  if (!input_buffer_already_written &&
      pending_input.type != PendingInput::kWriteEndOfStream) {
    ScopedJavaLocalRef<jobject> byte_buffer(
        media_codec_bridge_->GetInputBuffer(dequeue_input_result.index));
    if (byte_buffer.is_null()) {
      SB_LOG(ERROR) << "Unable to write to MediaCodec buffer, |byte_buffer| is"
                    << " null.";
      // TODO: Stop the decoding loop and call error_cb_ on fatal error.
      return false;
    }

    JNIEnv* env = AttachCurrentThread();
    jint capacity = env->GetDirectBufferCapacity(byte_buffer.obj());
    if (capacity < size) {
      auto error_message = FormatString(
          "Unable to write to MediaCodec buffer, input buffer size (%d) is"
          " greater than |byte_buffer.capacity()| (%d).",
          size, static_cast<int>(capacity));
      SB_LOG(ERROR) << error_message;
      ReportError(kSbPlayerErrorDecode, error_message);
      return false;
    }

    SB_DCHECK_GE(size, 0);
    SB_DCHECK_LE(size, capacity);
    void* address = env->GetDirectBufferAddress(byte_buffer.obj());
    memcpy(address, data, size);
  }

  if (size > 0 && decoder_state_tracker_) {
    decoder_state_tracker_->AddFrame(input_buffer->timestamp());
  }

  jint status;
  if (drm_system_ && !drm_system_->IsReady()) {
    // Drm system initialization is asynchronous. If there's a drm system, we
    // should wait until it's initialized to avoid errors.
    status = MEDIA_CODEC_NO_KEY;
  } else if (pending_input.type == PendingInput::kWriteCodecConfig) {
    status = media_codec_bridge_->QueueInputBuffer(
        dequeue_input_result.index, kNoOffset, size, kNoPts,
        BUFFER_FLAG_CODEC_CONFIG, false);
  } else if (pending_input.type == PendingInput::kWriteInputBuffer) {
    jlong pts_us = input_buffer->timestamp();
    if (drm_system_ && input_buffer->drm_info()) {
      status = media_codec_bridge_->QueueSecureInputBuffer(
          dequeue_input_result.index, kNoOffset, *input_buffer->drm_info(),
          pts_us, host_->IsBufferDecodeOnly(input_buffer));
    } else {
      status = media_codec_bridge_->QueueInputBuffer(
          dequeue_input_result.index, kNoOffset, size, pts_us, kNoBufferFlags,
          host_->IsBufferDecodeOnly(input_buffer));
    }
  } else {
    status = media_codec_bridge_->QueueInputBuffer(
        dequeue_input_result.index, kNoOffset, size, kNoPts,
        BUFFER_FLAG_END_OF_STREAM, false);
    host_->OnEndOfStreamWritten(media_codec_bridge_.get());
  }

  if (status != MEDIA_CODEC_OK) {
    HandleError("queue(Secure)?InputBuffer", status);
    // TODO: Stop the decoding loop and call error_cb_ on fatal error.
    SB_DCHECK(!pending_input_to_retry_);
    pending_input_to_retry_ = {dequeue_input_result, pending_input};
    return false;
  }

  is_output_restricted_ = false;
  return true;
}

void MediaDecoder::HandleError(const char* action_name, jint status) {
  SB_DCHECK_NE(status, MEDIA_CODEC_OK);

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
      ReportError(kSbPlayerErrorDecode,
                  FormatString("%s failed with status %d (audio).", action_name,
                               status));
    } else {
      ReportError(kSbPlayerErrorDecode,
                  FormatString("%s failed with status %d (video).", action_name,
                               status));
    }
  }

  if (retry) {
    SB_LOG(INFO) << "|" << action_name << "| failed with status: "
                 << GetNameForMediaCodecStatus(status)
                 << ", will try again after a delay.";
    sched_yield();
  } else {
    SB_LOG(ERROR) << "|" << action_name << "| failed with status: "
                  << GetNameForMediaCodecStatus(status) << ".";
  }
}

void MediaDecoder::ReportError(const SbPlayerError error,
                               const std::string error_message) {
  if (!BelongsToCurrentThread()) {
    Schedule(std::bind(&MediaDecoder::ReportError, this, error, error_message));
    return;
  }
  if (error_occurred_) {
    // Avoid to report error multiple times.
    return;
  }
  error_occurred_ = true;
  error_ = error;
  error_message_ = error_message;
  if (error_cb_) {
    error_cb_(error_, error_message_);
  }
}

void MediaDecoder::OnMediaCodecError(bool is_recoverable,
                                     bool is_transient,
                                     const std::string& diagnostic_info) {
  SB_LOG(WARNING) << "MediaDecoder encountered "
                  << (is_recoverable ? "recoverable, " : "unrecoverable, ")
                  << (is_transient ? "transient " : "intransient ")
                  << " error with message: " << diagnostic_info;
  // The callback may be called on a different thread and before |error_cb_| is
  // initialized.
  if (!is_transient) {
    if (media_type_ == kSbMediaTypeAudio) {
      ReportError(kSbPlayerErrorDecode,
                  "OnMediaCodecError (audio): " + diagnostic_info +
                      (is_recoverable ? ", recoverable " : ", unrecoverable "));
    } else {
      ReportError(kSbPlayerErrorDecode,
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
  SB_DCHECK_GE(buffer_index, 0);

  // TODO(b/291959069): After |decoder_thread_| is destroyed, it may still
  // receive output buffer, discard this invalid output buffer.
  if (destroying_.load() || decoder_thread_ == 0) {
    return;
  }

  if (size > 0 && decoder_state_tracker_) {
    decoder_state_tracker_->SetFrameDecoded(presentation_time_us);
  }

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

  FrameSize frame_size = media_codec_bridge_->GetOutputSize();
  SB_LOG(INFO) << __func__ << " > resolution=" << frame_size.display_width()
               << "x" << frame_size.display_height();

  DequeueOutputResult dequeue_output_result = {};
  dequeue_output_result.index = -1;

  ScopedLock scoped_lock(mutex_);
  dequeue_output_results_.push_back(dequeue_output_result);
  condition_variable_.Signal();
}

void MediaDecoder::OnMediaCodecFrameRendered(int64_t frame_timestamp) {
  frame_rendered_cb_(frame_timestamp);
}

void MediaDecoder::OnMediaCodecFirstTunnelFrameReady() {
  SB_DCHECK(tunnel_mode_enabled_);

  first_tunnel_frame_ready_cb_();
}

bool MediaDecoder::Flush() {
  SB_CHECK(thread_checker_.CalledOnValidThread());

  // Try to flush if we can, otherwise return |false| to recreate the codec
  // completely. Flush() is called by `player_worker` thread,
  // but MediaDecoder is on `audio_decoder` and `video_decoder`
  // threads, let `player_worker` destroy `audio_decoder` and
  // `video_decoder` threads to clean up all pending tasks,
  // and Flush()/Start() |media_codec_bridge_|.

  // 1. Terminate `audio_decoder` or `video_decoder` thread.
  TerminateDecoderThread();

  // 2. Flush()/Start() |media_codec_bridge_| and clean up pending tasks.
  if (is_valid()) {
    // 2.1. Flush() |media_codec_bridge_|.
    host_->OnFlushing();
    jint status = media_codec_bridge_->Flush();
    if (status != MEDIA_CODEC_OK) {
      SB_LOG(ERROR) << "Failed to flush media codec.";
      return false;
    }

    // 2.2. Clean up pending_inputs and input_buffer/output_buffer indices.
    number_of_pending_inputs_.store(0);
    pending_inputs_.clear();
    input_buffer_indices_.clear();
    dequeue_output_results_.clear();
    pending_input_to_retry_ = std::nullopt;

    if (decoder_state_tracker_) {
      decoder_state_tracker_->Reset();
    }

    // 2.3. Add OutputFormatChanged to get current output format after Flush().
    DequeueOutputResult dequeue_output_result = {};
    dequeue_output_result.index = -1;
    dequeue_output_results_.push_back(dequeue_output_result);

    // 2.4. Wait for |flush_delay_usec_| on pre Android 13 devices.
    if (flush_delay_usec_ > 0) {
      usleep(flush_delay_usec_);
    }

    // 2.5. Restart() |media_codec_bridge_|. As the codec is configured in
    // asynchronous mode, call Start() after Flush() has returned to
    // resume codec operations. After Restart(), input_buffer_index should
    // start with 0.
    if (!media_codec_bridge_->Restart()) {
      SB_LOG(ERROR) << "Failed to start media codec.";
      return false;
    }
  }

  // 3. Recreate `audio_decoder` and `video_decoder` threads in
  // WriteInputBuffers().
  stream_ended_.store(false);
  destroying_.store(false);
  return true;
}

}  // namespace starboard::android::shared
