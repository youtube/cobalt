// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_helpers.h"

#include <sstream>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/windows/d3d12_helpers.h"
#include "media/gpu/windows/format_utils.h"

namespace media {

namespace {

std::string_view D3D12VideoEncoderRateControlModeToString(
    D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE mode) {
  switch (mode) {
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_ABSOLUTE_QP_MAP:
      return "AbsoluteQP";
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP:
      return "CQP";
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR:
      return "CBR";
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR:
      return "VBR";
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR:
      return "QVBR";
  }
}

std::string D3D12VideoEncoderRateControlToString(
    const D3D12_VIDEO_ENCODER_RATE_CONTROL& rate_control) {
  switch (rate_control.Mode) {
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP:
      CHECK_EQ(rate_control.Flags, D3D12_VIDEO_ENCODER_RATE_CONTROL_FLAG_NONE);
      return base::StringPrintf(
          "CQP (I: %u, P: %u, B: %u)",
          rate_control.ConfigParams.pConfiguration_CQP
              ->ConstantQP_FullIntracodedFrame,
          rate_control.ConfigParams.pConfiguration_CQP
              ->ConstantQP_InterPredictedFrame_PrevRefOnly,
          rate_control.ConfigParams.pConfiguration_CQP
              ->ConstantQP_InterPredictedFrame_BiDirectionalRef);
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR:
      CHECK_EQ(rate_control.Flags, D3D12_VIDEO_ENCODER_RATE_CONTROL_FLAG_NONE);
      return base::StringPrintf(
          "CBR (%llu bps)",
          rate_control.ConfigParams.pConfiguration_CBR->TargetBitRate);
    case D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR:
      CHECK_EQ(rate_control.Flags, D3D12_VIDEO_ENCODER_RATE_CONTROL_FLAG_NONE);
      return base::StringPrintf(
          "VBR (target: %llu bps, peak: %llu bps)",
          rate_control.ConfigParams.pConfiguration_VBR->TargetAvgBitRate,
          rate_control.ConfigParams.pConfiguration_VBR->PeakBitRate);
    default:
      NOTREACHED();
  }
}

}  // namespace

EncoderStatus CheckD3D12VideoEncoderCodec(
    ID3D12VideoDevice* video_device,
    D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC* codec) {
  HRESULT hr = video_device->CheckFeatureSupport(
      D3D12_FEATURE_VIDEO_ENCODER_CODEC, codec, sizeof(*codec));
  RETURN_ON_HR_FAILURE(hr,
                       "CheckFeatureSupport for "
                       "D3D12_FEATURE_VIDEO_ENCODER_CODEC failed",
                       EncoderStatus::Codes::kSystemAPICallError);
  if (!codec->IsSupported) {
    return {EncoderStatus::Codes::kEncoderUnsupportedCodec,
            "D3D12VideoEncoder does not support codec"};
  }
  return EncoderStatus::Codes::kOk;
}

EncoderStatus CheckD3D12VideoEncoderCodecConfigurationSupport(
    ID3D12VideoDevice* video_device,
    D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT* support) {
  HRESULT hr = video_device->CheckFeatureSupport(
      D3D12_FEATURE_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT, support,
      sizeof(*support));
  RETURN_ON_HR_FAILURE(
      hr,
      "CheckFeatureSupport for "
      "D3D12_FEATURE_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT failed",
      EncoderStatus::Codes::kSystemAPICallError);
  if (!support->IsSupported) {
    return {EncoderStatus::Codes::kEncoderUnsupportedConfig,
            "D3D12VideoEncoder does not support codec configuration"};
  }
  return EncoderStatus::Codes::kOk;
}

EncoderStatus CheckD3D12VideoEncoderInputFormat(
    ID3D12VideoDevice* video_device,
    D3D12_FEATURE_DATA_VIDEO_ENCODER_INPUT_FORMAT* input_format) {
  HRESULT hr = video_device->CheckFeatureSupport(
      D3D12_FEATURE_VIDEO_ENCODER_INPUT_FORMAT, input_format,
      sizeof(*input_format));
  RETURN_ON_HR_FAILURE(hr,
                       "CheckFeatureSupport for "
                       "D3D12_FEATURE_VIDEO_ENCODER_INPUT_FORMAT failed",
                       EncoderStatus::Codes::kSystemAPICallError);
  if (!input_format->IsSupported) {
    return {EncoderStatus::Codes::kUnsupportedFrameFormat,
            base::StrCat({"D3D12VideoEncoder does not support input format ",
                          DxgiFormatToString(input_format->Format)})};
  }
  return EncoderStatus::Codes::kOk;
}

EncoderStatus CheckD3D12VideoEncoderProfileLevel(
    ID3D12VideoDevice* video_device,
    D3D12_FEATURE_DATA_VIDEO_ENCODER_PROFILE_LEVEL* profile_level) {
  HRESULT hr = video_device->CheckFeatureSupport(
      D3D12_FEATURE_VIDEO_ENCODER_PROFILE_LEVEL, profile_level,
      sizeof(*profile_level));
  RETURN_ON_HR_FAILURE(hr,
                       "CheckFeatureSupport for "
                       "D3D12_FEATURE_VIDEO_ENCODER_PROFILE_LEVEL failed",
                       EncoderStatus::Codes::kSystemAPICallError);
  if (!profile_level->IsSupported) {
    return {EncoderStatus::Codes::kEncoderUnsupportedProfile,
            "D3D12VideoEncoder does not support profile"};
  }
  return EncoderStatus::Codes::kOk;
}

EncoderStatus CheckD3D12VideoEncoderResourceRequirements(
    ID3D12VideoDevice* video_device,
    D3D12_FEATURE_DATA_VIDEO_ENCODER_RESOURCE_REQUIREMENTS*
        resource_requirements) {
  HRESULT hr = video_device->CheckFeatureSupport(
      D3D12_FEATURE_VIDEO_ENCODER_RESOURCE_REQUIREMENTS, resource_requirements,
      sizeof(*resource_requirements));
  RETURN_ON_HR_FAILURE(hr,
                       "CheckFeatureSupport for "
                       "D3D12_FEATURE_VIDEO_ENCODER_RESOURCE_REQUIREMENTS "
                       "failed",
                       EncoderStatus::Codes::kSystemAPICallError);
  if (!resource_requirements->IsSupported) {
    return {EncoderStatus::Codes::kEncoderUnsupportedConfig,
            "D3D12VideoEncoder does not support resource requirements"};
  }
  return EncoderStatus::Codes::kOk;
}

EncoderStatus CheckD3D12VideoEncoderSupport(
    ID3D12VideoDevice* video_device,
    D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT* support) {
  HRESULT hr = video_device->CheckFeatureSupport(
      D3D12_FEATURE_VIDEO_ENCODER_SUPPORT, support, sizeof(*support));
  RETURN_ON_HR_FAILURE(hr,
                       "CheckFeatureSupport for "
                       "D3D12_FEATURE_VIDEO_ENCODER_SUPPORT failed",
                       EncoderStatus::Codes::kSystemAPICallError);
  if (!(support->SupportFlags &
        D3D12_VIDEO_ENCODER_SUPPORT_FLAG_GENERAL_SUPPORT_OK)) {
    std::stringstream error;
    CHECK_EQ(0, support->ValidationFlags &
                    D3D12_VIDEO_ENCODER_VALIDATION_FLAG_CODEC_NOT_SUPPORTED);
    if (support->ValidationFlags &
        D3D12_VIDEO_ENCODER_VALIDATION_FLAG_INPUT_FORMAT_NOT_SUPPORTED) {
      error << "D3D12VideoEncoder does not support input format "
            << DxgiFormatToString(support->InputFormat) << ". ";
    }
    if (support->ValidationFlags &
        D3D12_VIDEO_ENCODER_VALIDATION_FLAG_CODEC_CONFIGURATION_NOT_SUPPORTED) {
      error << "D3D12VideoEncoder does not support codec configuration. ";
    }
    if (support->ValidationFlags &
        D3D12_VIDEO_ENCODER_VALIDATION_FLAG_RATE_CONTROL_MODE_NOT_SUPPORTED) {
      error << "D3D12VideoEncoder does not support rate control mode "
            << D3D12VideoEncoderRateControlModeToString(
                   support->RateControl.Mode)
            << ". ";
    }
    if (support->ValidationFlags &
        D3D12_VIDEO_ENCODER_VALIDATION_FLAG_RATE_CONTROL_CONFIGURATION_NOT_SUPPORTED) {
      error << "D3D12VideoEncoder does not support rate control configuration "
            << D3D12VideoEncoderRateControlToString(support->RateControl)
            << ". ";
    }
    if (support->ValidationFlags &
        D3D12_VIDEO_ENCODER_VALIDATION_FLAG_INTRA_REFRESH_MODE_NOT_SUPPORTED) {
      error << "D3D12VideoEncoder does not support intra refresh mode. ";
    }
    if (support->ValidationFlags &
        D3D12_VIDEO_ENCODER_VALIDATION_FLAG_SUBREGION_LAYOUT_MODE_NOT_SUPPORTED) {
      error << "D3D12VideoEncoder does not support subregion layout mode. ";
    }
    if (support->ValidationFlags &
        D3D12_VIDEO_ENCODER_VALIDATION_FLAG_RESOLUTION_NOT_SUPPORTED_IN_LIST) {
      std::stringstream resolutions;
      for (size_t i = 0; i < support->ResolutionsListCount; i++) {
        if (i > 0) {
          resolutions << ", ";
        }
        // SAFETY: Caller-side should have guaranteed that
        // |support->pResolutionList| has at least
        // |support->ResolutionsListCount| elements.
        resolutions << UNSAFE_BUFFERS(support->pResolutionList[i]).Width << "x"
                    << UNSAFE_BUFFERS(support->pResolutionList[i]).Height;
      }
      error << "D3D12VideoEncoder does not support resolutions: "
            << resolutions.str() << ". ";
    }
    if (support->ValidationFlags &
        D3D12_VIDEO_ENCODER_VALIDATION_FLAG_GOP_STRUCTURE_NOT_SUPPORTED) {
      error << "D3D12VideoEncoder does not support GOP structure. ";
    }
    return {EncoderStatus::Codes::kEncoderUnsupportedConfig, error.str()};
  }
  return EncoderStatus::Codes::kOk;
}

std::unique_ptr<D3D12VideoEncoderWrapper> CreateD3D12VideoEncoderWrapper(
    ID3D12VideoDevice* video_device,
    D3D12_VIDEO_ENCODER_CODEC codec,
    const D3D12_VIDEO_ENCODER_PROFILE_DESC& profile,
    const D3D12_VIDEO_ENCODER_LEVEL_SETTING& level,
    DXGI_FORMAT input_format,
    const D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION& codec_config,
    const D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC& resolution) {
  Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device3;
  HRESULT hr = video_device->QueryInterface(IID_PPV_ARGS(&video_device3));
  RETURN_ON_HR_FAILURE(hr, "QueryInterface for ID3D12VideoDevice3 failed",
                       nullptr);

  D3D12_VIDEO_ENCODER_HEAP_DESC video_encoder_heap_desc{
      .EncodeCodec = codec,
      .EncodeProfile = profile,
      .EncodeLevel = level,
      .ResolutionsListCount = 1,
      .pResolutionList = &resolution,
  };
  Microsoft::WRL::ComPtr<ID3D12VideoEncoderHeap> video_encoder_heap;
  hr = video_device3->CreateVideoEncoderHeap(&video_encoder_heap_desc,
                                             IID_PPV_ARGS(&video_encoder_heap));
  RETURN_ON_HR_FAILURE(hr, "CreateVideoEncoderHeap failed", nullptr);

  D3D12_VIDEO_ENCODER_DESC video_encoder_desc{
      .EncodeCodec = codec,
      .EncodeProfile = profile,
      .InputFormat = input_format,
      .CodecConfiguration = codec_config,
  };
  Microsoft::WRL::ComPtr<ID3D12VideoEncoder> video_encoder;
  hr = video_device3->CreateVideoEncoder(&video_encoder_desc,
                                         IID_PPV_ARGS(&video_encoder));
  RETURN_ON_HR_FAILURE(hr, "CreateVideoEncoder failed", nullptr);

  return std::make_unique<D3D12VideoEncoderWrapper>(
      std::move(video_encoder), std::move(video_encoder_heap));
}

}  // namespace media
