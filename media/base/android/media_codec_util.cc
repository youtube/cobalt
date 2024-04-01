// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_util.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_jni_headers/CodecProfileLevelList_jni.h"
#include "media/base/android/media_jni_headers/MediaCodecUtil_jni.h"
#include "media/base/video_codecs.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaIntArrayToIntVector;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::SDK_VERSION_LOLLIPOP;
using base::android::SDK_VERSION_LOLLIPOP_MR1;
using base::android::SDK_VERSION_P;

namespace media {

namespace {
const char kMp3MimeType[] = "audio/mpeg";
const char kAacMimeType[] = "audio/mp4a-latm";
const char kOpusMimeType[] = "audio/opus";
const char kVorbisMimeType[] = "audio/vorbis";
const char kFLACMimeType[] = "audio/flac";
const char kAc3MimeType[] = "audio/ac3";
const char kEac3MimeType[] = "audio/eac3";
const char kBitstreamAudioMimeType[] = "audio/raw";
const char kAvcMimeType[] = "video/avc";
const char kDolbyVisionMimeType[] = "video/dolby-vision";
const char kHevcMimeType[] = "video/hevc";
const char kVp8MimeType[] = "video/x-vnd.on2.vp8";
const char kVp9MimeType[] = "video/x-vnd.on2.vp9";
const char kAv1MimeType[] = "video/av01";
}  // namespace

static CodecProfileLevel MediaCodecProfileLevelToChromiumProfileLevel(
    JNIEnv* env,
    const JavaRef<jobject>& j_codec_profile_level) {
  VideoCodec codec = static_cast<VideoCodec>(
      Java_CodecProfileLevelAdapter_getCodec(env, j_codec_profile_level));
  VideoCodecProfile profile = static_cast<VideoCodecProfile>(
      Java_CodecProfileLevelAdapter_getProfile(env, j_codec_profile_level));
  auto level = static_cast<VideoCodecLevel>(
      Java_CodecProfileLevelAdapter_getLevel(env, j_codec_profile_level));
  return {codec, profile, level};
}

static bool IsSupportedAndroidMimeType(const std::string& mime_type) {
  std::vector<std::string> supported{
      kMp3MimeType, kAacMimeType,         kOpusMimeType, kVorbisMimeType,
      kAvcMimeType, kDolbyVisionMimeType, kHevcMimeType, kVp8MimeType,
      kVp9MimeType, kAv1MimeType};
  return std::find(supported.begin(), supported.end(), mime_type) !=
         supported.end();
}

static std::string GetDefaultCodecName(const std::string& mime_type,
                                       MediaCodecDirection direction,
                                       bool requires_software_codec) {
  DCHECK(MediaCodecUtil::IsMediaCodecAvailable());
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime_type);
  ScopedJavaLocalRef<jstring> j_codec_name =
      Java_MediaCodecUtil_getDefaultCodecName(
          env, j_mime, static_cast<int>(direction), requires_software_codec);
  return ConvertJavaStringToUTF8(env, j_codec_name.obj());
}

static bool IsDecoderSupportedByDevice(const std::string& android_mime_type) {
  DCHECK(MediaCodecUtil::IsMediaCodecAvailable());
  DCHECK(IsSupportedAndroidMimeType(android_mime_type));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime =
      ConvertUTF8ToJavaString(env, android_mime_type);
  return Java_MediaCodecUtil_isDecoderSupportedForDevice(env, j_mime);
}

static bool IsEncoderSupportedByDevice(const std::string& android_mime_type) {
  DCHECK(MediaCodecUtil::IsMediaCodecAvailable());
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime =
      ConvertUTF8ToJavaString(env, android_mime_type);
  return Java_MediaCodecUtil_isEncoderSupportedByDevice(env, j_mime);
}

static bool CanDecodeInternal(const std::string& mime, bool is_secure) {
  if (!MediaCodecUtil::IsMediaCodecAvailable())
    return false;
  if (mime.empty())
    return false;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);
  return Java_MediaCodecUtil_canDecode(env, j_mime, is_secure);
}

static bool HasVp9Profile23Decoder() {
  // Support for VP9.2, VP9.3 was added in Nougat but it requires hardware
  // support which we can't check from the renderer process. Since Android P+
  // has a software decoder available for VP9.2, VP9.3 content and usage is nil
  // on Android, just gate support on P+.
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
         base::android::SDK_VERSION_P;
}

// static
std::string MediaCodecUtil::CodecToAndroidMimeType(AudioCodec codec) {
  if (IsPassthroughAudioFormat(codec))
    return kBitstreamAudioMimeType;

  switch (codec) {
    case AudioCodec::kMP3:
      return kMp3MimeType;
    case AudioCodec::kVorbis:
      return kVorbisMimeType;
    case AudioCodec::kFLAC:
      return kFLACMimeType;
    case AudioCodec::kOpus:
      return kOpusMimeType;
    case AudioCodec::kAAC:
      return kAacMimeType;
    case AudioCodec::kAC3:
      return kAc3MimeType;
    case AudioCodec::kEAC3:
      return kEac3MimeType;
    default:
      return std::string();
  }
}

// static
std::string MediaCodecUtil::CodecToAndroidMimeType(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      return kAvcMimeType;
    case VideoCodec::kHEVC:
      return kHevcMimeType;
    case VideoCodec::kVP8:
      return kVp8MimeType;
    case VideoCodec::kVP9:
      return kVp9MimeType;
    case VideoCodec::kDolbyVision:
      return kDolbyVisionMimeType;
    case VideoCodec::kAV1:
      return kAv1MimeType;
    default:
      return std::string();
  }
}

// static
bool MediaCodecUtil::IsMediaCodecAvailable() {
  return IsMediaCodecAvailableFor(
      base::android::BuildInfo::GetInstance()->sdk_int(),
      base::android::BuildInfo::GetInstance()->model());
}

// static
bool MediaCodecUtil::IsMediaCodecAvailableFor(int sdk, const char* model) {
  // We will block the model on any sdk that is as old or older than
  // |last_bad_sdk| for the given model.
  struct BlocklistEntry {
    BlocklistEntry(const char* m, int s) : model(m), last_bad_sdk(s) {}
    base::StringPiece model;
    int last_bad_sdk;
    bool operator==(const BlocklistEntry& other) const {
      // Search on name only.  Ignore |last_bad_sdk|.
      return model == other.model;
    }
  };
  static const BlocklistEntry blocklist[] = {
      // crbug.com/653905
      {"LGMS330", SDK_VERSION_LOLLIPOP_MR1},
  };

  const BlocklistEntry* iter = std::find(
      std::begin(blocklist), std::end(blocklist), BlocklistEntry(model, 0));
  return iter == std::end(blocklist) || sdk > iter->last_bad_sdk;
}

// static
bool MediaCodecUtil::PlatformSupportsCbcsEncryption(int sdk) {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecUtil_platformSupportsCbcsEncryption(env, sdk);
}

// static
std::set<int> MediaCodecUtil::GetEncoderColorFormats(
    const std::string& mime_type) {
  std::set<int> color_formats;
  if (!IsMediaCodecAvailable())
    return color_formats;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime_type);
  ScopedJavaLocalRef<jintArray> j_color_format_array =
      Java_MediaCodecUtil_getEncoderColorFormatsForMime(env, j_mime);

  if (!j_color_format_array.is_null()) {
    std::vector<int> formats;
    JavaIntArrayToIntVector(env, j_color_format_array, &formats);
    color_formats = std::set<int>(formats.begin(), formats.end());
  }

  return color_formats;
}

#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
// static
bool MediaCodecUtil::IsDolbyVisionDecoderAvailable() {
  return IsMediaCodecAvailable() &&
         IsDecoderSupportedByDevice(kDolbyVisionMimeType);
}
#endif

// static
bool MediaCodecUtil::IsVp8DecoderAvailable() {
  return IsMediaCodecAvailable() && IsDecoderSupportedByDevice(kVp8MimeType);
}

// static
bool MediaCodecUtil::IsVp8EncoderAvailable() {
  // Currently the vp8 encoder and decoder blocklists cover the same devices,
  // but we have a second method for clarity in future issues.
  return IsVp8DecoderAvailable();
}

// static
bool MediaCodecUtil::IsVp9DecoderAvailable() {
  return IsMediaCodecAvailable() && IsDecoderSupportedByDevice(kVp9MimeType);
}

// static
bool MediaCodecUtil::IsVp9Profile2DecoderAvailable() {
  return IsVp9DecoderAvailable() && HasVp9Profile23Decoder();
}

// static
bool MediaCodecUtil::IsVp9Profile3DecoderAvailable() {
  return IsVp9DecoderAvailable() && HasVp9Profile23Decoder();
}

// static
bool MediaCodecUtil::IsOpusDecoderAvailable() {
  return IsMediaCodecAvailable() && IsDecoderSupportedByDevice(kOpusMimeType);
}

// static
bool MediaCodecUtil::IsAv1DecoderAvailable() {
  return IsMediaCodecAvailable() && IsDecoderSupportedByDevice(kAv1MimeType);
}

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
// static
bool MediaCodecUtil::IsHEVCDecoderAvailable() {
  return IsMediaCodecAvailable() && IsDecoderSupportedByDevice(kHevcMimeType);
}
#endif

// static
bool MediaCodecUtil::IsSurfaceViewOutputSupported() {
  // Disable SurfaceView output for the Samsung Galaxy S3; it does not work
  // well enough for even 360p24 H264 playback.  http://crbug.com/602870.
  //
  // Notably this is codec agnostic at present, so any devices added to the
  // disabled list will avoid trying to play any codecs on SurfaceView.  If
  // needed in the future this can be expanded to be codec specific.
  constexpr const char* kDisabledModels[] = {// Exynos 4 (Mali-400)
                                             "GT-I9300", "GT-I9305", "SHV-E210",
                                             // Snapdragon S4 (Adreno-225)
                                             "SCH-I535", "SCH-J201", "SCH-R530",
                                             "SCH-I960", "SCH-S968", "SGH-T999",
                                             "SGH-I747", "SGH-N064"};

  std::string model(base::android::BuildInfo::GetInstance()->model());
  for (auto* disabled_model : kDisabledModels) {
    if (base::StartsWith(model, disabled_model,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return false;
    }
  }

  return true;
}

// static
bool MediaCodecUtil::IsSetOutputSurfaceSupported() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecUtil_isSetOutputSurfaceSupported(env);
}

// static
bool MediaCodecUtil::IsPassthroughAudioFormat(AudioCodec codec) {
  return codec == AudioCodec::kAC3 || codec == AudioCodec::kEAC3;
}

// static
bool MediaCodecUtil::CanDecode(VideoCodec codec, bool is_secure) {
  return CanDecodeInternal(CodecToAndroidMimeType(codec), is_secure);
}

// static
bool MediaCodecUtil::CanDecode(AudioCodec codec) {
  return CanDecodeInternal(CodecToAndroidMimeType(codec), false);
}

// static
bool MediaCodecUtil::IsH264EncoderAvailable(bool use_codec_list) {
  if (!IsMediaCodecAvailable())
    return false;

  constexpr const char* kDisabledModels[] = {"SAMSUNG-SGH-I337", "Nexus 7",
                                             "Nexus 4"};
  const std::string model(base::android::BuildInfo::GetInstance()->model());
  for (auto* disabled_model : kDisabledModels) {
    if (base::StartsWith(model, disabled_model,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return false;
    }
  }

  if (use_codec_list)
    return IsEncoderSupportedByDevice(kAvcMimeType);

  // Assume support since Chrome only supports Lollipop+.
  return true;
}

// static
bool MediaCodecUtil::AddSupportedCodecProfileLevels(
    std::vector<CodecProfileLevel>* result) {
  DCHECK(result);
  if (!IsMediaCodecAvailable())
    return false;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_codec_profile_levels(
      Java_MediaCodecUtil_getSupportedCodecProfileLevels(env));
  for (auto java_codec_profile_level :
       j_codec_profile_levels.ReadElements<jobject>()) {
    result->push_back(MediaCodecProfileLevelToChromiumProfileLevel(
        env, java_codec_profile_level));
  }
  return true;
}

// static
bool MediaCodecUtil::IsKnownUnaccelerated(VideoCodec codec,
                                          MediaCodecDirection direction) {
  if (!IsMediaCodecAvailable())
    return true;

  std::string codec_name =
      GetDefaultCodecName(CodecToAndroidMimeType(codec), direction, false);
  DVLOG(1) << __func__ << "Default codec for " << GetCodecName(codec) << " : "
           << codec_name << ", direction: " << static_cast<int>(direction);
  if (codec_name.empty())
    return true;

  // MediaTek hardware vp8 is known slower than the software implementation.
  if (base::StartsWith(codec_name, "OMX.MTK.", base::CompareCase::SENSITIVE)) {
    if (codec == VideoCodec::kVP8) {
      // We may still reject VP8 hardware decoding later on certain chipsets,
      // see isDecoderSupportedForDevice(). We don't have the the chipset ID
      // here to check now though.
      return base::android::BuildInfo::GetInstance()->sdk_int() < SDK_VERSION_P;
    }

    return false;
  }

  // It would be nice if MediaCodecInfo externalized some notion of
  // HW-acceleration but it doesn't. Android Media guidance is that the
  // "OMX.google" prefix is always used for SW decoders, so that's what we
  // use. "OMX.SEC.*" codec is Samsung software implementation - report it
  // as unaccelerated as well.
  return base::StartsWith(codec_name, "OMX.google.",
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(codec_name, "OMX.SEC.", base::CompareCase::SENSITIVE);
}

}  // namespace media
