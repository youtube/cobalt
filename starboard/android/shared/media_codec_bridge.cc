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

#include "starboard/android/shared/media_codec_bridge.h"

namespace starboard {
namespace android {
namespace shared {

// static
scoped_ptr<MediaCodecBridge> MediaCodecBridge::CreateAudioMediaCodecBridge(
    SbMediaAudioCodec audio_codec,
    const SbMediaAudioHeader& audio_header,
    jobject j_media_crypto) {
  const char* mime = SupportedAudioCodecToMimeType(audio_codec);
  if (!mime) {
    return scoped_ptr<MediaCodecBridge>(NULL);
  }
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jstring> j_mime(env->NewStringUTFOrAbort(mime));
  jobject j_media_codec_bridge = env->CallStaticObjectMethodOrAbort(
      "foo/cobalt/media/MediaCodecBridge", "createAudioMediaCodecBridge",
      "(Ljava/lang/String;ZZIILandroid/media/MediaCrypto;)Lfoo/cobalt/media/"
      "MediaCodecBridge;",
      j_mime.Get(), false, false, audio_header.samples_per_second,
      audio_header.number_of_channels, j_media_crypto);

  if (!j_media_codec_bridge) {
    return scoped_ptr<MediaCodecBridge>(NULL);
  }

  j_media_codec_bridge = env->ConvertLocalRefToGlobalRef(j_media_codec_bridge);
  return scoped_ptr<MediaCodecBridge>(
      new MediaCodecBridge(j_media_codec_bridge));
}

// static
scoped_ptr<MediaCodecBridge> MediaCodecBridge::CreateVideoMediaCodecBridge(
    SbMediaVideoCodec video_codec,
    int width,
    int height,
    jobject j_surface,
    jobject j_media_crypto) {
  const char* mime = SupportedVideoCodecToMimeType(video_codec);
  if (!mime) {
    return scoped_ptr<MediaCodecBridge>(NULL);
  }
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jstring> j_mime(env->NewStringUTFOrAbort(mime));

  jobject j_media_codec_bridge = env->CallStaticObjectMethodOrAbort(
      "foo/cobalt/media/MediaCodecBridge", "createVideoMediaCodecBridge",
      "(Ljava/lang/String;ZZIILandroid/view/Surface;Landroid/media/"
      "MediaCrypto;)Lfoo/cobalt/media/MediaCodecBridge;",
      j_mime.Get(), false, false, width, height, j_surface, j_media_crypto);

  if (!j_media_codec_bridge) {
    return scoped_ptr<MediaCodecBridge>(NULL);
  }

  j_media_codec_bridge = env->ConvertLocalRefToGlobalRef(j_media_codec_bridge);
  return scoped_ptr<MediaCodecBridge>(
      new MediaCodecBridge(j_media_codec_bridge));
}

MediaCodecBridge::~MediaCodecBridge() {
  JniEnvExt* env = JniEnvExt::Get();

  SB_DCHECK(j_media_codec_bridge_);
  env->CallVoidMethodOrAbort(j_media_codec_bridge_, "release", "()V");
  env->DeleteGlobalRef(j_media_codec_bridge_);
  j_media_codec_bridge_ = NULL;

  SB_DCHECK(j_reused_dequeue_input_result_);
  env->DeleteGlobalRef(j_reused_dequeue_input_result_);
  j_reused_dequeue_input_result_ = NULL;

  SB_DCHECK(j_reused_dequeue_output_result_);
  env->DeleteGlobalRef(j_reused_dequeue_output_result_);
  j_reused_dequeue_output_result_ = NULL;

  SB_DCHECK(j_reused_get_output_format_result_);
  env->DeleteGlobalRef(j_reused_get_output_format_result_);
  j_reused_get_output_format_result_ = NULL;
}

DequeueInputResult MediaCodecBridge::DequeueInputBuffer(jlong timeout_us) {
  JniEnvExt* env = JniEnvExt::Get();
  env->CallVoidMethodOrAbort(
      j_media_codec_bridge_, "dequeueInputBuffer",
      "(JLfoo/cobalt/media/MediaCodecBridge$DequeueInputResult;)V", timeout_us,
      j_reused_dequeue_input_result_);
  return {env->CallIntMethodOrAbort(j_reused_dequeue_input_result_, "status",
                                    "()I"),
          env->CallIntMethodOrAbort(j_reused_dequeue_input_result_, "index",
                                    "()I")};
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

DequeueOutputResult MediaCodecBridge::DequeueOutputBuffer(jlong timeout_us) {
  JniEnvExt* env = JniEnvExt::Get();
  env->CallVoidMethodOrAbort(
      j_media_codec_bridge_, "dequeueOutputBuffer",
      "(JLfoo/cobalt/media/MediaCodecBridge$DequeueOutputResult;)V", timeout_us,
      j_reused_dequeue_output_result_);
  return {env->CallIntMethodOrAbort(j_reused_dequeue_output_result_, "status",
                                    "()I"),
          env->CallIntMethodOrAbort(j_reused_dequeue_output_result_, "index",
                                    "()I"),
          env->CallIntMethodOrAbort(j_reused_dequeue_output_result_, "flags",
                                    "()I"),
          env->CallIntMethodOrAbort(j_reused_dequeue_output_result_, "offset",
                                    "()I"),
          env->CallLongMethodOrAbort(j_reused_dequeue_output_result_,
                                     "presentationTimeMicroseconds", "()J"),
          env->CallIntMethodOrAbort(j_reused_dequeue_output_result_, "numBytes",
                                    "()I")};
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

jint MediaCodecBridge::Flush() {
  return JniEnvExt::Get()->CallIntMethodOrAbort(j_media_codec_bridge_, "flush",
                                                "()I");
}

SurfaceDimensions MediaCodecBridge::GetOutputDimensions() {
  JniEnvExt* env = JniEnvExt::Get();
  env->CallVoidMethodOrAbort(
      j_media_codec_bridge_, "getOutputFormat",
      "(Lfoo/cobalt/media/MediaCodecBridge$GetOutputFormatResult;)V",
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
      "(Lfoo/cobalt/media/MediaCodecBridge$GetOutputFormatResult;)V",
      j_reused_get_output_format_result_);

  jint status = env->CallIntMethodOrAbort(j_reused_get_output_format_result_,
                                          "status", "()I");
  if (status == MEDIA_CODEC_ERROR) {
    return {status, 0, 0};
  }

  return {status, env->CallIntMethodOrAbort(j_reused_get_output_format_result_,
                                            "sampleRate", "()I"),
          env->CallIntMethodOrAbort(j_reused_get_output_format_result_,
                                    "channelCount", "()I")};
}

MediaCodecBridge::MediaCodecBridge(jobject j_media_codec_bridge)
    : j_media_codec_bridge_(j_media_codec_bridge),
      j_reused_dequeue_input_result_(NULL),
      j_reused_dequeue_output_result_(NULL),
      j_reused_get_output_format_result_(NULL) {
  SB_DCHECK(j_media_codec_bridge_);
  JniEnvExt* env = JniEnvExt::Get();
  SB_DCHECK(env->GetObjectRefType(j_media_codec_bridge_) == JNIGlobalRefType);

  j_reused_dequeue_input_result_ = env->NewObjectOrAbort(
      "foo/cobalt/media/MediaCodecBridge$DequeueInputResult", "()V");
  SB_DCHECK(j_reused_dequeue_input_result_);
  j_reused_dequeue_input_result_ =
      env->ConvertLocalRefToGlobalRef(j_reused_dequeue_input_result_);

  j_reused_dequeue_output_result_ = env->NewObjectOrAbort(
      "foo/cobalt/media/MediaCodecBridge$DequeueOutputResult", "()V");
  SB_DCHECK(j_reused_dequeue_output_result_);
  j_reused_dequeue_output_result_ =
      env->ConvertLocalRefToGlobalRef(j_reused_dequeue_output_result_);

  j_reused_get_output_format_result_ = env->NewObjectOrAbort(
      "foo/cobalt/media/MediaCodecBridge$GetOutputFormatResult", "()V");
  SB_DCHECK(j_reused_get_output_format_result_);
  j_reused_get_output_format_result_ =
      env->ConvertLocalRefToGlobalRef(j_reused_get_output_format_result_);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
