// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include <drm_fourcc.h>
#include <gbm.h>
#include <unistd.h>
#include <map>
#include <vector>

#include <va/va.h>
#include <va/va_drmcommon.h>
#include <va/va_str.h>

#include "base/bits.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/cpu.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_suite.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media_switches.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/media_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/linux/gbm_defines.h"

namespace media {
namespace {

absl::optional<VAProfile> ConvertToVAProfile(VideoCodecProfile profile) {
  // A map between VideoCodecProfile and VAProfile.
  const std::map<VideoCodecProfile, VAProfile> kProfileMap = {
    // VAProfileH264Baseline is deprecated in <va/va.h> from libva 2.0.0.
    {H264PROFILE_BASELINE, VAProfileH264ConstrainedBaseline},
    {H264PROFILE_MAIN, VAProfileH264Main},
    {H264PROFILE_HIGH, VAProfileH264High},
    {VP8PROFILE_ANY, VAProfileVP8Version0_3},
    {VP9PROFILE_PROFILE0, VAProfileVP9Profile0},
    {VP9PROFILE_PROFILE2, VAProfileVP9Profile2},
    {AV1PROFILE_PROFILE_MAIN, VAProfileAV1Profile0},
#if BUILDFLAG(ENABLE_PLATFORM_HEVC_DECODING)
    {HEVCPROFILE_MAIN, VAProfileHEVCMain},
    {HEVCPROFILE_MAIN10, VAProfileHEVCMain10},
#endif
  };
  auto it = kProfileMap.find(profile);
  return it != kProfileMap.end() ? absl::make_optional<VAProfile>(it->second)
                                 : absl::nullopt;
}

// Converts the given string to VAProfile
absl::optional<VAProfile> StringToVAProfile(const std::string& va_profile) {
  const std::map<std::string, VAProfile> kStringToVAProfile = {
    {"VAProfileNone", VAProfileNone},
    {"VAProfileH264ConstrainedBaseline", VAProfileH264ConstrainedBaseline},
    // Even though it's deprecated, we leave VAProfileH264Baseline's
    // translation here to assert we never encounter it.
    {"VAProfileH264Baseline", VAProfileH264Baseline},
    {"VAProfileH264Main", VAProfileH264Main},
    {"VAProfileH264High", VAProfileH264High},
    {"VAProfileJPEGBaseline", VAProfileJPEGBaseline},
    {"VAProfileVP8Version0_3", VAProfileVP8Version0_3},
    {"VAProfileVP9Profile0", VAProfileVP9Profile0},
    {"VAProfileVP9Profile2", VAProfileVP9Profile2},
    {"VAProfileAV1Profile0", VAProfileAV1Profile0},
#if BUILDFLAG(ENABLE_PLATFORM_HEVC_DECODING)
    {"VAProfileHEVCMain", VAProfileHEVCMain},
    {"VAProfileHEVCMain10", VAProfileHEVCMain10},
#endif
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
    {"VAProfileProtected", VAProfileProtected},
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  };

  auto it = kStringToVAProfile.find(va_profile);
  return it != kStringToVAProfile.end()
             ? absl::make_optional<VAProfile>(it->second)
             : absl::nullopt;
}

// Converts the given string to VAEntrypoint
absl::optional<VAEntrypoint> StringToVAEntrypoint(
    const std::string& va_entrypoint) {
  const std::map<std::string, VAEntrypoint> kStringToVAEntrypoint = {
    {"VAEntrypointVLD", VAEntrypointVLD},
    {"VAEntrypointEncSlice", VAEntrypointEncSlice},
    {"VAEntrypointEncPicture", VAEntrypointEncPicture},
    {"VAEntrypointEncSliceLP", VAEntrypointEncSliceLP},
    {"VAEntrypointVideoProc", VAEntrypointVideoProc},
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
    {"VAEntrypointProtectedContent", VAEntrypointProtectedContent},
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  };

  auto it = kStringToVAEntrypoint.find(va_entrypoint);
  return it != kStringToVAEntrypoint.end()
             ? absl::make_optional<VAEntrypoint>(it->second)
             : absl::nullopt;
}

std::unique_ptr<base::test::ScopedFeatureList> CreateScopedFeatureList() {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitWithFeatures(
      /*enabled_features=*/{media::kVaapiAV1Decoder},
      /*disabled_features=*/{});
  return scoped_feature_list;
}

unsigned int ToVaRTFormat(uint32_t va_fourcc) {
  switch (va_fourcc) {
    case VA_FOURCC_I420:
    case VA_FOURCC_NV12:
      return VA_RT_FORMAT_YUV420;
    case VA_FOURCC_YUY2:
      return VA_RT_FORMAT_YUV422;
    case VA_FOURCC_RGBA:
      return VA_RT_FORMAT_RGB32;
    case VA_FOURCC_P010:
      return VA_RT_FORMAT_YUV420_10;
  }
  return kInvalidVaRtFormat;
}

uint32_t ToVaFourcc(unsigned int va_rt_format) {
  switch (va_rt_format) {
    case VA_RT_FORMAT_YUV420:
      return VA_FOURCC_NV12;
    case VA_RT_FORMAT_YUV420_10:
      return VA_FOURCC_P010;
  }
  return DRM_FORMAT_INVALID;
}

int ToGBMFormat(unsigned int va_rt_format) {
  switch (va_rt_format) {
    case VA_RT_FORMAT_YUV420:
      return DRM_FORMAT_NV12;
    case VA_RT_FORMAT_YUV420_10:
      return DRM_FORMAT_P010;
  }
  return DRM_FORMAT_INVALID;
}

const std::string VARTFormatToString(unsigned int va_rt_format) {
  switch (va_rt_format) {
    case VA_RT_FORMAT_YUV420:
      return "VA_RT_FORMAT_YUV420";
    case VA_RT_FORMAT_YUV420_10:
      return "VA_RT_FORMAT_YUV420_10";
  }
  NOTREACHED() << "Unknown VA_RT_FORMAT 0x" << std::hex << va_rt_format;
  return "Unknown VA_RT_FORMAT";
}

#define TOSTR(enumCase) \
  case enumCase:        \
    return #enumCase

const char* VAProfileToString(VAProfile profile) {
  // clang-format off
  switch (profile) {
    TOSTR(VAProfileNone);
    TOSTR(VAProfileMPEG2Simple);
    TOSTR(VAProfileMPEG2Main);
    TOSTR(VAProfileMPEG4Simple);
    TOSTR(VAProfileMPEG4AdvancedSimple);
    TOSTR(VAProfileMPEG4Main);
    case VAProfileH264Baseline:
      NOTREACHED() << "VAProfileH264Baseline is deprecated";
      return "Deprecated VAProfileH264Baseline";
    TOSTR(VAProfileH264Main);
    TOSTR(VAProfileH264High);
    TOSTR(VAProfileVC1Simple);
    TOSTR(VAProfileVC1Main);
    TOSTR(VAProfileVC1Advanced);
    TOSTR(VAProfileH263Baseline);
    TOSTR(VAProfileH264ConstrainedBaseline);
    TOSTR(VAProfileJPEGBaseline);
    TOSTR(VAProfileVP8Version0_3);
    TOSTR(VAProfileH264MultiviewHigh);
    TOSTR(VAProfileH264StereoHigh);
    TOSTR(VAProfileHEVCMain);
    TOSTR(VAProfileHEVCMain10);
    TOSTR(VAProfileVP9Profile0);
    TOSTR(VAProfileVP9Profile1);
    TOSTR(VAProfileVP9Profile2);
    TOSTR(VAProfileVP9Profile3);
    TOSTR(VAProfileHEVCMain12);
    TOSTR(VAProfileHEVCMain422_10);
    TOSTR(VAProfileHEVCMain422_12);
    TOSTR(VAProfileHEVCMain444);
    TOSTR(VAProfileHEVCMain444_10);
    TOSTR(VAProfileHEVCMain444_12);
    TOSTR(VAProfileHEVCSccMain);
    TOSTR(VAProfileHEVCSccMain10);
    TOSTR(VAProfileHEVCSccMain444);
    TOSTR(VAProfileAV1Profile0);
    TOSTR(VAProfileAV1Profile1);
    TOSTR(VAProfileHEVCSccMain444_10);
#if VA_MAJOR_VERSION >= 2 || VA_MINOR_VERSION >= 11
    TOSTR(VAProfileProtected);
#endif
  }
  // clang-format on
  return "<unknown profile>";
}

}  // namespace

class VaapiTest : public testing::Test {
 public:
  VaapiTest() : scoped_feature_list_(CreateScopedFeatureList()) {}
  ~VaapiTest() override = default;

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

std::map<VAProfile, std::vector<VAEntrypoint>> ParseVainfo(
    const std::string& output) {
  const std::vector<std::string> lines =
      base::SplitString(output, "\n", base::WhitespaceHandling::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_ALL);
  std::map<VAProfile, std::vector<VAEntrypoint>> info;
  for (const std::string& line : lines) {
    if (!base::StartsWith(line, "VAProfile",
                          base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }
    std::vector<std::string> res =
        base::SplitString(line, ":", base::WhitespaceHandling::TRIM_WHITESPACE,
                          base::SplitResult::SPLIT_WANT_ALL);
    if (res.size() != 2) {
      LOG(ERROR) << "Unexpected line: " << line;
      continue;
    }

    auto va_profile = StringToVAProfile(res[0]);
    if (!va_profile)
      continue;
    auto va_entrypoint = StringToVAEntrypoint(res[1]);
    if (!va_entrypoint)
      continue;
    info[*va_profile].push_back(*va_entrypoint);
    DVLOG(3) << line;
  }
  return info;
}

std::map<VAProfile, std::vector<VAEntrypoint>> RetrieveVAInfoOutput() {
  int fds[2];
  PCHECK(pipe(fds) == 0);
  base::File read_pipe(fds[0]);
  base::ScopedFD write_pipe_fd(fds[1]);

  base::LaunchOptions options;
  options.fds_to_remap.emplace_back(write_pipe_fd.get(), STDOUT_FILENO);
  std::vector<std::string> argv = {"vainfo"};
  EXPECT_TRUE(LaunchProcess(argv, options).IsValid());
  write_pipe_fd.reset();

  char buf[4096] = {};
  int n = read_pipe.ReadAtCurrentPos(buf, sizeof(buf));
  PCHECK(n >= 0);
  EXPECT_LT(n, 4096);
  std::string output(buf, n);
  DVLOG(4) << output;
  return ParseVainfo(output);
}

TEST_F(VaapiTest, VaapiSandboxInitialization) {
  // Here we just test that the PreSandboxInitialization() in SetUp() worked
  // fine. Said initialization is buried in internal singletons, but we can
  // verify that at least the implementation type has been filled in.
  EXPECT_NE(VaapiWrapper::GetImplementationType(), VAImplementation::kInvalid);
}

// Commit [1] deprecated VAProfileH264Baseline from libva in 2017 (release
// 2.0.0). This test verifies that such profile is never seen in the lab.
// [1] https://github.com/intel/libva/commit/6f69256f8ccc9a73c0b196ab77ac69ab1f4f33c2
TEST_F(VaapiTest, VerifyNoVAProfileH264Baseline) {
  const auto va_info = RetrieveVAInfoOutput();
  EXPECT_FALSE(base::Contains(va_info, VAProfileH264Baseline));
}

// Verifies that every VAProfile from VaapiWrapper::GetSupportedDecodeProfiles()
// is indeed supported by the command line vainfo utility and by
// VaapiWrapper::IsDecodeSupported().
TEST_F(VaapiTest, GetSupportedDecodeProfiles) {
  const auto va_info = RetrieveVAInfoOutput();

  for (const auto& profile : VaapiWrapper::GetSupportedDecodeProfiles()) {
    const auto va_profile = ConvertToVAProfile(profile.profile);
    ASSERT_TRUE(va_profile.has_value());

    EXPECT_TRUE(base::Contains(va_info.at(*va_profile), VAEntrypointVLD))
        << " profile: " << GetProfileName(profile.profile)
        << ", va profile: " << vaProfileStr(*va_profile);
    EXPECT_TRUE(VaapiWrapper::IsDecodeSupported(*va_profile))
        << " profile: " << GetProfileName(profile.profile)
        << ", va profile: " << vaProfileStr(*va_profile);
  }
}

// Verifies that every VAProfile from VaapiWrapper::GetSupportedEncodeProfiles()
// is indeed supported by the command line vainfo utility.
TEST_F(VaapiTest, GetSupportedEncodeProfiles) {
  const auto va_info = RetrieveVAInfoOutput();

  for (const auto& profile : VaapiWrapper::GetSupportedEncodeProfiles()) {
    const auto va_profile = ConvertToVAProfile(profile.profile);
    ASSERT_TRUE(va_profile.has_value());

    EXPECT_TRUE(base::Contains(va_info.at(*va_profile), VAEntrypointEncSlice) ||
                base::Contains(va_info.at(*va_profile), VAEntrypointEncSliceLP))
        << " profile: " << GetProfileName(profile.profile)
        << ", va profile: " << vaProfileStr(*va_profile);
  }
}

#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
// Verifies that VAProfileProtected is indeed supported by the command line
// vainfo utility.
TEST_F(VaapiTest, VaapiProfileProtected) {
  const auto va_info = RetrieveVAInfoOutput();

  EXPECT_TRUE(base::Contains(va_info.at(VAProfileProtected),
                             VAEntrypointProtectedContent))
      << ", va profile: " << vaProfileStr(VAProfileProtected);
}
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)

// Verifies that if JPEG decoding and encoding are supported by VaapiWrapper,
// they are also supported by by the command line vainfo utility.
TEST_F(VaapiTest, VaapiProfilesJPEG) {
  const auto va_info = RetrieveVAInfoOutput();

  EXPECT_EQ(VaapiWrapper::IsDecodeSupported(VAProfileJPEGBaseline),
            base::Contains(va_info.at(VAProfileJPEGBaseline), VAEntrypointVLD));
  EXPECT_EQ(VaapiWrapper::IsJpegEncodeSupported(),
            base::Contains(va_info.at(VAProfileJPEGBaseline),
                           VAEntrypointEncPicture));
}

// Verifies that the default VAEntrypoint as per VaapiWrapper is indeed among
// the supported ones.
TEST_F(VaapiTest, DefaultEntrypointIsSupported) {
  for (size_t i = 0; i < VaapiWrapper::kCodecModeMax; ++i) {
    const auto wrapper_mode = static_cast<VaapiWrapper::CodecMode>(i);
    std::map<VAProfile, std::vector<VAEntrypoint>> configurations =
        VaapiWrapper::GetSupportedConfigurationsForCodecModeForTesting(
            wrapper_mode);
    for (const auto& profile_and_entrypoints : configurations) {
      const VAEntrypoint default_entrypoint =
          VaapiWrapper::GetDefaultVaEntryPoint(wrapper_mode,
                                               profile_and_entrypoints.first);
      const auto& supported_entrypoints = profile_and_entrypoints.second;
      EXPECT_TRUE(base::Contains(supported_entrypoints, default_entrypoint))
          << "Default VAEntrypoint " << vaEntrypointStr(default_entrypoint)
          << " (VaapiWrapper mode = " << wrapper_mode
          << ") is not supported for "
          << vaProfileStr(profile_and_entrypoints.first);
    }
  }
}

// Verifies that VaapiWrapper::CreateContext() will queue up a buffer to set the
// encoder to its lowest quality setting if a given VAProfile and VAEntrypoint
// claims to support configuring it.
TEST_F(VaapiTest, LowQualityEncodingSetting) {
  // This test only applies to low powered Intel processors.
  constexpr int kPentiumAndLaterFamily = 0x06;
  const base::CPU cpuid;
  const bool is_core_y_processor =
      base::MatchPattern(cpuid.cpu_brand(), "Intel(R) Core(TM) *Y CPU*");
  const bool is_low_power_intel =
      cpuid.family() == kPentiumAndLaterFamily &&
      (base::Contains(cpuid.cpu_brand(), "Pentium") ||
       base::Contains(cpuid.cpu_brand(), "Celeron") || is_core_y_processor);
  if (!is_low_power_intel)
    GTEST_SKIP() << "Not an Intel low power processor";

  std::map<VAProfile, std::vector<VAEntrypoint>> configurations =
      VaapiWrapper::GetSupportedConfigurationsForCodecModeForTesting(
          VaapiWrapper::kEncodeConstantBitrate);

  for (const auto& codec_mode :
       {VaapiWrapper::kEncodeConstantBitrate,
        VaapiWrapper::kEncodeConstantQuantizationParameter}) {
    std::map<VAProfile, std::vector<VAEntrypoint>> configurations =
        VaapiWrapper::GetSupportedConfigurationsForCodecModeForTesting(
            codec_mode);

    for (const auto& profile_and_entrypoints : configurations) {
      const VAProfile va_profile = profile_and_entrypoints.first;
      scoped_refptr<VaapiWrapper> wrapper = VaapiWrapper::Create(
          VaapiWrapper::kEncodeConstantBitrate, va_profile,
          EncryptionScheme::kUnencrypted, base::DoNothing());

      // Depending on the GPU Gen, flags and policies, we may or may not utilize
      // all entrypoints (e.g. we might always want VAEntrypointEncSliceLP if
      // supported and enabled). Query VaapiWrapper's mandated entry point.
      const VAEntrypoint entrypoint =
          VaapiWrapper::GetDefaultVaEntryPoint(codec_mode, va_profile);
      ASSERT_TRUE(base::Contains(profile_and_entrypoints.second, entrypoint));

      VAConfigAttrib attrib{};
      attrib.type = VAConfigAttribEncQualityRange;
      {
        base::AutoLock auto_lock(*wrapper->va_lock_);
        VAStatus va_res = vaGetConfigAttributes(
            wrapper->va_display_, va_profile, entrypoint, &attrib, 1);
        ASSERT_EQ(va_res, VA_STATUS_SUCCESS);
      }
      const auto quality_level = attrib.value;
      if (quality_level == VA_ATTRIB_NOT_SUPPORTED || quality_level <= 1u)
        continue;
      DLOG(INFO) << vaProfileStr(va_profile)
                 << " supports encoding quality setting, with max value "
                 << quality_level;

      // If we get here it means the |va_profile| and |entrypoint| support
      // the quality setting. We cannot inspect what the driver does with this
      // number (it could ignore it), so instead just make sure there's a
      // |pending_va_buffers_| that, when mapped, looks correct. That buffer
      // should be created by CreateContext().
      ASSERT_TRUE(wrapper->CreateContext(gfx::Size(640, 368)));
      ASSERT_EQ(wrapper->pending_va_buffers_.size(), 1u);
      {
        base::AutoLock auto_lock(*wrapper->va_lock_);
        ScopedVABufferMapping mapping(wrapper->va_lock_, wrapper->va_display_,
                                      wrapper->pending_va_buffers_.front());
        ASSERT_TRUE(mapping.IsValid());

        auto* const va_buffer =
            reinterpret_cast<VAEncMiscParameterBuffer*>(mapping.data());
        EXPECT_EQ(va_buffer->type, VAEncMiscParameterTypeQualityLevel);

        auto* const enc_quality =
            reinterpret_cast<VAEncMiscParameterBufferQualityLevel*>(
                va_buffer->data);
        EXPECT_EQ(enc_quality->quality_level, quality_level)
            << vaProfileStr(va_profile) << " " << vaEntrypointStr(entrypoint);
      }
    }
  }
}

// This test checks the supported SVC scalability mode.
TEST_F(VaapiTest, CheckSupportedSVCScalabilityModes) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const std::vector<SVCScalabilityMode> kSupportedTemporalSVC = {
      SVCScalabilityMode::kL1T2, SVCScalabilityMode::kL1T3};
  const std::vector<SVCScalabilityMode> kSupportedTemporalAndKeySVC = {
      SVCScalabilityMode::kL1T2,    SVCScalabilityMode::kL1T3,
      SVCScalabilityMode::kL2T2Key, SVCScalabilityMode::kL2T3Key,
      SVCScalabilityMode::kL3T2Key, SVCScalabilityMode::kL3T3Key};
#endif

  const auto scalability_modes_vp9_profile0 =
      VaapiWrapper::GetSupportedScalabilityModes(VP9PROFILE_PROFILE0,
                                                 VAProfileVP9Profile0);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(kVaapiVp9kSVCHWEncoding) &&
      VaapiWrapper::GetDefaultVaEntryPoint(
          VaapiWrapper::kEncodeConstantQuantizationParameter,
          VAProfileVP9Profile0) == VAEntrypointEncSliceLP) {
    EXPECT_EQ(scalability_modes_vp9_profile0, kSupportedTemporalAndKeySVC);
  } else {
    EXPECT_EQ(scalability_modes_vp9_profile0, kSupportedTemporalSVC);
  }
#else
  EXPECT_TRUE(scalability_modes_vp9_profile0.empty());
#endif

  const auto scalability_modes_vp9_profile2 =
      VaapiWrapper::GetSupportedScalabilityModes(VP9PROFILE_PROFILE2,
                                                 VAProfileVP9Profile2);
  EXPECT_TRUE(scalability_modes_vp9_profile2.empty());

  const auto scalability_modes_h264_baseline =
      VaapiWrapper::GetSupportedScalabilityModes(
          H264PROFILE_BASELINE, VAProfileH264ConstrainedBaseline);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(b/199487660): Enable H.264 temporal layer encoding on AMD once their
  // drivers support them.
  const auto implementation = VaapiWrapper::GetImplementationType();
  if (base::FeatureList::IsEnabled(kVaapiH264TemporalLayerHWEncoding) &&
      (implementation == VAImplementation::kIntelI965 ||
       implementation == VAImplementation::kIntelIHD)) {
    EXPECT_EQ(scalability_modes_h264_baseline, kSupportedTemporalSVC);
  } else {
    EXPECT_TRUE(scalability_modes_h264_baseline.empty());
  }
#else
  EXPECT_TRUE(scalability_modes_h264_baseline.empty());
#endif
}

class VaapiVppTest
    : public VaapiTest,
      public testing::WithParamInterface<std::tuple<uint32_t, uint32_t>> {
 public:
  VaapiVppTest() = default;
  ~VaapiVppTest() override = default;

  // Populate meaningful test suffixes instead of /0, /1, etc.
  struct PrintToStringParamName {
    template <class ParamType>
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      std::stringstream ss;
      ss << FourccToString(std::get<0>(info.param)) << "_to_"
         << FourccToString(std::get<1>(info.param));
      return ss.str();
    }
  };
};

TEST_P(VaapiVppTest, BlitWithVAAllocatedSurfaces) {
  const uint32_t va_fourcc_in = std::get<0>(GetParam());
  const uint32_t va_fourcc_out = std::get<1>(GetParam());

  // TODO(b/187852384): enable the other two backends.
  if (VaapiWrapper::GetImplementationType() != VAImplementation::kIntelIHD)
    GTEST_SKIP() << "backend not supported";

  if (!VaapiWrapper::IsVppFormatSupported(va_fourcc_in) ||
      !VaapiWrapper::IsVppFormatSupported(va_fourcc_out)) {
    GTEST_SKIP() << FourccToString(va_fourcc_in) << " -> "
                 << FourccToString(va_fourcc_out) << " not supported";
  }
  constexpr gfx::Size kInputSize(640, 320);
  constexpr gfx::Size kOutputSize(320, 180);
  ASSERT_TRUE(VaapiWrapper::IsVppResolutionAllowed(kInputSize));
  ASSERT_TRUE(VaapiWrapper::IsVppResolutionAllowed(kOutputSize));

  auto wrapper =
      VaapiWrapper::Create(VaapiWrapper::kVideoProcess, VAProfileNone,
                           EncryptionScheme::kUnencrypted, base::DoNothing());
  ASSERT_TRUE(!!wrapper);
  // Size is unnecessary for a VPP context.
  ASSERT_TRUE(wrapper->CreateContext(gfx::Size()));

  const unsigned int va_rt_format_in = ToVaRTFormat(va_fourcc_in);
  ASSERT_NE(va_rt_format_in, kInvalidVaRtFormat);
  const unsigned int va_rt_format_out = ToVaRTFormat(va_fourcc_out);
  ASSERT_NE(va_rt_format_out, kInvalidVaRtFormat);

  auto scoped_surfaces = wrapper->CreateScopedVASurfaces(
      va_rt_format_in, kInputSize, {VaapiWrapper::SurfaceUsageHint::kGeneric},
      1u, /*visible_size=*/absl::nullopt, /*va_fourcc=*/absl::nullopt);
  ASSERT_FALSE(scoped_surfaces.empty());
  std::unique_ptr<ScopedVASurface> scoped_surface_in =
      std::move(scoped_surfaces[0]);

  scoped_surfaces = wrapper->CreateScopedVASurfaces(
      va_rt_format_out, kOutputSize, {VaapiWrapper::SurfaceUsageHint::kGeneric},
      1u, /*visible_size=*/absl::nullopt, /*va_fourcc=*/absl::nullopt);
  ASSERT_FALSE(scoped_surfaces.empty());
  std::unique_ptr<ScopedVASurface> scoped_surface_out =
      std::move(scoped_surfaces[0]);

  scoped_refptr<VASurface> surface_in = base::MakeRefCounted<VASurface>(
      scoped_surface_in->id(), kInputSize, va_rt_format_in, base::DoNothing());
  scoped_refptr<VASurface> surface_out =
      base::MakeRefCounted<VASurface>(scoped_surface_out->id(), kOutputSize,
                                      va_rt_format_out, base::DoNothing());

  ASSERT_TRUE(wrapper->BlitSurface(*surface_in, *surface_out,
                                   gfx::Rect(kInputSize),
                                   gfx::Rect(kOutputSize), VIDEO_ROTATION_0));
  ASSERT_TRUE(wrapper->SyncSurface(scoped_surface_out->id()));
  wrapper->DestroyContext();
}

// TODO(b/187852384): Consider adding more VaapiVppTest cases, e.g. crops.

// Note: vaCreateSurfaces() uses the RT version of the Four CC, so we don't need
// to consider swizzlings, since they'll end up mapped to the same RT format.
constexpr uint32_t kVAFourCCs[] = {VA_FOURCC_I420, VA_FOURCC_YUY2,
                                   VA_FOURCC_RGBA, VA_FOURCC_P010};

INSTANTIATE_TEST_SUITE_P(,
                         VaapiVppTest,
                         ::testing::Combine(::testing::ValuesIn(kVAFourCCs),
                                            ::testing::ValuesIn(kVAFourCCs)),
                         VaapiVppTest::PrintToStringParamName());

class VaapiMinigbmTest
    : public VaapiTest,
      public testing::WithParamInterface<
          std::tuple<VAProfile, unsigned int /*va_rt_format*/, gfx::Size>> {
 public:
  VaapiMinigbmTest() = default;
  ~VaapiMinigbmTest() override = default;

  // Populate meaningful test suffixes instead of /0, /1, etc.
  struct PrintToStringParamName {
    template <class ParamType>
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      // Using here vaProfileStr(std::get<0>(info.param)) crashes the binary.
      // TODO(mcasas): investigate why and use it instead of codec%d.
      return base::StringPrintf(
          "%s__%s__%s", VAProfileToString(std::get<0>(info.param)),
          VARTFormatToString(std::get<1>(info.param)).c_str(),
          std::get<2>(info.param).ToString().c_str());
    }
  };
};

// This test allocates a VASurface (via VaapiWrapper) for the given VAProfile,
// VA RT Format and resolution (as per the test parameters). It then verifies
// that said VASurface's metadata (e.g. width, height, number of planes, pitch)
// are the same as those we would allocate via minigbm.
TEST_P(VaapiMinigbmTest, AllocateAndCompareWithMinigbm) {
  const VAProfile va_profile = std::get<0>(GetParam());
  const unsigned int va_rt_format = std::get<1>(GetParam());
  const gfx::Size resolution = std::get<2>(GetParam());

  // TODO(b/187852384): enable the other backends.
  if (VaapiWrapper::GetImplementationType() != VAImplementation::kIntelIHD)
    GTEST_SKIP() << "backend not supported";

  ASSERT_NE(va_rt_format, kInvalidVaRtFormat);
  if (!VaapiWrapper::IsDecodeSupported(va_profile))
    GTEST_SKIP() << vaProfileStr(va_profile) << " not supported.";

  if (!VaapiWrapper::IsDecodingSupportedForInternalFormat(va_profile,
                                                          va_rt_format)) {
    GTEST_SKIP() << VARTFormatToString(va_rt_format) << " not supported.";
  }

  gfx::Size minimum_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetDecodeMinResolution(va_profile,
                                                   &minimum_supported_size));
  gfx::Size maximum_supported_size;
  ASSERT_TRUE(VaapiWrapper::GetDecodeMaxResolution(va_profile,
                                                   &maximum_supported_size));

  if (resolution.width() < minimum_supported_size.width() ||
      resolution.height() < minimum_supported_size.height() ||
      resolution.width() > maximum_supported_size.width() ||
      resolution.height() > maximum_supported_size.height()) {
    GTEST_SKIP() << resolution.ToString()
                 << " not supported (min: " << minimum_supported_size.ToString()
                 << ", max: " << maximum_supported_size.ToString() << ")";
  }

  auto wrapper =
      VaapiWrapper::Create(VaapiWrapper::kDecode, va_profile,
                           EncryptionScheme::kUnencrypted, base::DoNothing());
  ASSERT_TRUE(!!wrapper);
  ASSERT_TRUE(wrapper->CreateContext(resolution));

  auto scoped_surfaces = wrapper->CreateScopedVASurfaces(
      va_rt_format, resolution, {VaapiWrapper::SurfaceUsageHint::kVideoDecoder},
      1u,
      /*visible_size=*/absl::nullopt, /*va_fourcc=*/absl::nullopt);
  ASSERT_FALSE(scoped_surfaces.empty());
  const auto scoped_va_surface = std::move(scoped_surfaces[0]);
  wrapper->DestroyContext();

  ASSERT_TRUE(scoped_va_surface->IsValid());
  EXPECT_EQ(scoped_va_surface->format(), va_rt_format);

  // Request the underlying DRM metadata for |scoped_va_surface|.
  VADRMPRIMESurfaceDescriptor va_descriptor{};
  {
    base::AutoLock auto_lock(*wrapper->va_lock_);
    VAStatus va_res =
        vaSyncSurface(wrapper->va_display_, scoped_va_surface->id());
    ASSERT_EQ(va_res, VA_STATUS_SUCCESS);
    va_res = vaExportSurfaceHandle(
        wrapper->va_display_, scoped_va_surface->id(),
        VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
        VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
        &va_descriptor);
    ASSERT_EQ(va_res, VA_STATUS_SUCCESS);
  }

  // Verify some expected properties of the allocated VASurface. We expect a
  // single |object|, with a number of |layers| of the same |pitch|.
  EXPECT_EQ(scoped_va_surface->size(),
            gfx::Size(base::checked_cast<int>(va_descriptor.width),
                      base::checked_cast<int>(va_descriptor.height)));

  const auto va_fourcc = ToVaFourcc(va_rt_format);
  ASSERT_NE(va_fourcc, base::checked_cast<unsigned int>(DRM_FORMAT_INVALID));
  EXPECT_EQ(va_descriptor.fourcc, va_fourcc)
      << FourccToString(va_descriptor.fourcc)
      << " != " << FourccToString(va_fourcc);
  EXPECT_EQ(va_descriptor.num_objects, 1u);
  // TODO(mcasas): consider comparing |size| with a better estimate of the
  // |scoped_va_surface| memory footprint (e.g. including planes and format).
  EXPECT_GE(va_descriptor.objects[0].size,
            base::checked_cast<uint32_t>(scoped_va_surface->size().GetArea()));
  EXPECT_EQ(va_descriptor.objects[0].drm_format_modifier,
            I915_FORMAT_MOD_Y_TILED);
  // TODO(mcasas): |num_layers| actually depends on |va_descriptor.va_fourcc|.
  EXPECT_EQ(va_descriptor.num_layers, 2u);
  for (uint32_t i = 0; i < va_descriptor.num_layers; ++i) {
    EXPECT_EQ(va_descriptor.layers[i].num_planes, 1u);
    EXPECT_EQ(va_descriptor.layers[i].object_index[0], 0u);

    DVLOG(2) << "plane " << i
             << ", pitch: " << va_descriptor.layers[i].pitch[0];
    // Luma and chroma planes have different |pitch| expectations.
    // TODO(mcasas): consider bitdepth for pitch lower thresholds.
    if (i == 0) {
      EXPECT_GE(
          va_descriptor.layers[i].pitch[0],
          base::checked_cast<uint32_t>(scoped_va_surface->size().width()));
    } else {
      const auto expected_rounded_up_pitch =
          base::bits::AlignUp(scoped_va_surface->size().width(), 2);
      EXPECT_GE(va_descriptor.layers[i].pitch[0],
                base::checked_cast<uint32_t>(expected_rounded_up_pitch));
    }
  }

  // Now open minigbm pointing to the DRM primary node, allocate a gbm_bo, and
  // compare its width/height/stride/etc with the |va_descriptor|s.
  base::File drm_fd(
      base::FilePath("/dev/dri/card0"),
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);

  ASSERT_TRUE(drm_fd.IsValid());
  struct gbm_device* gbm = gbm_create_device(drm_fd.GetPlatformFile());
  ASSERT_TRUE(gbm);

  const auto gbm_format = ToGBMFormat(va_rt_format);
  ASSERT_NE(gbm_format, DRM_FORMAT_INVALID);
  const auto bo_use_flags = GBM_BO_USE_TEXTURING | GBM_BO_USE_HW_VIDEO_DECODER;
  struct gbm_bo* bo =
      gbm_bo_create(gbm, resolution.width(), resolution.height(), gbm_format,
                    bo_use_flags | GBM_BO_USE_SCANOUT);
  if (!bo) {
    // Try again without the scanout flag. This reproduces Chrome's behaviour.
    bo = gbm_bo_create(gbm, resolution.width(), resolution.height(), gbm_format,
                       bo_use_flags);
  }
  ASSERT_TRUE(bo);
  EXPECT_EQ(scoped_va_surface->size(),
            gfx::Size(base::checked_cast<int>(gbm_bo_get_width(bo)),
                      base::checked_cast<int>(gbm_bo_get_height(bo))));

  const int bo_num_planes = gbm_bo_get_plane_count(bo);
  ASSERT_EQ(va_descriptor.num_layers,
            base::checked_cast<uint32_t>(bo_num_planes));
  for (int i = 0; i < bo_num_planes; ++i) {
    EXPECT_EQ(va_descriptor.layers[i].pitch[0],
              gbm_bo_get_stride_for_plane(bo, i));
  }

  // TODO(mcasas): consider comparing |va_descriptor.objects[0].size| with |bo|s
  // size (as returned by lseek()ing it).

  gbm_bo_destroy(bo);
  gbm_device_destroy(gbm);
}

constexpr VAProfile kVACodecProfiles[] = {
    VAProfileVP8Version0_3, VAProfileH264ConstrainedBaseline,
    VAProfileVP9Profile0,   VAProfileVP9Profile2,
    VAProfileAV1Profile0,   VAProfileJPEGBaseline};
constexpr uint32_t kVARTFormatsForGBM[] = {VA_RT_FORMAT_YUV420,
                                           VA_RT_FORMAT_YUV420_10};
constexpr gfx::Size kResolutions[] = {
    // clang-format off
    gfx::Size(127, 127),
    gfx::Size(128, 128),
    gfx::Size(129, 129),
    gfx::Size(320, 180),
    gfx::Size(320, 240),  // QVGA
    gfx::Size(323, 243),
    gfx::Size(480, 320),  // 3/4 VGA
    gfx::Size(640, 360),  // VGA
    gfx::Size(640, 480),
    gfx::Size(1280, 720)};
// clang-format on

INSTANTIATE_TEST_SUITE_P(
    ,
    VaapiMinigbmTest,
    ::testing::Combine(::testing::ValuesIn(kVACodecProfiles),
                       ::testing::ValuesIn(kVARTFormatsForGBM),
                       ::testing::ValuesIn(kResolutions)),
    VaapiMinigbmTest::PrintToStringParamName());

}  // namespace media

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  {
    // Enables/Disables features during PreSandboxInitialization(). We have to
    // destruct ScopedFeatureList after it because base::TestSuite::Run()
    // creates a ScopedFeatureList and multiple concurrent ScopedFeatureLists
    // are not allowed.
    auto scoped_feature_list = media::CreateScopedFeatureList();
    // PreSandboxInitialization() loads and opens the driver, queries its
    // capabilities and fills in the VASupportedProfiles.
    media::VaapiWrapper::PreSandboxInitialization();
  }

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
