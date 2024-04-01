// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/supported_profile_helpers.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include <d3d9.h>
#include <dxva2api.h>

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "base/win/windows_version.h"
#include "media/base/media_switches.h"
#include "media/gpu/windows/av1_guids.h"

namespace {

// R600, R700, Evergreen and Cayman AMD cards. These support DXVA via UVD3
// or earlier, and don't handle resolutions higher than 1920 x 1088 well.
//
// NOTE: This list must be kept in sorted order.
constexpr uint16_t kLegacyAmdGpuList[] = {
    0x130f, 0x6700, 0x6701, 0x6702, 0x6703, 0x6704, 0x6705, 0x6706, 0x6707,
    0x6708, 0x6709, 0x6718, 0x6719, 0x671c, 0x671d, 0x671f, 0x6720, 0x6721,
    0x6722, 0x6723, 0x6724, 0x6725, 0x6726, 0x6727, 0x6728, 0x6729, 0x6738,
    0x6739, 0x673e, 0x6740, 0x6741, 0x6742, 0x6743, 0x6744, 0x6745, 0x6746,
    0x6747, 0x6748, 0x6749, 0x674a, 0x6750, 0x6751, 0x6758, 0x6759, 0x675b,
    0x675d, 0x675f, 0x6760, 0x6761, 0x6762, 0x6763, 0x6764, 0x6765, 0x6766,
    0x6767, 0x6768, 0x6770, 0x6771, 0x6772, 0x6778, 0x6779, 0x677b, 0x6798,
    0x67b1, 0x6821, 0x683d, 0x6840, 0x6841, 0x6842, 0x6843, 0x6849, 0x6850,
    0x6858, 0x6859, 0x6880, 0x6888, 0x6889, 0x688a, 0x688c, 0x688d, 0x6898,
    0x6899, 0x689b, 0x689c, 0x689d, 0x689e, 0x68a0, 0x68a1, 0x68a8, 0x68a9,
    0x68b0, 0x68b8, 0x68b9, 0x68ba, 0x68be, 0x68bf, 0x68c0, 0x68c1, 0x68c7,
    0x68c8, 0x68c9, 0x68d8, 0x68d9, 0x68da, 0x68de, 0x68e0, 0x68e1, 0x68e4,
    0x68e5, 0x68e8, 0x68e9, 0x68f1, 0x68f2, 0x68f8, 0x68f9, 0x68fa, 0x68fe,
    0x9400, 0x9401, 0x9402, 0x9403, 0x9405, 0x940a, 0x940b, 0x940f, 0x9440,
    0x9441, 0x9442, 0x9443, 0x9444, 0x9446, 0x944a, 0x944b, 0x944c, 0x944e,
    0x9450, 0x9452, 0x9456, 0x945a, 0x945b, 0x945e, 0x9460, 0x9462, 0x946a,
    0x946b, 0x947a, 0x947b, 0x9480, 0x9487, 0x9488, 0x9489, 0x948a, 0x948f,
    0x9490, 0x9491, 0x9495, 0x9498, 0x949c, 0x949e, 0x949f, 0x94a0, 0x94a1,
    0x94a3, 0x94b1, 0x94b3, 0x94b4, 0x94b5, 0x94b9, 0x94c0, 0x94c1, 0x94c3,
    0x94c4, 0x94c5, 0x94c6, 0x94c7, 0x94c8, 0x94c9, 0x94cb, 0x94cc, 0x94cd,
    0x9500, 0x9501, 0x9504, 0x9505, 0x9506, 0x9507, 0x9508, 0x9509, 0x950f,
    0x9511, 0x9515, 0x9517, 0x9519, 0x9540, 0x9541, 0x9542, 0x954e, 0x954f,
    0x9552, 0x9553, 0x9555, 0x9557, 0x955f, 0x9580, 0x9581, 0x9583, 0x9586,
    0x9587, 0x9588, 0x9589, 0x958a, 0x958b, 0x958c, 0x958d, 0x958e, 0x958f,
    0x9590, 0x9591, 0x9593, 0x9595, 0x9596, 0x9597, 0x9598, 0x9599, 0x959b,
    0x95c0, 0x95c2, 0x95c4, 0x95c5, 0x95c6, 0x95c7, 0x95c9, 0x95cc, 0x95cd,
    0x95ce, 0x95cf, 0x9610, 0x9611, 0x9612, 0x9613, 0x9614, 0x9615, 0x9616,
    0x9640, 0x9641, 0x9642, 0x9643, 0x9644, 0x9645, 0x9647, 0x9648, 0x9649,
    0x964a, 0x964b, 0x964c, 0x964e, 0x964f, 0x9710, 0x9711, 0x9712, 0x9713,
    0x9714, 0x9715, 0x9802, 0x9803, 0x9804, 0x9805, 0x9806, 0x9807, 0x9808,
    0x9809, 0x980a, 0x9830, 0x983d, 0x9850, 0x9851, 0x9874, 0x9900, 0x9901,
    0x9903, 0x9904, 0x9905, 0x9906, 0x9907, 0x9908, 0x9909, 0x990a, 0x990b,
    0x990c, 0x990d, 0x990e, 0x990f, 0x9910, 0x9913, 0x9917, 0x9918, 0x9919,
    0x9990, 0x9991, 0x9992, 0x9993, 0x9994, 0x9995, 0x9996, 0x9997, 0x9998,
    0x9999, 0x999a, 0x999b, 0x999c, 0x999d, 0x99a0, 0x99a2, 0x99a4};

// Legacy Intel GPUs which have trouble even querying if resolutions higher than
// 1920 x 1088 are supported. Updated based on crash reports.
//
// NOTE: This list must be kept in sorted order.
constexpr uint16_t kLegacyIntelGpuList[] = {
    0x102, 0x106, 0x116, 0x126, 0x152, 0x156, 0x166,
    0x402, 0x406, 0x416, 0x41e, 0xa06, 0xa16, 0xf31,
};

// Windows Media Foundation H.264 decoding does not support decoding videos
// with any dimension smaller than 48 pixels:
// http://msdn.microsoft.com/en-us/library/windows/desktop/dd797815
//
// TODO(dalecurtis): These values are too low. We should only be using
// hardware decode for videos above ~360p, see http://crbug.com/684792.
constexpr gfx::Size kMinResolution(64, 64);

// Certain AMD GPU drivers like R600, R700, Evergreen and Cayman and some second
// generation Intel GPU drivers crash if we create a video device with a
// resolution higher then 1920 x 1088. This function checks if the GPU is in
// this list and if yes returns true.
bool IsLegacyGPU(ID3D11Device* device) {
  DCHECK(std::is_sorted(std::begin(kLegacyAmdGpuList),
                        std::end(kLegacyAmdGpuList)));
  DCHECK(std::is_sorted(std::begin(kLegacyIntelGpuList),
                        std::end(kLegacyIntelGpuList)));

  constexpr int kAMDGPUId1 = 0x1002;
  constexpr int kAMDGPUId2 = 0x1022;
  constexpr int kIntelGPU = 0x8086;

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&dxgi_device));
  if (FAILED(hr))
    return true;

  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  hr = dxgi_device->GetAdapter(&adapter);
  if (FAILED(hr))
    return true;

  DXGI_ADAPTER_DESC adapter_desc = {};
  hr = adapter->GetDesc(&adapter_desc);
  if (FAILED(hr))
    return true;

  // All the values in the legacy gpu list are uint16_t.
  if (adapter_desc.DeviceId > std::numeric_limits<uint16_t>::max())
    return false;

  const uint16_t device_id = adapter_desc.DeviceId;

  // We check if the device is an Intel or an AMD device and whether it is in
  // the global list defined by the kLegacyAmdGpuList and kLegacyIntelGpuList
  // arrays above. If yes then the device is treated as a legacy device.
  if (adapter_desc.VendorId == kAMDGPUId1 ||
      adapter_desc.VendorId == kAMDGPUId2) {
    if (std::binary_search(std::begin(kLegacyAmdGpuList),
                           std::end(kLegacyAmdGpuList), device_id)) {
      return true;
    }
  } else if (adapter_desc.VendorId == kIntelGPU) {
    if (std::binary_search(std::begin(kLegacyIntelGpuList),
                           std::end(kLegacyIntelGpuList), device_id)) {
      return true;
    }
  }

  return false;
}

bool IsResolutionSupportedForDevice(const gfx::Size& resolution_to_test,
                                    const GUID& decoder_guid,
                                    ID3D11VideoDevice* video_device,
                                    DXGI_FORMAT format) {
  D3D11_VIDEO_DECODER_DESC desc = {
      decoder_guid,                                    // Guid
      static_cast<UINT>(resolution_to_test.width()),   // SampleWidth
      static_cast<UINT>(resolution_to_test.height()),  // SampleHeight
      format                                           // OutputFormat
  };

  // We've chosen the least expensive test for identifying if a given resolution
  // is supported. Actually creating the VideoDecoder instance only fails ~0.4%
  // of the time and the outcome is that we will offer support and then
  // immediately fall back to software; e.g., playback still works. Since these
  // calls can take hundreds of milliseconds to complete and are often executed
  // during startup, this seems a reasonably trade off.
  //
  // See the deprecated histograms Media.DXVAVDA.GetDecoderConfigStatus which
  // succeeds 100% of the time and Media.DXVAVDA.CreateDecoderStatus which
  // only succeeds 99.6% of the time (in a 28 day aggregation).
  UINT config_count;
  return SUCCEEDED(
             video_device->GetVideoDecoderConfigCount(&desc, &config_count)) &&
         config_count > 0;
}

media::SupportedResolutionRange GetResolutionsForGUID(
    ID3D11VideoDevice* video_device,
    const GUID& decoder_guid,
    const std::vector<gfx::Size>& resolutions_to_test,
    DXGI_FORMAT format = DXGI_FORMAT_NV12,
    const gfx::Size& min_resolution = kMinResolution) {
  media::SupportedResolutionRange result;

  // Verify input is in ascending order by height.
  DCHECK(std::is_sorted(resolutions_to_test.begin(), resolutions_to_test.end(),
                        [](const gfx::Size& a, const gfx::Size& b) {
                          return a.height() < b.height();
                        }));

  for (const auto& res : resolutions_to_test) {
    if (!IsResolutionSupportedForDevice(res, decoder_guid, video_device,
                                        format)) {
      break;
    }
    result.max_landscape_resolution = res;
  }

  // The max supported portrait resolution should be just be a w/h flip of the
  // max supported landscape resolution.
  const gfx::Size flipped(result.max_landscape_resolution.height(),
                          result.max_landscape_resolution.width());
  if (flipped == result.max_landscape_resolution ||
      IsResolutionSupportedForDevice(flipped, decoder_guid, video_device,
                                     format)) {
    result.max_portrait_resolution = flipped;
  }

  if (!result.max_landscape_resolution.IsEmpty())
    result.min_resolution = min_resolution;

  return result;
}

}  // namespace

namespace media {

SupportedResolutionRangeMap GetSupportedD3D11VideoDecoderResolutions(
    ComD3D11Device device,
    const gpu::GpuDriverBugWorkarounds& workarounds,
    bool provide_av1_resolutions) {
  TRACE_EVENT0("gpu,startup", "GetSupportedD3D11VideoDecoderResolutions");
  SupportedResolutionRangeMap supported_resolutions;

  // We always insert support for H.264 regardless of the tests below. It's old
  // enough to be ubiquitous.
  //
  // On Windows 7 the maximum resolution supported by media foundation is
  // 1920 x 1088. We use 1088 to account for 16x16 macro-blocks.
  constexpr gfx::Size kDefaultMaxH264Resolution(1920, 1088);
  SupportedResolutionRange h264_profile;
  h264_profile.min_resolution = kMinResolution;
  h264_profile.max_landscape_resolution = kDefaultMaxH264Resolution;

  // We don't have a way to map DXVA support to specific H.264 profiles, so just
  // mark all the common ones with the same level of support.
  constexpr VideoCodecProfile kSupportedH264Profiles[] = {
      H264PROFILE_BASELINE, H264PROFILE_MAIN, H264PROFILE_HIGH};
  for (const auto profile : kSupportedH264Profiles)
    supported_resolutions[profile] = h264_profile;

  if (base::win::GetVersion() <= base::win::Version::WIN7)
    return supported_resolutions;

  // To detect if a driver supports the desired resolutions, we try and create
  // a DXVA decoder instance for that resolution and profile. If that succeeds
  // we assume that the driver supports decoding for that resolution.
  // Legacy AMD drivers with UVD3 or earlier and some Intel GPU's crash while
  // creating surfaces larger than 1920 x 1088.
  if (!device || IsLegacyGPU(device.Get()))
    return supported_resolutions;

  ComD3D11VideoDevice video_device;
  if (FAILED(device.As(&video_device)))
    return supported_resolutions;

  const std::vector<gfx::Size> kModernResolutions = {
      gfx::Size(4096, 2160), gfx::Size(4096, 2304), gfx::Size(7680, 4320),
      gfx::Size(8192, 4320), gfx::Size(8192, 8192)};

  const bool should_test_for_av1_support =
      (base::FeatureList::IsEnabled(kMediaFoundationAV1Decoding) ||
       base::FeatureList::IsEnabled(kD3D11VideoDecoderAV1)) &&
      !workarounds.disable_accelerated_av1_decode && provide_av1_resolutions;

  // Enumerate supported video profiles and look for the known profile for each
  // codec. We first look through the the decoder profiles so we don't run N
  // resolution tests for a profile that's unsupported.
  UINT profile_count = video_device->GetVideoDecoderProfileCount();
  for (UINT i = 0; i < profile_count; i++) {
    GUID profile_id;
    if (FAILED(video_device->GetVideoDecoderProfile(i, &profile_id)))
      continue;

    if (profile_id == D3D11_DECODER_PROFILE_H264_VLD_NOFGT) {
      const auto result = GetResolutionsForGUID(
          video_device.Get(), profile_id,
          {gfx::Size(2560, 1440), gfx::Size(3840, 2160), gfx::Size(4096, 2160),
           gfx::Size(4096, 2304), gfx::Size(4096, 4096)});

      // Unlike the other codecs, H.264 support is assumed up to 1080p, even if
      // our initial queries fail. If they fail, we use the defaults set above.
      if (!result.max_landscape_resolution.IsEmpty()) {
        for (const auto profile : kSupportedH264Profiles)
          supported_resolutions[profile] = result;
      }
      continue;
    }

    // Note: Each bit depth of AV1 uses a different DXGI_FORMAT, here we only
    // test for the 8-bit one (NV12).
    if (should_test_for_av1_support) {
      if (profile_id == DXVA_ModeAV1_VLD_Profile0) {
        supported_resolutions[AV1PROFILE_PROFILE_MAIN] = GetResolutionsForGUID(
            video_device.Get(), profile_id, kModernResolutions);
        continue;
      }
      if (profile_id == DXVA_ModeAV1_VLD_Profile1) {
        supported_resolutions[AV1PROFILE_PROFILE_HIGH] = GetResolutionsForGUID(
            video_device.Get(), profile_id, kModernResolutions);
        continue;
      }
      if (profile_id == DXVA_ModeAV1_VLD_Profile2) {
        // TODO(dalecurtis): 12-bit profile 2 support is complicated. Ideally,
        // we should test DXVA_ModeAV1_VLD_12bit_Profile2 and
        // DXVA_ModeAV1_VLD_12bit_Profile2_420 when the bit depth of the content
        // is 12-bit. However we don't know the bit depth or pixel format until
        // too late. In these cases we'll end up initializing the decoder and
        // failing on the first decode (which will trigger software fallback).
        supported_resolutions[AV1PROFILE_PROFILE_PRO] = GetResolutionsForGUID(
            video_device.Get(), profile_id, kModernResolutions);
        continue;
      }
    }

    if (!workarounds.disable_accelerated_vp8_decode &&
        profile_id == D3D11_DECODER_PROFILE_VP8_VLD &&
        base::FeatureList::IsEnabled(kMediaFoundationVP8Decoding)) {
      // VP8 decoding is cheap on modern devices compared to other codecs, so
      // much so that hardware decoding performance is actually worse at low
      // resolutions than software decoding. See https://crbug.com/1136495.
      constexpr gfx::Size kMinVp8Resolution = gfx::Size(640, 480);

      supported_resolutions[VP8PROFILE_ANY] = GetResolutionsForGUID(
          video_device.Get(), profile_id,
          {gfx::Size(4096, 2160), gfx::Size(4096, 2304), gfx::Size(4096, 4096)},
          DXGI_FORMAT_NV12, kMinVp8Resolution);
      continue;
    }

    if (workarounds.disable_accelerated_vp9_decode)
      continue;

    if (profile_id == D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0) {
      supported_resolutions[VP9PROFILE_PROFILE0] = GetResolutionsForGUID(
          video_device.Get(), profile_id, kModernResolutions);
      continue;
    }

    // RS3 has issues with VP9.2 decoding. See https://crbug.com/937108.
    if (profile_id == D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2 &&
        base::win::GetVersion() != base::win::Version::WIN10_RS3) {
      supported_resolutions[VP9PROFILE_PROFILE2] = GetResolutionsForGUID(
          video_device.Get(), profile_id, kModernResolutions, DXGI_FORMAT_P010);
      continue;
    }
  }

  return supported_resolutions;
}

}  // namespace media
