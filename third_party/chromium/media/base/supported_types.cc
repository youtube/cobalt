// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/supported_types.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media.h"
#include "media/base/media_client.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#if !defined(STARBOARD)
#include "ui/display/display_switches.h"

#if BUILDFLAG(ENABLE_LIBVPX)
// TODO(dalecurtis): This technically should not be allowed in media/base. See
// TODO below about moving outside of base.
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_codec.h"
#endif

#endif  // !defined(STARBOARD)

#if defined(OS_ANDROID)
#include "base/android/build_info.h"

// TODO(dalecurtis): This include is not allowed by media/base since
// media/base/android is technically a different component. We should move
// supported_types*.{cc,h} out of media/base to fix this.
#include "media/base/android/media_codec_util.h"  // nogncheck
#endif

namespace media {

namespace {

bool IsSupportedHdrMetadata(const gfx::HdrMetadataType& hdr_metadata_type) {
  switch (hdr_metadata_type) {
    case gfx::HdrMetadataType::kNone:
      return true;

    case gfx::HdrMetadataType::kSmpteSt2086:
    case gfx::HdrMetadataType::kSmpteSt2094_10:
    case gfx::HdrMetadataType::kSmpteSt2094_40:
      return false;
  }

  NOTREACHED();
  return false;
}

#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_HEVC)
bool IsHevcProfileSupported(VideoCodecProfile profile) {
  // Only encrypted HEVC content is supported, and normally MSE.isTypeSupported
  // returns false for HEVC. The kEnableClearHevcForTesting flag allows it to
  // return true to enable a wider array of test scenarios to function properly.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableClearHevcForTesting)) {
    return false;
  }
  switch (profile) {
    case HEVCPROFILE_MAIN:
      FALLTHROUGH;
    case HEVCPROFILE_MAIN10:
      return true;
    case HEVCPROFILE_MAIN_STILL_PICTURE:
      return false;
    default:
      NOTREACHED();
  }
  return false;
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_HEVC)

}  // namespace

bool IsSupportedAudioType(const AudioType& type) {
#if !defined(STARBOARD)
  MediaClient* media_client = GetMediaClient();
  if (media_client)
    return media_client->IsSupportedAudioType(type);
#endif  // !defined(STARBOARD)

  return IsDefaultSupportedAudioType(type);
}

bool IsSupportedVideoType(const VideoType& type) {
#if !defined(STARBOARD)
  MediaClient* media_client = GetMediaClient();
  if (media_client)
    return media_client->IsSupportedVideoType(type);
#endif  // !defined(STARBOARD)

  return IsDefaultSupportedVideoType(type);
}

bool IsColorSpaceSupported(const VideoColorSpace& color_space) {
  switch (color_space.primaries) {
    case VideoColorSpace::PrimaryID::EBU_3213_E:
    case VideoColorSpace::PrimaryID::INVALID:
      return false;

    // Transfers supported before color management.
    case VideoColorSpace::PrimaryID::BT709:
    case VideoColorSpace::PrimaryID::UNSPECIFIED:
    case VideoColorSpace::PrimaryID::BT470M:
    case VideoColorSpace::PrimaryID::BT470BG:
    case VideoColorSpace::PrimaryID::SMPTE170M:
      break;

    // Supported with color management.
    case VideoColorSpace::PrimaryID::SMPTE240M:
    case VideoColorSpace::PrimaryID::FILM:
    case VideoColorSpace::PrimaryID::BT2020:
    case VideoColorSpace::PrimaryID::SMPTEST428_1:
    case VideoColorSpace::PrimaryID::SMPTEST431_2:
    case VideoColorSpace::PrimaryID::SMPTEST432_1:
      break;
  }

  switch (color_space.transfer) {
    // Transfers supported before color management.
    case VideoColorSpace::TransferID::UNSPECIFIED:
    case VideoColorSpace::TransferID::GAMMA22:
    case VideoColorSpace::TransferID::BT709:
    case VideoColorSpace::TransferID::SMPTE170M:
    case VideoColorSpace::TransferID::BT2020_10:
    case VideoColorSpace::TransferID::BT2020_12:
    case VideoColorSpace::TransferID::IEC61966_2_1:
      break;

    // Supported with color management.
    case VideoColorSpace::TransferID::GAMMA28:
    case VideoColorSpace::TransferID::SMPTE240M:
    case VideoColorSpace::TransferID::LINEAR:
    case VideoColorSpace::TransferID::LOG:
    case VideoColorSpace::TransferID::LOG_SQRT:
    case VideoColorSpace::TransferID::BT1361_ECG:
    case VideoColorSpace::TransferID::SMPTEST2084:
    case VideoColorSpace::TransferID::IEC61966_2_4:
    case VideoColorSpace::TransferID::SMPTEST428_1:
    case VideoColorSpace::TransferID::ARIB_STD_B67:
      break;

    // Never supported.
    case VideoColorSpace::TransferID::INVALID:
      return false;
  }

  switch (color_space.matrix) {
    // Supported before color management.
    case VideoColorSpace::MatrixID::BT709:
    case VideoColorSpace::MatrixID::UNSPECIFIED:
    case VideoColorSpace::MatrixID::BT470BG:
    case VideoColorSpace::MatrixID::SMPTE170M:
    case VideoColorSpace::MatrixID::BT2020_NCL:
      break;

    // Supported with color management.
    case VideoColorSpace::MatrixID::RGB:
    case VideoColorSpace::MatrixID::FCC:
    case VideoColorSpace::MatrixID::SMPTE240M:
    case VideoColorSpace::MatrixID::YCOCG:
    case VideoColorSpace::MatrixID::YDZDX:
    case VideoColorSpace::MatrixID::BT2020_CL:
      break;

    // Never supported.
    case VideoColorSpace::MatrixID::INVALID:
      return false;
  }

  if (color_space.range == gfx::ColorSpace::RangeID::INVALID)
    return false;

  return true;
}

bool IsVp9ProfileSupported(VideoCodecProfile profile) {
#if defined(STARBOARD)
  // Assume all profiles are supported, and let the Starboard implementation to
  // filter it out.
  return profile == VP9PROFILE_PROFILE0 || profile == VP9PROFILE_PROFILE1 ||
         profile == VP9PROFILE_PROFILE2 || profile == VP9PROFILE_PROFILE3;
#else  // defined(STARBOARD)
#if BUILDFLAG(ENABLE_LIBVPX)
  // High bit depth capabilities may be toggled via LibVPX config flags.
  static const bool vpx_supports_hbd = (vpx_codec_get_caps(vpx_codec_vp9_dx()) &
                                        VPX_CODEC_CAP_HIGHBITDEPTH) != 0;
  switch (profile) {
    // LibVPX always supports Profiles 0 and 1.
    case VP9PROFILE_PROFILE0:
    case VP9PROFILE_PROFILE1:
      return true;
#if defined(OS_ANDROID)
    case VP9PROFILE_PROFILE2:
      return vpx_supports_hbd ||
             MediaCodecUtil::IsVp9Profile2DecoderAvailable();
    case VP9PROFILE_PROFILE3:
      return vpx_supports_hbd ||
             MediaCodecUtil::IsVp9Profile3DecoderAvailable();
#else
    case VP9PROFILE_PROFILE2:
    case VP9PROFILE_PROFILE3:
      return vpx_supports_hbd;
#endif
    default:
      NOTREACHED();
  }
#endif
  return false;
#endif  // defined(STARBOARD)
}

bool IsAudioCodecProprietary(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kAAC:
    case AudioCodec::kAC3:
    case AudioCodec::kEAC3:
    case AudioCodec::kAMR_NB:
    case AudioCodec::kAMR_WB:
    case AudioCodec::kGSM_MS:
    case AudioCodec::kALAC:
    case AudioCodec::kMpegHAudio:
      return true;

    case AudioCodec::kFLAC:
    case AudioCodec::kIAMF:
    case AudioCodec::kMP3:
    case AudioCodec::kOpus:
    case AudioCodec::kVorbis:
    case AudioCodec::kPCM:
    case AudioCodec::kPCM_MULAW:
    case AudioCodec::kPCM_S16BE:
    case AudioCodec::kPCM_S24BE:
    case AudioCodec::kPCM_ALAW:
    case AudioCodec::kUnknown:
      return false;
  }

  NOTREACHED();
  return false;
}

bool IsDefaultSupportedAudioType(const AudioType& type) {
  if (type.spatial_rendering)
    return false;

#if !BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (IsAudioCodecProprietary(type.codec))
    return false;
#endif

  switch (type.codec) {
    case AudioCodec::kAAC:
      if (type.profile != AudioCodecProfile::kXHE_AAC)
        return true;
#if defined(OS_ANDROID)
      return base::android::BuildInfo::GetInstance()->sdk_int() >=
             base::android::SDK_VERSION_P;
#else
      return false;
#endif

    case AudioCodec::kFLAC:
    case AudioCodec::kMP3:
    case AudioCodec::kOpus:
    case AudioCodec::kPCM:
    case AudioCodec::kPCM_MULAW:
    case AudioCodec::kPCM_S16BE:
    case AudioCodec::kPCM_S24BE:
    case AudioCodec::kPCM_ALAW:
    case AudioCodec::kVorbis:
      return true;

    case AudioCodec::kAMR_NB:
    case AudioCodec::kAMR_WB:
    case AudioCodec::kGSM_MS:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      return true;
#else
      return false;
#endif

    case AudioCodec::kEAC3:
    case AudioCodec::kALAC:
    case AudioCodec::kAC3:
    case AudioCodec::kMpegHAudio:
    case AudioCodec::kIAMF:
    case AudioCodec::kUnknown:
      return false;
  }

  NOTREACHED();
  return false;
}

bool IsVideoCodecProprietary(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kVC1:
    case VideoCodec::kH264:
    case VideoCodec::kMPEG2:
    case VideoCodec::kMPEG4:
    case VideoCodec::kHEVC:
    case VideoCodec::kDolbyVision:
      return true;
    case VideoCodec::kUnknown:
    case VideoCodec::kTheora:
    case VideoCodec::kVP8:
    case VideoCodec::kVP9:
    case VideoCodec::kAV1:
      return false;
  }

  NOTREACHED();
  return false;
}

// TODO(chcunningham): Add platform specific logic for Android (move from
// MimeUtilIntenral).
bool IsDefaultSupportedVideoType(const VideoType& type) {
  if (!IsSupportedHdrMetadata(type.hdr_metadata_type))
    return false;

#if !BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (IsVideoCodecProprietary(type.codec))
    return false;
#endif

  switch (type.codec) {
    case VideoCodec::kAV1:
      // If the AV1 decoder is enabled, or if we're on Q or later, yes.
#if BUILDFLAG(ENABLE_AV1_DECODER)
      return IsColorSpaceSupported(type.color_space);
#else
#if defined(OS_ANDROID)
      if (base::android::BuildInfo::GetInstance()->sdk_int() >=
              base::android::SDK_VERSION_Q &&
          IsColorSpaceSupported(type.color_space)) {
        return true;
      }
#endif
      return false;
#endif

    case VideoCodec::kVP9:
      // Color management required for HDR to not look terrible.
      return IsColorSpaceSupported(type.color_space) &&
             IsVp9ProfileSupported(type.profile);
    case VideoCodec::kH264:
    case VideoCodec::kVP8:
    case VideoCodec::kTheora:
      return true;

    case VideoCodec::kHEVC:
#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_HEVC)
      return IsColorSpaceSupported(type.color_space) &&
             IsHevcProfileSupported(type.profile);
#else
      return false;
#endif  // BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_HEVC)
    case VideoCodec::kUnknown:
    case VideoCodec::kVC1:
    case VideoCodec::kMPEG2:
    case VideoCodec::kDolbyVision:
      return false;

    case VideoCodec::kMPEG4:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      return true;
#else
      return false;
#endif
  }

  NOTREACHED();
  return false;
}

}  // namespace media
