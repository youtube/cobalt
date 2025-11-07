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

#include "starboard/android/shared/media_codec_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/android/shared/media_codec_bridge_eradicator.h"
#include "starboard/common/string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/MediaCodecBridge_jni.h"

namespace starboard::android::shared {

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;
using base::android::ToJavaIntArray;

namespace {

// See
// https://developer.android.com/reference/android/media/MediaFormat.html#COLOR_RANGE_FULL.
const jint COLOR_RANGE_FULL = 1;
const jint COLOR_RANGE_LIMITED = 2;
// Not defined in MediaFormat. Represents unspecified color ID range.
const jint COLOR_RANGE_UNSPECIFIED = 0;

const jint COLOR_STANDARD_BT2020 = 6;
const jint COLOR_STANDARD_BT601_NTSC = 4;
const jint COLOR_STANDARD_BT601_PAL = 2;
const jint COLOR_STANDARD_BT709 = 1;

const jint COLOR_TRANSFER_HLG = 7;
const jint COLOR_TRANSFER_LINEAR = 1;
const jint COLOR_TRANSFER_SDR_VIDEO = 3;
const jint COLOR_TRANSFER_ST2084 = 6;

// A special value to represent that no mapping between an SbMedia* HDR
// metadata value and Android HDR metadata value is possible.  This value
// implies that HDR playback should not be attempted.
const jint COLOR_VALUE_UNKNOWN = -1;

jint SbMediaPrimaryIdToColorStandard(SbMediaPrimaryId primary_id) {
  switch (primary_id) {
    case kSbMediaPrimaryIdBt709:
      return COLOR_STANDARD_BT709;
    case kSbMediaPrimaryIdBt2020:
      return COLOR_STANDARD_BT2020;
    default:
      return COLOR_VALUE_UNKNOWN;
  }
}

jint SbMediaTransferIdToColorTransfer(SbMediaTransferId transfer_id) {
  switch (transfer_id) {
    case kSbMediaTransferIdBt709:
      return COLOR_TRANSFER_SDR_VIDEO;
    case kSbMediaTransferIdSmpteSt2084:
      return COLOR_TRANSFER_ST2084;
    case kSbMediaTransferIdAribStdB67:
      return COLOR_TRANSFER_HLG;
    default:
      return COLOR_VALUE_UNKNOWN;
  }
}

jint SbMediaRangeIdToColorRange(SbMediaRangeId range_id) {
  switch (range_id) {
    case kSbMediaRangeIdLimited:
      return COLOR_RANGE_LIMITED;
    case kSbMediaRangeIdFull:
      return COLOR_RANGE_FULL;
    case kSbMediaRangeIdUnspecified:
      return COLOR_RANGE_UNSPECIFIED;
    default:
      return COLOR_VALUE_UNKNOWN;
  }
}

}  // namespace

extern "C" SB_EXPORT_PLATFORM void
JNI_MediaCodecBridge_OnMediaCodecFrameRendered(JNIEnv* env,
                                               jlong native_media_codec_bridge,
                                               jlong presentation_time_us,
                                               jlong render_at_system_time_ns) {
  MediaCodecBridge* media_codec_bridge =
      reinterpret_cast<MediaCodecBridge*>(native_media_codec_bridge);
  SB_DCHECK(media_codec_bridge);
  media_codec_bridge->OnMediaCodecFrameRendered(
      presentation_time_us, render_at_system_time_ns / 1000);
}

extern "C" SB_EXPORT_PLATFORM void
JNI_MediaCodecBridge_OnMediaCodecFirstTunnelFrameReady(
    JNIEnv* env,
    jlong native_media_codec_bridge) {
  MediaCodecBridge* media_codec_bridge =
      reinterpret_cast<MediaCodecBridge*>(native_media_codec_bridge);
  SB_DCHECK(media_codec_bridge);
  media_codec_bridge->OnMediaCodecFirstTunnelFrameReady();
}

extern "C" SB_EXPORT_PLATFORM void JNI_MediaCodecBridge_OnMediaCodecError(
    JNIEnv* env,
    jlong native_media_codec_bridge,
    jboolean is_recoverable,
    jboolean is_transient,
    const JavaParamRef<jstring>& diagnostic_info) {
  MediaCodecBridge* media_codec_bridge =
      reinterpret_cast<MediaCodecBridge*>(native_media_codec_bridge);
  SB_DCHECK(media_codec_bridge);
  std::string diagnostic_info_in_str =
      ConvertJavaStringToUTF8(env, diagnostic_info);
  media_codec_bridge->OnMediaCodecError(is_recoverable, is_transient,
                                        diagnostic_info_in_str);
}

extern "C" SB_EXPORT_PLATFORM void
JNI_MediaCodecBridge_OnMediaCodecInputBufferAvailable(
    JNIEnv* env,
    jlong native_media_codec_bridge,
    jint buffer_index) {
  MediaCodecBridge* media_codec_bridge =
      reinterpret_cast<MediaCodecBridge*>(native_media_codec_bridge);
  SB_DCHECK(media_codec_bridge);
  media_codec_bridge->OnMediaCodecInputBufferAvailable(buffer_index);
}

extern "C" SB_EXPORT_PLATFORM void
JNI_MediaCodecBridge_OnMediaCodecOutputBufferAvailable(
    JNIEnv* env,
    jlong native_media_codec_bridge,
    jint buffer_index,
    jint flags,
    jint offset,
    jlong presentation_time_us,
    jint size) {
  MediaCodecBridge* media_codec_bridge =
      reinterpret_cast<MediaCodecBridge*>(native_media_codec_bridge);
  SB_DCHECK(media_codec_bridge);
  media_codec_bridge->OnMediaCodecOutputBufferAvailable(
      buffer_index, flags, offset, presentation_time_us, size);
}

extern "C" SB_EXPORT_PLATFORM void
JNI_MediaCodecBridge_OnMediaCodecOutputFormatChanged(
    JNIEnv* env,
    jlong native_media_codec_bridge) {
  MediaCodecBridge* media_codec_bridge =
      reinterpret_cast<MediaCodecBridge*>(native_media_codec_bridge);
  SB_DCHECK(media_codec_bridge);
  media_codec_bridge->OnMediaCodecOutputFormatChanged();
}

// static
std::unique_ptr<MediaCodecBridge> MediaCodecBridge::CreateAudioMediaCodecBridge(
    const AudioStreamInfo& audio_stream_info,
    Handler* handler,
    jobject j_media_crypto) {
  bool is_passthrough = false;
  const char* mime =
      SupportedAudioCodecToMimeType(audio_stream_info.codec, &is_passthrough);
  if (!mime) {
    SB_LOG(ERROR) << "Unsupported codec " << audio_stream_info.codec << ".";
    return nullptr;
  }

  std::string decoder_name =
      MediaCapabilitiesCache::GetInstance()->FindAudioDecoder(
          mime, /* bitrate = */ 0);

  if (decoder_name.empty()) {
    SB_LOG(ERROR) << "Failed to find decoder for " << audio_stream_info.codec
                  << ".";
    return nullptr;
  }

  if (MediaCodecBridgeEradicator::GetInstance()->IsEnabled()) {
    // block if the old MediaCodecBridge instances haven't been destroyed yet
    bool destruction_finished =
        MediaCodecBridgeEradicator::GetInstance()->WaitForPendingDestructions();
    if (!destruction_finished) {
      // timed out
      std::string diagnostic_info_in_str = FormatString(
          "MediaCodec destruction timeout: %d seconds, potential thread "
          "leakage happened. Type = Audio",
          MediaCodecBridgeEradicator::GetInstance()->GetTimeoutSeconds());
      handler->OnMediaCodecError(false, false, diagnostic_info_in_str);
      return std::unique_ptr<MediaCodecBridge>();
    }
  }

  // TODO(cobalt): remove all the JniEnvExt references when we can, after all
  // JNI usages are migrated to jni_zero in this file.
  JniEnvExt* env = JniEnvExt::Get();

  JNIEnv* env_jni = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> configuration_data;
  if (audio_stream_info.codec == kSbMediaAudioCodecOpus &&
      !audio_stream_info.audio_specific_config.empty()) {
    configuration_data.Reset(
        ToJavaByteArray(env, audio_stream_info.audio_specific_config.data(),
                        audio_stream_info.audio_specific_config.size()));
  }
  ScopedJavaLocalRef<jstring> j_mime(env_jni, env_jni->NewStringUTF(mime));
  ScopedJavaLocalRef<jstring> j_decoder_name(
      env_jni, env_jni->NewStringUTF(decoder_name.c_str()));
  std::unique_ptr<MediaCodecBridge> native_media_codec_bridge(
      new MediaCodecBridge(handler));
  ScopedJavaLocalRef<jobject> j_media_codec_bridge(
      env_jni,
      static_cast<jobject>(env->CallStaticObjectMethodOrAbort(
          "dev/cobalt/media/MediaCodecBridgeBuilder", "createAudioDecoder",
          "(JLjava/lang/String;Ljava/lang/String;IILandroid/media/"
          "MediaCrypto;"
          "[B)Ldev/cobalt/media/MediaCodecBridge;",
          reinterpret_cast<jlong>(native_media_codec_bridge.get()),
          j_mime.obj(), j_decoder_name.obj(),
          audio_stream_info.samples_per_second,
          audio_stream_info.number_of_channels, j_media_crypto,
          configuration_data.obj())));

  if (!j_media_codec_bridge) {
    SB_LOG(ERROR) << "Failed to create codec bridge for "
                  << audio_stream_info.codec << ".";
    return nullptr;
  }

  native_media_codec_bridge->Initialize(j_media_codec_bridge.obj());
  return native_media_codec_bridge;
}

// static
std::unique_ptr<MediaCodecBridge> MediaCodecBridge::CreateVideoMediaCodecBridge(
    SbMediaVideoCodec video_codec,
    int width_hint,
    int height_hint,
    int fps,
    std::optional<int> max_width,
    std::optional<int> max_height,
    Handler* handler,
    jobject j_surface,
    jobject j_media_crypto,
    const SbMediaColorMetadata* color_metadata,
    bool require_secured_decoder,
    bool require_software_codec,
    int tunnel_mode_audio_session_id,
    bool force_big_endian_hdr_metadata,
    int max_video_input_size,
    std::string* error_message) {
  SB_DCHECK(error_message);
  SB_DCHECK_EQ(max_width.has_value(), max_height.has_value());
  SB_DCHECK_GT(max_width.value_or(1920), 0);
  SB_DCHECK_GT(max_height.value_or(1080), 0);

  const char* mime = SupportedVideoCodecToMimeType(video_codec);
  if (!mime) {
    *error_message = FormatString("Unsupported mime for codec %d", video_codec);
    return nullptr;
  }

  const bool must_support_secure = require_secured_decoder;
  const bool must_support_hdr = color_metadata;
  const bool must_support_tunnel_mode = tunnel_mode_audio_session_id != -1;
  // On first pass, try to find a decoder with HDR if the color info is
  // non-null.
  std::string decoder_name =
      MediaCapabilitiesCache::GetInstance()->FindVideoDecoder(
          mime, must_support_secure, must_support_hdr, require_software_codec,
          must_support_tunnel_mode,
          /* frame_width = */ 0,
          /* frame_height = */ 0,
          /* bitrate = */ 0,
          /* fps = */ 0);
  if (decoder_name.empty() && color_metadata) {
    // On second pass, forget HDR.
    decoder_name = MediaCapabilitiesCache::GetInstance()->FindVideoDecoder(
        mime, must_support_secure, /* must_support_hdr = */ false,
        require_software_codec, must_support_tunnel_mode,
        /* frame_width = */ 0,
        /* frame_height = */ 0,
        /* bitrate = */ 0,
        /* fps = */ 0);
  }
  if (decoder_name.empty() && require_software_codec) {
    // On third pass, forget software codec required.
    decoder_name = MediaCapabilitiesCache::GetInstance()->FindVideoDecoder(
        mime, must_support_secure, /* must_support_hdr = */ false,
        /* require_software_codec = */ false, must_support_tunnel_mode,
        /* frame_width = */ 0,
        /* frame_height = */ 0,
        /* bitrate = */ 0,
        /* fps = */ 0);
  }

  if (decoder_name.empty()) {
    *error_message =
        FormatString("Failed to find decoder: %s, mustSupportSecure: %d.", mime,
                     !!j_media_crypto);
    return nullptr;
  }

  if (MediaCodecBridgeEradicator::GetInstance()->IsEnabled()) {
    // block if the old MediaCodecBridge instances haven't been destroyed yet
    bool destruction_finished =
        MediaCodecBridgeEradicator::GetInstance()->WaitForPendingDestructions();
    if (!destruction_finished) {
      // timed out
      *error_message = FormatString(
          "MediaCodec destruction timeout: %d seconds, potential thread "
          "leakage happened. Type = Video",
          MediaCodecBridgeEradicator::GetInstance()->GetTimeoutSeconds());
      return std::unique_ptr<MediaCodecBridge>();
    }
  }

  JniEnvExt* env = JniEnvExt::Get();

  JNIEnv* env_jni = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime(env_jni, env_jni->NewStringUTF(mime));
  ScopedJavaLocalRef<jstring> j_decoder_name(
      env_jni, env_jni->NewStringUTF(decoder_name.c_str()));

  ScopedJavaLocalRef<jobject> j_color_info(nullptr);
  if (color_metadata) {
    jint color_standard =
        SbMediaPrimaryIdToColorStandard(color_metadata->primaries);
    jint color_transfer =
        SbMediaTransferIdToColorTransfer(color_metadata->transfer);
    jint color_range = SbMediaRangeIdToColorRange(color_metadata->range);

    if (color_standard != COLOR_VALUE_UNKNOWN &&
        color_transfer != COLOR_VALUE_UNKNOWN &&
        color_range != COLOR_VALUE_UNKNOWN) {
      const auto& mastering_metadata = color_metadata->mastering_metadata;
      JNIEnv* env_jni = base::android::AttachCurrentThread();
      j_color_info.Reset(Java_ColorInfo_Constructor(
          env_jni, color_range, color_standard, color_transfer,
          mastering_metadata.primary_r_chromaticity_x,
          mastering_metadata.primary_r_chromaticity_y,
          mastering_metadata.primary_g_chromaticity_x,
          mastering_metadata.primary_g_chromaticity_y,
          mastering_metadata.primary_b_chromaticity_x,
          mastering_metadata.primary_b_chromaticity_y,
          mastering_metadata.white_point_chromaticity_x,
          mastering_metadata.white_point_chromaticity_y,
          mastering_metadata.luminance_max, mastering_metadata.luminance_min,
          color_metadata->max_cll, color_metadata->max_fall,
          force_big_endian_hdr_metadata));
    }
  }

  ScopedJavaLocalRef<jobject> j_create_media_codec_bridge_result(
      Java_CreateMediaCodecBridgeResult_Constructor(env_jni));

  std::unique_ptr<MediaCodecBridge> native_media_codec_bridge(
      new MediaCodecBridge(handler));
  env->CallStaticVoidMethodOrAbort(
      "dev/cobalt/media/MediaCodecBridge", "createVideoMediaCodecBridge",
      "(JLjava/lang/String;Ljava/lang/String;IIIIILandroid/view/Surface;"
      "Landroid/media/MediaCrypto;"
      "Ldev/cobalt/media/MediaCodecBridge$ColorInfo;"
      "II"
      "Ldev/cobalt/media/MediaCodecBridge$CreateMediaCodecBridgeResult;)"
      "V",
      reinterpret_cast<jlong>(native_media_codec_bridge.get()), j_mime.obj(),
      j_decoder_name.obj(), width_hint, height_hint, fps,
      max_width.value_or(-1), max_height.value_or(-1), j_surface,
      j_media_crypto, j_color_info.obj(), tunnel_mode_audio_session_id,
      max_video_input_size, j_create_media_codec_bridge_result.obj());

  ScopedJavaLocalRef<jobject> j_media_codec_bridge(
      env_jni, static_cast<jobject>(env->CallObjectMethodOrAbort(
                   j_create_media_codec_bridge_result.obj(), "mediaCodecBridge",
                   "()Ldev/cobalt/media/MediaCodecBridge;")));

  if (!j_media_codec_bridge) {
    ScopedJavaLocalRef<jstring> j_error_message(
        Java_CreateMediaCodecBridgeResult_errorMessage(
            env_jni, j_create_media_codec_bridge_result));
    *error_message = ConvertJavaStringToUTF8(env_jni, j_error_message);
    return nullptr;
  }

  native_media_codec_bridge->Initialize(j_media_codec_bridge.obj());
  return native_media_codec_bridge;
}

MediaCodecBridge::~MediaCodecBridge() {
  if (!j_media_codec_bridge_) {
    return;
  }

  if (MediaCodecBridgeEradicator::GetInstance()->IsEnabled()) {
    if (MediaCodecBridgeEradicator::GetInstance()->Destroy(
            j_media_codec_bridge_.obj())) {
      return;
    }
    SB_LOG(WARNING)
        << "MediaCodecBridge destructor fallback into none eradicator mode.";
  }

  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_release(env, j_media_codec_bridge_);
}

ScopedJavaLocalRef<jobject> MediaCodecBridge::GetInputBuffer(jint index) {
  SB_DCHECK_GE(index, 0);
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_getInputBuffer(env, j_media_codec_bridge_,
                                              index);
}

jint MediaCodecBridge::QueueInputBuffer(jint index,
                                        jint offset,
                                        jint size,
                                        jlong presentation_time_microseconds,
                                        jint flags,
                                        jboolean is_decode_only) {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_queueInputBuffer(
      env, j_media_codec_bridge_, index, offset, size,
      presentation_time_microseconds, flags, is_decode_only);
}

jint MediaCodecBridge::QueueSecureInputBuffer(
    jint index,
    jint offset,
    const SbDrmSampleInfo& drm_sample_info,
    jlong presentation_time_microseconds,
    jboolean is_decode_only) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> j_iv =
      ToJavaByteArray(env, drm_sample_info.initialization_vector,
                      drm_sample_info.initialization_vector_size);
  ScopedJavaLocalRef<jbyteArray> j_key_id = ToJavaByteArray(
      env, drm_sample_info.identifier, drm_sample_info.identifier_size);

  // Reshape the sub sample mapping like this:
  // [(c0, e0), (c1, e1), ...] -> [c0, c1, ...] and [e0, e1, ...]
  int32_t subsample_count = drm_sample_info.subsample_count;
  std::unique_ptr<jint[]> clear_bytes(new jint[subsample_count]);
  std::unique_ptr<jint[]> encrypted_bytes(new jint[subsample_count]);
  for (int i = 0; i < subsample_count; ++i) {
    clear_bytes[i] = drm_sample_info.subsample_mapping[i].clear_byte_count;
    encrypted_bytes[i] =
        drm_sample_info.subsample_mapping[i].encrypted_byte_count;
  }
  ScopedJavaLocalRef<jintArray> j_clear_bytes =
      ToJavaIntArray(env, clear_bytes.get(), subsample_count);
  ScopedJavaLocalRef<jintArray> j_encrypted_bytes =
      ToJavaIntArray(env, encrypted_bytes.get(), subsample_count);

  jint cipher_mode = CRYPTO_MODE_AES_CTR;
  jint blocks_to_encrypt = 0;
  jint blocks_to_skip = 0;
  if (drm_sample_info.encryption_scheme == kSbDrmEncryptionSchemeAesCbc) {
    cipher_mode = CRYPTO_MODE_AES_CBC;
    blocks_to_encrypt = drm_sample_info.encryption_pattern.crypt_byte_block;
    blocks_to_skip = drm_sample_info.encryption_pattern.skip_byte_block;
  }

  return Java_MediaCodecBridge_queueSecureInputBuffer(
      env, j_media_codec_bridge_, index, offset, j_iv, j_key_id, j_clear_bytes,
      j_encrypted_bytes, subsample_count, cipher_mode, blocks_to_encrypt,
      blocks_to_skip, presentation_time_microseconds, is_decode_only);
}

ScopedJavaLocalRef<jobject> MediaCodecBridge::GetOutputBuffer(jint index) {
  SB_DCHECK_GE(index, 0);
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_getOutputBuffer(env, j_media_codec_bridge_,
                                               index);
}

void MediaCodecBridge::ReleaseOutputBuffer(jint index, jboolean render) {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_releaseOutputBuffer(env, j_media_codec_bridge_, index,
                                            render);
}

void MediaCodecBridge::ReleaseOutputBufferAtTimestamp(
    jint index,
    jlong render_timestamp_ns) {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_releaseOutputBufferAtTimestamp(
      env, j_media_codec_bridge_, index, render_timestamp_ns);
}

void MediaCodecBridge::SetPlaybackRate(double playback_rate) {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_setPlaybackRate(env, j_media_codec_bridge_,
                                        playback_rate);
}

bool MediaCodecBridge::Restart() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_restart(env, j_media_codec_bridge_) == JNI_TRUE;
}

jint MediaCodecBridge::Flush() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_flush(env, j_media_codec_bridge_);
}

void MediaCodecBridge::Stop() {
  JniEnvExt::Get()->CallVoidMethodOrAbort(j_media_codec_bridge_.obj(), "stop",
                                          "()V");
}

FrameSize MediaCodecBridge::GetOutputSize() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_get_output_format_result(
      Java_GetOutputFormatResult_Constructor(env));
  Java_MediaCodecBridge_getOutputFormat(env, j_media_codec_bridge_,
                                        j_get_output_format_result);

  jint status =
      Java_GetOutputFormatResult_status(env, j_get_output_format_result);
  if (status == MEDIA_CODEC_ERROR) {
    return {};
  }

  jint textureWidth =
      Java_GetOutputFormatResult_textureWidth(env, j_get_output_format_result);
  jint textureHeight =
      Java_GetOutputFormatResult_textureHeight(env, j_get_output_format_result);
  jint cropLeft =
      Java_GetOutputFormatResult_cropLeft(env, j_get_output_format_result);
  jint cropTop =
      Java_GetOutputFormatResult_cropTop(env, j_get_output_format_result);
  jint cropRight =
      Java_GetOutputFormatResult_cropRight(env, j_get_output_format_result);
  jint cropBottom =
      Java_GetOutputFormatResult_cropBottom(env, j_get_output_format_result);

  FrameSize size = {textureWidth, textureHeight, cropLeft,
                    cropTop,      cropRight,     cropBottom};

  size.DCheckValid();
  return size;
}

AudioOutputFormatResult MediaCodecBridge::GetAudioOutputFormat() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_get_output_format_result(
      Java_GetOutputFormatResult_Constructor(env));
  Java_MediaCodecBridge_getOutputFormat(env, j_media_codec_bridge_,
                                        j_get_output_format_result);

  jint status =
      Java_GetOutputFormatResult_status(env, j_get_output_format_result);

  if (status == MEDIA_CODEC_ERROR) {
    return {status, 0, 0};
  }

  jint sample_rate =
      Java_GetOutputFormatResult_sampleRate(env, j_get_output_format_result);
  jint channel_count =
      Java_GetOutputFormatResult_channelCount(env, j_get_output_format_result);

  return {status, sample_rate, channel_count};
}

void MediaCodecBridge::OnMediaCodecError(bool is_recoverable,
                                         bool is_transient,
                                         const std::string& diagnostic_info) {
  handler_->OnMediaCodecError(is_recoverable, is_transient, diagnostic_info);
}

void MediaCodecBridge::OnMediaCodecInputBufferAvailable(int buffer_index) {
  handler_->OnMediaCodecInputBufferAvailable(buffer_index);
}

void MediaCodecBridge::OnMediaCodecOutputBufferAvailable(
    int buffer_index,
    int flags,
    int offset,
    int64_t presentation_time_us,
    int size) {
  handler_->OnMediaCodecOutputBufferAvailable(buffer_index, flags, offset,
                                              presentation_time_us, size);
}

void MediaCodecBridge::OnMediaCodecOutputFormatChanged() {
  handler_->OnMediaCodecOutputFormatChanged();
}

void MediaCodecBridge::OnMediaCodecFrameRendered(int64_t frame_timestamp,
                                                 int64_t frame_rendered_us) {
  handler_->OnMediaCodecFrameRendered(frame_timestamp, frame_rendered_us);
}

void MediaCodecBridge::OnMediaCodecFirstTunnelFrameReady() {
  handler_->OnMediaCodecFirstTunnelFrameReady();
}

MediaCodecBridge::MediaCodecBridge(Handler* handler) : handler_(handler) {
  SB_CHECK(handler_);
}

void MediaCodecBridge::Initialize(jobject j_media_codec_bridge) {
  SB_DCHECK(j_media_codec_bridge);

  JNIEnv* env_jni = AttachCurrentThread();
  j_media_codec_bridge_.Reset(env_jni, j_media_codec_bridge);
}

// static
jboolean MediaCodecBridge::IsFrameRenderedCallbackEnabled() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_isFrameRenderedCallbackEnabled(env);
}

}  // namespace starboard::android::shared
