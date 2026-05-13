// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/ndk_media_codec_bridge.h"

#include <android/api-level.h>
#include <android/native_window_jni.h>
#include <media/NdkMediaCrypto.h>
#include <media/NdkMediaFormat.h>

#include "starboard/common/log.h"
#include "starboard/common/string.h"

// Suppress availability warnings for API 28+ async callback symbols
#pragma clang diagnostic ignored "-Wunguarded-availability"

namespace starboard {

namespace {

void OnInputBufferAvailableCallback(AMediaCodec* codec,
                                    void* userdata,
                                    int32_t index) {
  auto* bridge = reinterpret_cast<NdkMediaCodecBridge*>(userdata);
  bridge->OnInputBufferAvailable(index);
}

void OnOutputBufferAvailableCallback(AMediaCodec* codec,
                                     void* userdata,
                                     int32_t index,
                                     AMediaCodecBufferInfo* info) {
  auto* bridge = reinterpret_cast<NdkMediaCodecBridge*>(userdata);
  bridge->OnOutputBufferAvailable(index, info);
}

void OnFormatChangedCallback(AMediaCodec* codec,
                             void* userdata,
                             AMediaFormat* format) {
  auto* bridge = reinterpret_cast<NdkMediaCodecBridge*>(userdata);
  bridge->OnFormatChanged(format);
}

void OnErrorCallback(AMediaCodec* codec,
                     void* userdata,
                     media_status_t error,
                     int32_t actionCode,
                     const char* detail) {
  auto* bridge = reinterpret_cast<NdkMediaCodecBridge*>(userdata);
  bridge->OnError(error, actionCode, detail);
}

}  // namespace

std::unique_ptr<NdkMediaCodecBridge> NdkMediaCodecBridge::Create(
    SbMediaVideoCodec video_codec,
    const std::string& decoder_name,
    const Size& frame_size_hint,
    int fps,
    const std::optional<Size>& max_frame_size,
    Handler* handler,
    jobject j_surface,
    jobject j_media_crypto,
    const SbMediaColorMetadata* color_metadata,
    bool require_secured_decoder,
    bool require_software_codec,
    int max_video_input_size,
    bool enable_output_checker) {
  JNIEnv* env = jni_zero::AttachCurrentThread();

  AMediaCodec* codec = AMediaCodec_createCodecByName(decoder_name.c_str());
  if (!codec) {
    SB_LOG(ERROR) << "AMediaCodec_createCodecByName failed for "
                  << decoder_name;
    return nullptr;
  }

  AMediaFormat* format = AMediaFormat_new();
  const char* mime = SupportedVideoCodecToMimeType(video_codec);
  AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mime);
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, frame_size_hint.width);
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT,
                        frame_size_hint.height);

  if (max_frame_size) {
    AMediaFormat_setInt32(format, "max-width", max_frame_size->width);
    AMediaFormat_setInt32(format, "max-height", max_frame_size->height);
  }
  if (max_video_input_size > 0) {
    AMediaFormat_setInt32(format, "max-input-size", max_video_input_size);
  }

  ANativeWindow* native_window = nullptr;
  if (j_surface) {
    native_window = ANativeWindow_fromSurface(env, j_surface);
  }

  media_status_t status = AMediaCodec_configure(
      codec, format, native_window, /*crypto=*/nullptr, /*flags=*/0);
  AMediaFormat_delete(format);

  if (status != AMEDIA_OK) {
    SB_LOG(ERROR) << "AMediaCodec_configure failed with status " << status;
    AMediaCodec_delete(codec);
    if (native_window) {
      ANativeWindow_release(native_window);
    }
    return nullptr;
  }

  auto bridge = std::unique_ptr<NdkMediaCodecBridge>(
      new NdkMediaCodecBridge(handler, codec));

  AMediaCodecOnAsyncNotifyCallback callbacks = {
      OnInputBufferAvailableCallback,
      OnOutputBufferAvailableCallback,
      OnFormatChangedCallback,
      OnErrorCallback,
  };
  status = AMediaCodec_setAsyncNotifyCallback(codec, callbacks, bridge.get());
  if (status != AMEDIA_OK) {
    SB_LOG(ERROR) << "AMediaCodec_setAsyncNotifyCallback failed with status "
                  << status;
    if (native_window) {
      ANativeWindow_release(native_window);
    }
    return nullptr;
  }

  status = AMediaCodec_start(codec);
  if (status != AMEDIA_OK) {
    SB_LOG(ERROR) << "AMediaCodec_start failed with status " << status;
    if (native_window) {
      ANativeWindow_release(native_window);
    }
    return nullptr;
  }

  if (native_window) {
    ANativeWindow_release(native_window);
  }

  SB_LOG(INFO) << "NdkMediaCodecBridge successfully started for "
               << decoder_name;
  return bridge;
}

NdkMediaCodecBridge::NdkMediaCodecBridge(Handler* handler, AMediaCodec* codec)
    : MediaCodecBridge(handler), codec_(codec) {}

NdkMediaCodecBridge::~NdkMediaCodecBridge() {
  if (codec_) {
    AMediaCodec_stop(codec_);
    AMediaCodec_delete(codec_);
  }
}

jni_zero::ScopedJavaLocalRef<jobject> NdkMediaCodecBridge::GetInputBuffer(
    jint index) {
  return jni_zero::ScopedJavaLocalRef<jobject>();
}

void* NdkMediaCodecBridge::GetInputBufferAddress(jint index, size_t* capacity) {
  return AMediaCodec_getInputBuffer(codec_, index, capacity);
}

jint NdkMediaCodecBridge::QueueInputBuffer(jint index,
                                           jint offset,
                                           jint size,
                                           jlong presentation_time_microseconds,
                                           jint flags,
                                           jboolean is_decode_only) {
  media_status_t status = AMediaCodec_queueInputBuffer(
      codec_, index, offset, size, presentation_time_microseconds, flags);
  return status == AMEDIA_OK ? MEDIA_CODEC_OK : MEDIA_CODEC_ERROR;
}

jint NdkMediaCodecBridge::QueueSecureInputBuffer(
    jint index,
    jint offset,
    const SbDrmSampleInfo& drm_sample_info,
    jlong presentation_time_microseconds,
    jboolean is_decode_only) {
  SB_NOTREACHED() << "NdkMediaCodecBridge does not support secure playback.";
  return MEDIA_CODEC_ERROR;
}

jni_zero::ScopedJavaLocalRef<jobject> NdkMediaCodecBridge::GetOutputBuffer(
    jint index) {
  return jni_zero::ScopedJavaLocalRef<jobject>();
}

void NdkMediaCodecBridge::ReleaseOutputBuffer(jint index, jboolean render) {
  AMediaCodec_releaseOutputBuffer(codec_, index, render);
}

void NdkMediaCodecBridge::ReleaseOutputBufferAtTimestamp(
    jint index,
    jlong render_timestamp_ns) {
  AMediaCodec_releaseOutputBufferAtTime(codec_, index, render_timestamp_ns);
}

void NdkMediaCodecBridge::SetPlaybackRate(double playback_rate) {
  SB_LOG(INFO) << "NdkMediaCodecBridge::SetPlaybackRate: " << playback_rate;
}

bool NdkMediaCodecBridge::Restart() {
  media_status_t status = AMediaCodec_start(codec_);
  return status == AMEDIA_OK;
}

jint NdkMediaCodecBridge::Flush() {
  media_status_t status = AMediaCodec_flush(codec_);
  return status == AMEDIA_OK ? MEDIA_CODEC_OK : MEDIA_CODEC_ERROR;
}

std::optional<FrameSize> NdkMediaCodecBridge::GetOutputSize() {
  AMediaFormat* format = AMediaCodec_getOutputFormat(codec_);
  if (!format) {
    return std::nullopt;
  }
  int32_t width = 0;
  int32_t height = 0;
  AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width);
  AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height);
  AMediaFormat_delete(format);
  return FrameSize(width, height, /*has_crop_values=*/false);
}

std::optional<AudioOutputFormatResult>
NdkMediaCodecBridge::GetAudioOutputFormat() {
  return std::nullopt;
}

void NdkMediaCodecBridge::OnInputBufferAvailable(int32_t index) {
  handler_->OnMediaCodecInputBufferAvailable(index);
}

void NdkMediaCodecBridge::OnOutputBufferAvailable(int32_t index,
                                                  AMediaCodecBufferInfo* info) {
  handler_->OnMediaCodecOutputBufferAvailable(
      index, info->flags, info->offset, info->presentationTimeUs, info->size);
}

void NdkMediaCodecBridge::OnFormatChanged(AMediaFormat* format) {
  handler_->OnMediaCodecOutputFormatChanged();
}

void NdkMediaCodecBridge::OnError(media_status_t error,
                                  int32_t actionCode,
                                  const char* detail) {
  handler_->OnMediaCodecError(/*is_recoverable=*/true, /*is_transient=*/false,
                              detail ? detail : "NDK MediaCodec Error");
}

}  // namespace starboard
