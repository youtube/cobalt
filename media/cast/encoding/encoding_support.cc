// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/external_video_encoder.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "third_party/libaom/libaom_buildflags.h"

namespace media::cast::encoding_support {
namespace {

bool IsCastStreamingAv1Enabled() {
#if BUILDFLAG(ENABLE_LIBAOM)
  return base::FeatureList::IsEnabled(kCastStreamingAv1);
#else
  return false;
#endif
}

bool IsHardwareEncodingEnabled(
    const std::vector<VideoEncodeAccelerator::SupportedProfile>& profiles,
    VideoCodecProfile min_profile,
    VideoCodecProfile max_profile,
    bool is_enabled_on_platform,
    bool is_force_enabled) {
  // Check if it's enabled on this platform ("default" behavior) or if it is
  // force enabled.
  const bool should_query = is_enabled_on_platform || is_force_enabled;
  if (should_query) {
    for (const auto& vea_profile : profiles) {
      if (vea_profile.profile >= min_profile &&
          vea_profile.profile <= max_profile) {
        return true;
      }
    }
  }
  return false;
}

// Scan profiles for hardware VP8 encoder support.
bool IsHardwareVP8EncodingEnabled(
    const std::vector<VideoEncodeAccelerator::SupportedProfile>& profiles) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kCastStreamingForceDisableHardwareVp8)) {
    return false;
  }

  const bool is_force_enabled =
      command_line.HasSwitch(switches::kCastStreamingForceEnableHardwareVp8);

  return IsHardwareEncodingEnabled(profiles, VP8PROFILE_MIN, VP8PROFILE_MAX,
                                   /*is_enabled_on_platform=*/true,
                                   is_force_enabled);
}

// Scan profiles for hardware H.264 encoder support.
bool IsHardwareH264EncodingEnabled(
    const std::vector<VideoEncodeAccelerator::SupportedProfile>& profiles) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(
          switches::kCastStreamingForceDisableHardwareH264)) {
    return false;
  }

  // TODO(crbug.com/1015482): hardware encoder broken on Windows, Apple OSes.
  bool is_enabled_on_platform = !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_WIN);

  const bool is_force_enabled =
      command_line.HasSwitch(switches::kCastStreamingForceEnableHardwareH264);

  return IsHardwareEncodingEnabled(profiles, H264PROFILE_MIN, H264PROFILE_MAX,
                                   is_enabled_on_platform, is_force_enabled);
}

}  // namespace

bool IsSoftwareEnabled(Codec codec) {
  switch (codec) {
    case Codec::kVideoVp8:
      return true;

    case Codec::kVideoVp9:
      return base::FeatureList::IsEnabled(kCastStreamingVp9);

    case Codec::kVideoAv1:
      return IsCastStreamingAv1Enabled();

    // The test infrastructure is responsible for ensuring the fake codec is
    // used properly.
    case Codec::kVideoFake:
      return true;

    default:
      return false;
  }
}

bool IsHardwareEnabled(
    Codec codec,
    const std::vector<VideoEncodeAccelerator::SupportedProfile>& profiles) {
  switch (codec) {
    case Codec::kVideoVp8:
      return IsHardwareVP8EncodingEnabled(profiles);

    case Codec::kVideoH264:
      return IsHardwareH264EncodingEnabled(profiles);

    default:
      return false;
  }
}

}  //  namespace media::cast::encoding_support
