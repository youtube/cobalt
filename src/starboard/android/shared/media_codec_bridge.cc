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

#include "starboard/common/string.h"

namespace starboard {
namespace android {
namespace shared {

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
Java_dev_cobalt_media_MediaCodecBridge_nativeOnMediaCodecFrameRendered(
    JNIEnv* env,
    jobject unused_this,
    jlong native_media_codec_bridge,
    jlong presentation_time_us,
    jlong render_at_system_time_ns) {
  MediaCodecBridge* media_codec_bridge =
      reinterpret_cast<MediaCodecBridge*>(native_media_codec_bridge);
  SB_DCHECK(media_codec_bridge);
  media_codec_bridge->OnMediaCodecFrameRendered(presentation_time_us);
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_MediaCodecBridge_nativeOnMediaCodecError(
    JniEnvExt* env,
    jobject unused_this,
    jlong native_media_codec_bridge,
    jboolean is_recoverable,
    jboolean is_transient,
    jstring diagnostic_info) {
  MediaCodecBridge* media_codec_bridge =
      reinterpret_cast<MediaCodecBridge*>(native_media_codec_bridge);
  SB_DCHECK(media_codec_bridge);
  std::string diagnostic_info_in_str =
      env->GetStringStandardUTFOrAbort(diagnostic_info);
  media_codec_bridge->OnMediaCodecError(is_recoverable, is_transient,
                                        diagnostic_info_in_str);
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_MediaCodecBridge_nativeOnMediaCodecInputBufferAvailable(
    JNIEnv* env,
    jobject unused_this,
    jlong native_media_codec_bridge,
    jint buffer_index) {
  MediaCodecBridge* media_codec_bridge =
      reinterpret_cast<MediaCodecBridge*>(native_media_codec_bridge);
  SB_DCHECK(media_codec_bridge);
  media_codec_bridge->OnMediaCodecInputBufferAvailable(buffer_index);
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_MediaCodecBridge_nativeOnMediaCodecOutputBufferAvailable(
    JNIEnv* env,
    jobject unused_this,
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
Java_dev_cobalt_media_MediaCodecBridge_nativeOnMediaCodecOutputFormatChanged(
    JNIEnv* env,
    jobject unused_this,
    jlong native_media_codec_bridge) {
  MediaCodecBridge* media_codec_bridge =
      reinterpret_cast<MediaCodecBridge*>(native_media_codec_bridge);
  SB_DCHECK(media_codec_bridge);
  media_codec_bridge->OnMediaCodecOutputFormatChanged();
}

// static
scoped_ptr<MediaCodecBridge> MediaCodecBridge::CreateAudioMediaCodecBridge(
    SbMediaAudioCodec audio_codec,
    const SbMediaAudioSampleInfo& audio_sample_info,
    Handler* handler,
    jobject j_media_crypto) {
  bool is_passthrough = false;
  const char* mime =
      SupportedAudioCodecToMimeType(audio_codec, &is_passthrough);
  if (!mime) {
    return scoped_ptr<MediaCodecBridge>(NULL);
  }
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jstring> j_mime(env->NewStringStandardUTFOrAbort(mime));
  scoped_ptr<MediaCodecBridge> native_media_codec_bridge(
      new MediaCodecBridge(handler));
  jobject j_media_codec_bridge = env->CallStaticObjectMethodOrAbort(
      "dev/cobalt/media/MediaCodecBridge", "createAudioMediaCodecBridge",
      "(JLjava/lang/String;ZZIILandroid/media/MediaCrypto;)Ldev/cobalt/media/"
      "MediaCodecBridge;",
      reinterpret_cast<jlong>(native_media_codec_bridge.get()), j_mime.Get(),
      !!j_media_crypto, false, audio_sample_info.samples_per_second,
      audio_sample_info.number_of_channels, j_media_crypto);

  if (!j_media_codec_bridge) {
    return scoped_ptr<MediaCodecBridge>(NULL);
  }

  j_media_codec_bridge = env->ConvertLocalRefToGlobalRef(j_media_codec_bridge);
  native_media_codec_bridge->Initialize(j_media_codec_bridge);
  return native_media_codec_bridge.Pass();
}

// static
scoped_ptr<MediaCodecBridge> MediaCodecBridge::CreateVideoMediaCodecBridge(
    SbMediaVideoCodec video_codec,
    int width,
    int height,
    int fps,
    Handler* handler,
    jobject j_surface,
    jobject j_media_crypto,
    const SbMediaColorMetadata* color_metadata,
    bool require_software_codec,
    int tunnel_mode_audio_session_id,
    bool force_big_endian_hdr_metadata,
    std::string* error_message) {
  SB_DCHECK(error_message);

  const char* mime = SupportedVideoCodecToMimeType(video_codec);
  if (!mime) {
    *error_message = FormatString("Unsupported mime for codec %d", video_codec);
    return scoped_ptr<MediaCodecBridge>(NULL);
  }
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jstring> j_mime(env->NewStringStandardUTFOrAbort(mime));

  ScopedLocalJavaRef<jobject> j_color_info(nullptr);
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
      j_color_info.Reset(env->NewObjectOrAbort(
          "dev/cobalt/media/MediaCodecBridge$ColorInfo", "(IIIFFFFFFFFFFIIZ)V",
          color_range, color_standard, color_transfer,
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

  ScopedLocalJavaRef<jobject> j_create_media_codec_bridge_result(
      env->NewObjectOrAbort(
          "dev/cobalt/media/MediaCodecBridge$CreateMediaCodecBridgeResult",
          "()V"));

  scoped_ptr<MediaCodecBridge> native_media_codec_bridge(
      new MediaCodecBridge(handler));
  env->CallStaticVoidMethodOrAbort(
      "dev/cobalt/media/MediaCodecBridge", "createVideoMediaCodecBridge",
      "(JLjava/lang/String;ZZIIILandroid/view/Surface;"
      "Landroid/media/MediaCrypto;"
      "Ldev/cobalt/media/MediaCodecBridge$ColorInfo;"
      "I"
      "Ldev/cobalt/media/MediaCodecBridge$CreateMediaCodecBridgeResult;)"
      "V",
      reinterpret_cast<jlong>(native_media_codec_bridge.get()), j_mime.Get(),
      !!j_media_crypto, require_software_codec, width, height, fps, j_surface,
      j_media_crypto, j_color_info.Get(), tunnel_mode_audio_session_id,
      j_create_media_codec_bridge_result.Get());

  jobject j_media_codec_bridge = env->CallObjectMethodOrAbort(
      j_create_media_codec_bridge_result.Get(), "mediaCodecBridge",
      "()Ldev/cobalt/media/MediaCodecBridge;");

  if (!j_media_codec_bridge) {
    ScopedLocalJavaRef<jstring> j_error_message(
        env->CallObjectMethodOrAbort(j_create_media_codec_bridge_result.Get(),
                                     "errorMessage", "()Ljava/lang/String;"));
    *error_message = env->GetStringStandardUTFOrAbort(j_error_message.Get());
    return scoped_ptr<MediaCodecBridge>(NULL);
  }

  j_media_codec_bridge = env->ConvertLocalRefToGlobalRef(j_media_codec_bridge);
  native_media_codec_bridge->Initialize(j_media_codec_bridge);
  return native_media_codec_bridge.Pass();
}

MediaCodecBridge::~MediaCodecBridge() {
  if (!j_media_codec_bridge_) {
    return;
  }

  JniEnvExt* env = JniEnvExt::Get();

  env->CallVoidMethodOrAbort(j_media_codec_bridge_, "stop", "()V");
  env->CallVoidMethodOrAbort(j_media_codec_bridge_, "release", "()V");
  env->DeleteGlobalRef(j_media_codec_bridge_);
  j_media_codec_bridge_ = NULL;

  SB_DCHECK(j_reused_get_output_format_result_);
  env->DeleteGlobalRef(j_reused_get_output_format_result_);
  j_reused_get_output_format_result_ = NULL;
}

jobject MediaCodecBridge::GetInputBuffer(jint index) {
  SB_DCHECK(index >= 0);
  return JniEnvExt::Get()->CallObjectMethodOrAbort(
      j_media_codec_bridge_, "getInputBuffer", "(I)Ljava/nio/ByteBuffer;",
      index);
}

jint MediaCodecBridge::QueueInputBuffer(jint index,
                                        jint offset,
                                        jint size,
                                        jlong presentation_time_microseconds,
                                        jint flags) {
  return JniEnvExt::Get()->CallIntMethodOrAbort(
      j_media_codec_bridge_, "queueInputBuffer", "(IIIJI)I", index, offset,
      size, presentation_time_microseconds, flags);
}

jint MediaCodecBridge::QueueSecureInputBuffer(
    jint index,
    jint offset,
    const SbDrmSampleInfo& drm_sample_info,
    jlong presentation_time_microseconds) {
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jbyteArray> j_iv(env->NewByteArrayFromRaw(
      reinterpret_cast<const jbyte*>(drm_sample_info.initialization_vector),
      drm_sample_info.initialization_vector_size));
  ScopedLocalJavaRef<jbyteArray> j_key_id(env->NewByteArrayFromRaw(
      reinterpret_cast<const jbyte*>(drm_sample_info.identifier),
      drm_sample_info.identifier_size));

  // Reshape the sub sample mapping like this:
  // [(c0, e0), (c1, e1), ...] -> [c0, c1, ...] and [e0, e1, ...]
  int32_t subsample_count = drm_sample_info.subsample_count;
  scoped_array<jint> clear_bytes(new jint[subsample_count]);
  scoped_array<jint> encrypted_bytes(new jint[subsample_count]);
  for (int i = 0; i < subsample_count; ++i) {
    clear_bytes[i] = drm_sample_info.subsample_mapping[i].clear_byte_count;
    encrypted_bytes[i] =
        drm_sample_info.subsample_mapping[i].encrypted_byte_count;
  }
  ScopedLocalJavaRef<jintArray> j_clear_bytes(
      env->NewIntArrayFromRaw(clear_bytes.get(), subsample_count));
  ScopedLocalJavaRef<jintArray> j_encrypted_bytes(
      env->NewIntArrayFromRaw(encrypted_bytes.get(), subsample_count));

  return env->CallIntMethodOrAbort(
      j_media_codec_bridge_, "queueSecureInputBuffer", "(II[B[B[I[IIIIIJ)I",
      index, offset, j_iv.Get(), j_key_id.Get(), j_clear_bytes.Get(),
      j_encrypted_bytes.Get(), subsample_count, CRYPTO_MODE_AES_CTR, 0, 0,
      presentation_time_microseconds);
}

jobject MediaCodecBridge::GetOutputBuffer(jint index) {
  SB_DCHECK(index >= 0);
  return JniEnvExt::Get()->CallObjectMethodOrAbort(
      j_media_codec_bridge_, "getOutputBuffer", "(I)Ljava/nio/ByteBuffer;",
      index);
}

void MediaCodecBridge::ReleaseOutputBuffer(jint index, jboolean render) {
  JniEnvExt::Get()->CallVoidMethodOrAbort(
      j_media_codec_bridge_, "releaseOutputBuffer", "(IZ)V", index, render);
}

void MediaCodecBridge::ReleaseOutputBufferAtTimestamp(
    jint index,
    jlong render_timestamp_ns) {
  JniEnvExt::Get()->CallVoidMethodOrAbort(j_media_codec_bridge_,
                                          "releaseOutputBuffer", "(IJ)V", index,
                                          render_timestamp_ns);
}

void MediaCodecBridge::SetPlaybackRate(double playback_rate) {
  JniEnvExt::Get()->CallVoidMethodOrAbort(
      j_media_codec_bridge_, "setPlaybackRate", "(D)V", playback_rate);
}

jint MediaCodecBridge::Flush() {
  return JniEnvExt::Get()->CallIntMethodOrAbort(j_media_codec_bridge_, "flush",
                                                "()I");
}

SurfaceDimensions MediaCodecBridge::GetOutputDimensions() {
  JniEnvExt* env = JniEnvExt::Get();
  env->CallVoidMethodOrAbort(
      j_media_codec_bridge_, "getOutputFormat",
      "(Ldev/cobalt/media/MediaCodecBridge$GetOutputFormatResult;)V",
      j_reused_get_output_format_result_);
  return {env->CallIntMethodOrAbort(j_reused_get_output_format_result_, "width",
                                    "()I"),
          env->CallIntMethodOrAbort(j_reused_get_output_format_result_,
                                    "height", "()I")};
}

AudioOutputFormatResult MediaCodecBridge::GetAudioOutputFormat() {
  JniEnvExt* env = JniEnvExt::Get();
  env->CallVoidMethodOrAbort(
      j_media_codec_bridge_, "getOutputFormat",
      "(Ldev/cobalt/media/MediaCodecBridge$GetOutputFormatResult;)V",
      j_reused_get_output_format_result_);

  jint status = env->CallIntMethodOrAbort(j_reused_get_output_format_result_,
                                          "status", "()I");
  if (status == MEDIA_CODEC_ERROR) {
    return {status, 0, 0};
  }

  return {status,
          env->CallIntMethodOrAbort(j_reused_get_output_format_result_,
                                    "sampleRate", "()I"),
          env->CallIntMethodOrAbort(j_reused_get_output_format_result_,
                                    "channelCount", "()I")};
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

void MediaCodecBridge::OnMediaCodecFrameRendered(SbTime frame_timestamp) {
  handler_->OnMediaCodecFrameRendered(frame_timestamp);
}

MediaCodecBridge::MediaCodecBridge(Handler* handler) : handler_(handler) {
  SB_DCHECK(handler_);
}

void MediaCodecBridge::Initialize(jobject j_media_codec_bridge) {
  SB_DCHECK(j_media_codec_bridge);

  j_media_codec_bridge_ = j_media_codec_bridge;

  JniEnvExt* env = JniEnvExt::Get();
  SB_DCHECK(env->GetObjectRefType(j_media_codec_bridge_) == JNIGlobalRefType);

  j_reused_get_output_format_result_ = env->NewObjectOrAbort(
      "dev/cobalt/media/MediaCodecBridge$GetOutputFormatResult", "()V");
  SB_DCHECK(j_reused_get_output_format_result_);
  j_reused_get_output_format_result_ =
      env->ConvertLocalRefToGlobalRef(j_reused_get_output_format_result_);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
