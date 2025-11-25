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
#include "starboard/common/media.h"
#include "starboard/common/string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/MediaCodecBridgeBuilder_jni.h"
#include "cobalt/android/jni_headers/MediaCodecBridge_jni.h"

namespace starboard {
namespace {

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;
using base::android::ToJavaIntArray;
using jni_zero::AttachCurrentThread;

// See
// https://developer.android.com/reference/android/media/MediaFormat.html#COLOR_RANGE_FULL.
const jint COLOR_RANGE_FULL = 1;
const jint COLOR_RANGE_LIMITED = 2;
// Not defined in MediaFormat. Represents unspecified color ID range.
const jint COLOR_RANGE_UNSPECIFIED = 0;

const jint COLOR_STANDARD_BT2020 = 6;
const jint COLOR_STANDARD_BT709 = 1;

const jint COLOR_TRANSFER_HLG = 7;
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

std::ostream& operator<<(std::ostream& os, const FrameSize& size) {
  return os << "{display_size=" << size.display_size
            << ", has_crop_values=" << to_string(size.has_crop_values) << "}";
}

FrameSize::FrameSize()
    : FrameSize(/*width=*/0, /*height=*/0, /*has_crop_values=*/false) {}

FrameSize::FrameSize(int width, int height, bool has_crop_values)
    : display_size({width, height}), has_crop_values(has_crop_values) {
  SB_CHECK_GE(this->display_size.width, 0);
  SB_CHECK_GE(this->display_size.height, 0);
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

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> configuration_data;
  if (audio_stream_info.codec == kSbMediaAudioCodecOpus &&
      !audio_stream_info.audio_specific_config.empty()) {
    configuration_data.Reset(
        ToJavaByteArray(env, audio_stream_info.audio_specific_config.data(),
                        audio_stream_info.audio_specific_config.size()));
  }

  std::unique_ptr<MediaCodecBridge> native_media_codec_bridge(
      new MediaCodecBridge(handler));
  ScopedJavaLocalRef<jstring> j_mime(env, env->NewStringUTF(mime));
  ScopedJavaLocalRef<jstring> j_decoder_name(
      env, env->NewStringUTF(decoder_name.c_str()));
  ScopedJavaLocalRef<jobject> j_media_crypto_local(
      env, env->NewLocalRef(j_media_crypto));

  ScopedJavaLocalRef<jobject> j_media_codec_bridge =
      Java_MediaCodecBridgeBuilder_createAudioDecoder(
          env, reinterpret_cast<jlong>(native_media_codec_bridge.get()), j_mime,
          j_decoder_name, audio_stream_info.samples_per_second,
          audio_stream_info.number_of_channels, j_media_crypto_local,
          configuration_data);

  if (!j_media_codec_bridge) {
    SB_LOG(ERROR) << "Failed to create codec bridge for "
                  << audio_stream_info.codec << ".";
    return nullptr;
  }

  SB_LOG(INFO) << __func__ << ": audio_stream_info=" << audio_stream_info;

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

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_mime(env, env->NewStringUTF(mime));
  ScopedJavaLocalRef<jstring> j_decoder_name(
      env, env->NewStringUTF(decoder_name.c_str()));

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
      j_color_info.Reset(Java_ColorInfo_Constructor(
          env, color_range, color_standard, color_transfer,
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
      Java_CreateMediaCodecBridgeResult_Constructor(env));

  std::unique_ptr<MediaCodecBridge> native_media_codec_bridge(
      new MediaCodecBridge(handler));
  ScopedJavaLocalRef<jobject> j_surface_local(env, env->NewLocalRef(j_surface));
  ScopedJavaLocalRef<jobject> j_media_crypto_local(
      env, env->NewLocalRef(j_media_crypto));

  Java_MediaCodecBridge_createVideoMediaCodecBridge(
      env, reinterpret_cast<jlong>(native_media_codec_bridge.get()), j_mime,
      j_decoder_name, width_hint, height_hint, fps, max_width.value_or(-1),
      max_height.value_or(-1), j_surface_local, j_media_crypto_local,
      j_color_info, tunnel_mode_audio_session_id, max_video_input_size,
      j_create_media_codec_bridge_result);

  ScopedJavaLocalRef<jobject> j_media_codec_bridge(
      Java_CreateMediaCodecBridgeResult_mediaCodecBridge(
          env, j_create_media_codec_bridge_result));

  if (!j_media_codec_bridge) {
    ScopedJavaLocalRef<jstring> j_error_message(
        Java_CreateMediaCodecBridgeResult_errorMessage(
            env, j_create_media_codec_bridge_result));
    *error_message = ConvertJavaStringToUTF8(env, j_error_message);
    return nullptr;
  }

  SB_LOG(INFO)
      << __func__ << ": video_codec=" << GetMediaVideoCodecName(video_codec)
      << ", width_hint=" << width_hint << ", height_hint=" << height_hint
      << ", fps=" << fps << ", max_width=" << max_width
      << ", max_height=" << max_height
      << ", has_color_metadata=" << to_string(color_metadata)
      << ", require_secured_decoder=" << to_string(require_secured_decoder)
      << ", require_software_codec=" << to_string(require_software_codec)
      << ", tunnel_mode_audio_session_id=" << tunnel_mode_audio_session_id
      << ", force_big_endian_hdr_metadata="
      << to_string(force_big_endian_hdr_metadata)
      << ", max_video_input_size=" << max_video_input_size;

  native_media_codec_bridge->Initialize(j_media_codec_bridge.obj());
  return native_media_codec_bridge;
}

MediaCodecBridge::~MediaCodecBridge() {
  if (!j_media_codec_bridge_) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_stop(env, j_media_codec_bridge_);
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
  ScopedJavaLocalRef<jintArray> j_clear_bytes = ToJavaIntArray(
      env, base::span<const jint>(clear_bytes.get(),
                                  static_cast<size_t>(subsample_count)));
  ScopedJavaLocalRef<jintArray> j_encrypted_bytes = ToJavaIntArray(
      env, base::span<const jint>(encrypted_bytes.get(),
                                  static_cast<size_t>(subsample_count)));

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
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_stop(env, j_media_codec_bridge_);
}

std::optional<FrameSize> MediaCodecBridge::GetOutputSize() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result(
      Java_MediaCodecBridge_getOutputFormat(env, j_media_codec_bridge_));
  if (!result) {
    return std::nullopt;
  }

  return FrameSize(Java_MediaFormatWrapper_width(env, result),
                   Java_MediaFormatWrapper_height(env, result),
                   Java_MediaFormatWrapper_formatHasCropValues(env, result));
}

std::optional<AudioOutputFormatResult>
MediaCodecBridge::GetAudioOutputFormat() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result(
      Java_MediaCodecBridge_getOutputFormat(env, j_media_codec_bridge_));
  if (!result) {
    return std::nullopt;
  }

  return AudioOutputFormatResult{
      Java_MediaFormatWrapper_sampleRate(env, result),
      Java_MediaFormatWrapper_channelCount(env, result),
  };
}

void MediaCodecBridge::OnMediaCodecError(
    JNIEnv* env,
    jboolean is_recoverable,
    jboolean is_transient,
    const base::android::JavaParamRef<jstring>& diagnostic_info) {
  std::string diagnostic_info_in_str =
      ConvertJavaStringToUTF8(env, diagnostic_info);
  handler_->OnMediaCodecError(is_recoverable, is_transient,
                              diagnostic_info_in_str);
}

void MediaCodecBridge::OnMediaCodecInputBufferAvailable(JNIEnv* env,
                                                        jint buffer_index) {
  handler_->OnMediaCodecInputBufferAvailable(buffer_index);
}

void MediaCodecBridge::OnMediaCodecOutputBufferAvailable(
    JNIEnv* env,
    jint buffer_index,
    jint flags,
    jint offset,
    jlong presentation_time_us,
    jint size) {
  handler_->OnMediaCodecOutputBufferAvailable(buffer_index, flags, offset,
                                              presentation_time_us, size);
}

void MediaCodecBridge::OnMediaCodecOutputFormatChanged(JNIEnv* env) {
  handler_->OnMediaCodecOutputFormatChanged();
}

void MediaCodecBridge::OnMediaCodecFrameRendered(
    JNIEnv* env,
    jlong presentation_time_us,
    jlong render_at_system_time_ns) {
  handler_->OnMediaCodecFrameRendered(presentation_time_us);
}

void MediaCodecBridge::OnMediaCodecFirstTunnelFrameReady(JNIEnv* env) {
  handler_->OnMediaCodecFirstTunnelFrameReady();
}

MediaCodecBridge::MediaCodecBridge(Handler* handler) : handler_(handler) {
  SB_CHECK(handler_);
}

void MediaCodecBridge::Initialize(jobject j_media_codec_bridge) {
  SB_DCHECK(j_media_codec_bridge);

  JNIEnv* env = AttachCurrentThread();
  j_media_codec_bridge_.Reset(env, j_media_codec_bridge);
}

// static
jboolean MediaCodecBridge::IsFrameRenderedCallbackEnabled() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_isFrameRenderedCallbackEnabled(env);
}

}  // namespace starboard
