// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/browser/cdm_message_filter_android.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/android/build_info.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/cdm/common/cdm_messages_android.h"
#include "content/public/browser/android/android_overlay_provider.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/audio_codecs.h"
#include "media/base/eme_constants.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/media_buildflags.h"

using media::MediaDrmBridge;
using media::SupportedCodecs;

namespace cdm {

const size_t kMaxKeySystemLength = 256;

template <typename CodecType>
struct CodecInfo {
  SupportedCodecs eme_codec;
  CodecType codec;
};

const CodecInfo<media::VideoCodec> kWebMVideoCodecsToQuery[] = {
    {media::EME_CODEC_VP8, media::VideoCodec::kVP8},
    {media::EME_CODEC_VP9_PROFILE0, media::VideoCodec::kVP9},
    // Checking for EME_CODEC_VP9_PROFILE2 is handled in code below.
};

const CodecInfo<media::VideoCodec> kMP4VideoCodecsToQuery[] = {
    {media::EME_CODEC_VP9_PROFILE0, media::VideoCodec::kVP9},
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    {media::EME_CODEC_AVC1, media::VideoCodec::kH264},
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    {media::EME_CODEC_HEVC_PROFILE_MAIN, media::VideoCodec::kHEVC},
#endif
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
    {media::EME_CODEC_DOLBY_VISION_AVC, media::VideoCodec::kDolbyVision},
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    {media::EME_CODEC_DOLBY_VISION_HEVC, media::VideoCodec::kDolbyVision},
#endif
#endif
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
};

// Vorbis is not supported. See http://crbug.com/710924 for details.

const CodecInfo<media::AudioCodec> kWebMAudioCodecsToQuery[] = {
    {media::EME_CODEC_OPUS, media::AudioCodec::kOpus},
};

const CodecInfo<media::AudioCodec> kMP4AudioCodecsToQuery[] = {
    {media::EME_CODEC_FLAC, media::AudioCodec::kFLAC},
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    {media::EME_CODEC_AAC, media::AudioCodec::kAAC},
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
    {media::EME_CODEC_AC3, media::AudioCodec::kAC3},
    {media::EME_CODEC_EAC3, media::AudioCodec::kEAC3},
#endif
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    {media::EME_CODEC_DTS, media::AudioCodec::kDTS},
    {media::EME_CODEC_DTSXP2, media::AudioCodec::kDTSXP2},
    {media::EME_CODEC_DTSE, media::AudioCodec::kDTSE},
#endif
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
};

// Is an audio sink connected which supports the given codec?
static bool CanPassthrough(media::AudioCodec codec) {
  return (media::AudioManagerAndroid::GetSinkAudioEncodingFormats() &
          media::ConvertAudioCodecToBitstreamFormat(codec)) != 0;
}

static SupportedCodecs GetSupportedCodecs(
    const SupportedKeySystemRequest& request,
    bool is_secure) {
  const std::string& key_system = request.key_system;
  SupportedCodecs supported_codecs = media::EME_CODEC_NONE;

  if (MediaDrmBridge::IsKeySystemSupportedWithType(key_system, "video/webm")) {
    for (const auto& info : kWebMVideoCodecsToQuery) {
      if ((request.codecs & info.eme_codec) &&
          media::MediaCodecUtil::CanDecode(info.codec, is_secure)) {
        supported_codecs |= info.eme_codec;
      }
    }

    for (const auto& info : kWebMAudioCodecsToQuery) {
      if ((request.codecs & info.eme_codec) &&
          media::MediaCodecUtil::CanDecode(info.codec)) {
        supported_codecs |= info.eme_codec;
      }
    }
  }

  if (MediaDrmBridge::IsKeySystemSupportedWithType(key_system, "video/mp4")) {
    for (const auto& info : kMP4VideoCodecsToQuery) {
      if ((request.codecs & info.eme_codec) &&
          media::MediaCodecUtil::CanDecode(info.codec, is_secure)) {
        supported_codecs |= info.eme_codec;
      }
    }

    // It is possible that a device that is not able to decode the audio stream
    // is connected to an audiosink device that can. In this case, CanDecode()
    // returns false but CanPassthrough() will return true. CanPassthrough()
    // calls AudioManagerAndroid::GetSinkAudioEncodingFormats() to retrieve a
    // bitmask representing audio bitstream formats that are supported by the
    // connected audiosink device. This bitmask is then matched against current
    // audio stream's codec type. A match indicates that the connected
    // audiosink device is able to decode the current audio stream and Chromium
    // should passthrough the audio bitstream instead of trying to decode it.
    for (const auto& info : kMP4AudioCodecsToQuery) {
      if ((request.codecs & info.eme_codec) &&
          (media::MediaCodecUtil::CanDecode(info.codec) ||
           CanPassthrough(info.codec))) {
        supported_codecs |= info.eme_codec;
      }
    }
  }

  // As VP9 profile 2 can not be detected as a MIME type, check for it
  // differently. If it is requested, and VP9 profile 0 is supported, check for
  // profile 2 in the list of supported profiles. This assumes the same decoder
  // is used (so if secure decoding of profile 0 is supported, secure decoding
  // of profile 2 is supported as long as profile 2 is found).
  if ((request.codecs & media::EME_CODEC_VP9_PROFILE2) &&
      (supported_codecs & media::EME_CODEC_VP9_PROFILE0)) {
    std::vector<media::CodecProfileLevel> profiles;
    media::MediaCodecUtil::AddSupportedCodecProfileLevels(&profiles);
    if (base::ranges::any_of(
            profiles, [](const media::CodecProfileLevel& profile) {
              return profile.codec == media::VideoCodec::kVP9 &&
                     profile.profile == media::VP9PROFILE_PROFILE2;
            })) {
      supported_codecs |= media::EME_CODEC_VP9_PROFILE2;
    }
  }
  // Similar to VP9 profile 2 above, check for HEVC profile Main10.
  if ((request.codecs & media::EME_CODEC_HEVC_PROFILE_MAIN10) &&
      (supported_codecs & media::EME_CODEC_HEVC_PROFILE_MAIN)) {
    std::vector<media::CodecProfileLevel> profiles;
    media::MediaCodecUtil::AddSupportedCodecProfileLevels(&profiles);
    if (base::ranges::any_of(
            profiles, [](const media::CodecProfileLevel& profile) {
              return profile.codec == media::VideoCodec::kHEVC &&
                     profile.profile == media::HEVCPROFILE_MAIN10;
            })) {
      supported_codecs |= media::EME_CODEC_HEVC_PROFILE_MAIN10;
    }
  }

  return supported_codecs;
}

CdmMessageFilterAndroid::CdmMessageFilterAndroid(
    bool can_persist_data,
    bool force_to_support_secure_codecs)
    : BrowserMessageFilter(EncryptedMediaMsgStart),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      can_persist_data_(can_persist_data),
      force_to_support_secure_codecs_(force_to_support_secure_codecs) {}

CdmMessageFilterAndroid::~CdmMessageFilterAndroid() {}

bool CdmMessageFilterAndroid::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CdmMessageFilterAndroid, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_QueryKeySystemSupport,
                        OnQueryKeySystemSupport)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_GetPlatformKeySystemNames,
                        OnGetPlatformKeySystemNames)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

scoped_refptr<base::SequencedTaskRunner>
CdmMessageFilterAndroid::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  // Move the IPC handling to FILE thread as it is not very cheap.
  if (message.type() == ChromeViewHostMsg_QueryKeySystemSupport::ID)
    return task_runner_.get();

  return nullptr;
}

void CdmMessageFilterAndroid::OnQueryKeySystemSupport(
    const SupportedKeySystemRequest& request,
    SupportedKeySystemResponse* response) {
  if (!response) {
    NOTREACHED() << "NULL response pointer provided.";
    return;
  }

  if (request.key_system.size() > kMaxKeySystemLength) {
    NOTREACHED() << "Invalid key system: " << request.key_system;
    return;
  }

  if (!MediaDrmBridge::IsKeySystemSupported(request.key_system))
    return;

  // When using MediaDrm, we assume it'll always try to persist some data. If
  // |can_persist_data_| is false and MediaDrm were to persist data on the
  // Android system, we are somewhat violating the incognito assumption.
  // This cannot be used detect incognito mode easily because the result is the
  // same when |can_persist_data_| is false, and when user blocks the "protected
  // media identifier" permission prompt.
  if (!can_persist_data_)
    return;

  DCHECK(request.codecs & media::EME_CODEC_ALL) << "unrecognized codec";
  response->key_system = request.key_system;
  response->non_secure_codecs = GetSupportedCodecs(request, false);

  bool are_overlay_supported =
      content::AndroidOverlayProvider::GetInstance()->AreOverlaysSupported();
  bool overlay_fullscreen_video =
      base::FeatureList::IsEnabled(media::kOverlayFullscreenVideo);
  if (force_to_support_secure_codecs_ ||
      (are_overlay_supported && overlay_fullscreen_video)) {
    DVLOG(1) << "Rendering the output of secure codecs is supported!";
    response->secure_codecs = GetSupportedCodecs(request, true);
  }

  response->is_persistent_license_supported =
      MediaDrmBridge::IsPersistentLicenseTypeSupported(request.key_system);

  response->is_cbcs_encryption_supported =
      media::MediaCodecUtil::PlatformSupportsCbcsEncryption(
          base::android::BuildInfo::GetInstance()->sdk_int());
}

void CdmMessageFilterAndroid::OnGetPlatformKeySystemNames(
    std::vector<std::string>* key_systems) {
  *key_systems = MediaDrmBridge::GetPlatformKeySystemNames();
}

}  // namespace cdm
