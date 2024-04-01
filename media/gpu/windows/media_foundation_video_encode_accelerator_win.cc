// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/media_foundation_video_encode_accelerator_win.h"

#pragma warning(push)
#pragma warning(disable : 4800)  // Disable warning for added padding.

#include <codecapi.h>
#include <d3d11_1.h>
#include <mferror.h>
#include <mftransform.h>
#include <objbase.h>

#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/cxx17_backports.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "gpu/ipc/common/dxgi_helpers.h"
#include "media/base/media_switches.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/gpu_memory_buffer.h"

using media::MediaBufferScopedPointer;

namespace media {

namespace {

const int32_t kDefaultTargetBitrate = 5000000;
const size_t kMaxFrameRateNumerator = 30;
const size_t kMaxFrameRateDenominator = 1;
const size_t kMaxResolutionWidth = 1920;
const size_t kMaxResolutionHeight = 1088;
const size_t kNumInputBuffers = 3;
// Media Foundation uses 100 nanosecond units for time, see
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms697282(v=vs.85).aspx.
const size_t kOneMicrosecondInMFSampleTimeUnits = 10;
const size_t kOutputSampleBufferSizeRatio = 4;

constexpr const wchar_t* const kMediaFoundationVideoEncoderDLLs[] = {
    L"mf.dll",
    L"mfplat.dll",
};

eAVEncH264VProfile GetH264VProfile(VideoCodecProfile profile,
                                   bool is_constrained_h264) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return is_constrained_h264 ? eAVEncH264VProfile_ConstrainedBase
                                 : eAVEncH264VProfile_Base;
    case H264PROFILE_MAIN:
      return eAVEncH264VProfile_Main;
    case H264PROFILE_HIGH: {
      // eAVEncH264VProfile_High requires Windows 8.
      if (base::win::GetVersion() < base::win::Version::WIN8) {
        return eAVEncH264VProfile_unknown;
      }
      return eAVEncH264VProfile_High;
    }
    default:
      return eAVEncH264VProfile_unknown;
  }
}

bool IsSvcSupported(IMFActivate* activate) {
#if defined(ARCH_CPU_X86)
  // x86 systems sometimes crash in video drivers here.
  // More info: https://crbug.com/1253748
  return false;
#else
  Microsoft::WRL::ComPtr<IMFTransform> encoder;
  Microsoft::WRL::ComPtr<ICodecAPI> codec_api;
  HRESULT hr = activate->ActivateObject(IID_PPV_ARGS(&encoder));
  if (FAILED(hr))
    return false;

  bool result = false;
  hr = encoder.As(&codec_api);
  if (SUCCEEDED(hr)) {
    result = (codec_api->IsSupported(&CODECAPI_AVEncVideoTemporalLayerCount) ==
              S_OK);
    if (result) {
      VARIANT min, max, step;
      VariantInit(&min);
      VariantInit(&max);
      VariantInit(&step);

      hr = codec_api->GetParameterRange(&CODECAPI_AVEncVideoTemporalLayerCount,
                                        &min, &max, &step);
      if (hr != S_OK || min.ulVal > 1 || max.ulVal < 3)
        result = false;

      VariantClear(&min);
      VariantClear(&max);
      VariantClear(&step);
    }
  }

  activate->ShutdownObject();
  return result;
#endif  // defined(ARCH_CPU_X86)
}

}  // namespace

class MediaFoundationVideoEncodeAccelerator::EncodeOutput {
 public:
  EncodeOutput(uint32_t size,
               bool key_frame,
               base::TimeDelta timestamp,
               int temporal_id = 0)
      : keyframe(key_frame),
        capture_timestamp(timestamp),
        temporal_layer_id(temporal_id),
        data_(size) {}

  uint8_t* memory() { return data_.data(); }

  int size() const { return static_cast<int>(data_.size()); }

  const bool keyframe;
  const base::TimeDelta capture_timestamp;
  const int temporal_layer_id;

 private:
  std::vector<uint8_t> data_;

  DISALLOW_COPY_AND_ASSIGN(EncodeOutput);
};

struct MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(int32_t id,
                     base::WritableSharedMemoryMapping mapping,
                     size_t size)
      : id(id), mapping(std::move(mapping)), size(size) {}
  const int32_t id;
  const base::WritableSharedMemoryMapping mapping;
  const size_t size;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BitstreamBufferRef);
};

// TODO(zijiehe): Respect |compatible_with_win7_| in the implementation. Some
// attributes are not supported by Windows 7, setting them will return errors.
// See bug: http://crbug.com/777659.
MediaFoundationVideoEncodeAccelerator::MediaFoundationVideoEncodeAccelerator(
    bool compatible_with_win7,
    bool enable_async_mft)
    : compatible_with_win7_(compatible_with_win7),
      enable_async_mft_(enable_async_mft),
      is_async_mft_(false),
      input_required_(false),
      main_client_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      encoder_thread_("MFEncoderThread") {}

MediaFoundationVideoEncodeAccelerator::
    ~MediaFoundationVideoEncodeAccelerator() {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  DCHECK(!encoder_thread_.IsRunning());
  DCHECK(!encoder_task_weak_factory_.HasWeakPtrs());
}

VideoEncodeAccelerator::SupportedProfiles
MediaFoundationVideoEncodeAccelerator::GetSupportedProfiles() {
  TRACE_EVENT0("gpu,startup",
               "MediaFoundationVideoEncodeAccelerator::GetSupportedProfiles");
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  SupportedProfiles profiles;
  bitrate_ = Bitrate::ConstantBitrate(kDefaultTargetBitrate);
  frame_rate_ = kMaxFrameRateNumerator / kMaxFrameRateDenominator;
  IMFActivate** pp_activate = nullptr;
  uint32_t encoder_count = EnumerateHardwareEncoders(&pp_activate);
  if (!encoder_count) {
    ReleaseEncoderResources();
    DVLOG(1)
        << "Hardware encode acceleration is not available on this platform.";

    return profiles;
  }

  bool svc_supported = false;
  if (pp_activate) {
    for (UINT32 i = 0; i < encoder_count; i++) {
      if (pp_activate[i]) {
        if (IsSvcSupported(pp_activate[i]))
          svc_supported = true;

        // Release the enumerated instances if any.
        // According to Windows Dev Center,
        // https://docs.microsoft.com/en-us/windows/win32/api/mfapi/nf-mfapi-mftenumex
        // The caller must release the pointers.
        pp_activate[i]->Release();
        pp_activate[i] = nullptr;
      }
    }
    CoTaskMemFree(pp_activate);
  }
  ReleaseEncoderResources();

  SupportedProfile profile;
  // More profiles can be supported here, but they should be available in SW
  // fallback as well.
  profile.profile = H264PROFILE_BASELINE;
  profile.max_framerate_numerator = kMaxFrameRateNumerator;
  profile.max_framerate_denominator = kMaxFrameRateDenominator;
  profile.max_resolution = gfx::Size(kMaxResolutionWidth, kMaxResolutionHeight);
  if (svc_supported) {
    profile.scalability_modes.push_back(SVCScalabilityMode::kL1T2);
    profile.scalability_modes.push_back(SVCScalabilityMode::kL1T3);
  }
  profiles.push_back(profile);

  profile.profile = H264PROFILE_MAIN;
  profiles.push_back(profile);

  profile.profile = H264PROFILE_HIGH;
  profiles.push_back(profile);

  return profiles;
}

bool MediaFoundationVideoEncodeAccelerator::Initialize(const Config& config,
                                                       Client* client) {
  DVLOG(3) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  if (PIXEL_FORMAT_I420 != config.input_format &&
      PIXEL_FORMAT_NV12 != config.input_format) {
    DLOG(ERROR) << "Input format not supported= "
                << VideoPixelFormatToString(config.input_format);
    return false;
  }

  if (GetH264VProfile(config.output_profile, config.is_constrained_h264) ==
      eAVEncH264VProfile_unknown) {
    DLOG(ERROR) << "Output profile not supported= " << config.output_profile;
    return false;
  }

  encoder_thread_.init_com_with_mta(false);
  if (!encoder_thread_.Start()) {
    DLOG(ERROR) << "Failed spawning encoder thread.";
    return false;
  }
  encoder_thread_task_runner_ = encoder_thread_.task_runner();
  IMFActivate** pp_activate = nullptr;
  uint32_t encoder_count = EnumerateHardwareEncoders(&pp_activate);
  if (!encoder_count) {
    DLOG(ERROR) << "Failed finding a hardware encoder MFT.";
    return false;
  }

  if (is_async_mft_) {
    if (!ActivateAsyncEncoder(pp_activate, encoder_count)) {
      DLOG(ERROR) << "Failed activating an async hardware encoder MFT.";

      if (pp_activate) {
        // Release the enumerated instances if any.
        // According to Windows Dev Center,
        // https://docs.microsoft.com/en-us/windows/win32/api/mfapi/nf-mfapi-mftenumex
        // The caller must release the pointers.
        for (UINT32 i = 0; i < encoder_count; i++) {
          if (pp_activate[i]) {
            pp_activate[i]->Release();
            pp_activate[i] = nullptr;
          }
        }
        CoTaskMemFree(pp_activate);
      }
      return false;
    }

    if (pp_activate) {
      // Release the enumerated instances if any.
      // According to Windows Dev Center,
      // https://docs.microsoft.com/en-us/windows/win32/api/mfapi/nf-mfapi-mftenumex
      // The caller must release the pointers.
      for (UINT32 i = 0; i < encoder_count; i++) {
        if (pp_activate[i]) {
          pp_activate[i]->Release();
          pp_activate[i] = nullptr;
        }
      }
      CoTaskMemFree(pp_activate);
    }
  }

  main_client_weak_factory_ =
      std::make_unique<base::WeakPtrFactory<Client>>(client);
  main_client_ = main_client_weak_factory_->GetWeakPtr();
  input_visible_size_ = config.input_visible_size;
  if (config.initial_framerate.has_value())
    frame_rate_ = config.initial_framerate.value();
  else
    frame_rate_ = kMaxFrameRateNumerator / kMaxFrameRateDenominator;
  bitrate_ = config.bitrate;
  bitstream_buffer_size_ = config.input_visible_size.GetArea();
  gop_length_ = config.gop_length;
  low_latency_mode_ = config.require_low_delay;

  if (config.HasTemporalLayer())
    num_temporal_layers_ = config.spatial_layers.front().num_of_temporal_layers;

  if (!SetEncoderModes()) {
    DLOG(ERROR) << "Failed setting encoder parameters.";
    return false;
  }

  if (!InitializeInputOutputParameters(config.output_profile,
                                       config.is_constrained_h264)) {
    DLOG(ERROR) << "Failed initializing input-output samples.";
    return false;
  }

  HRESULT hr = MFCreateSample(&input_sample_);
  RETURN_ON_HR_FAILURE(hr, "Failed to create sample", false);

  if (config.input_format == PIXEL_FORMAT_NV12 &&
      base::FeatureList::IsEnabled(media::kMediaFoundationD3D11VideoCapture)) {
    dxgi_device_manager_ = DXGIDeviceManager::Create();
    if (!dxgi_device_manager_) {
      DLOG(ERROR) << "Failed to create DXGIDeviceManager";
      return false;
    }
  }

  if (is_async_mft_) {
    // Start the asynchronous processing model
    if (dxgi_device_manager_) {
      auto mf_dxgi_device_manager =
          dxgi_device_manager_->GetMFDXGIDeviceManager();
      hr = encoder_->ProcessMessage(
          MFT_MESSAGE_SET_D3D_MANAGER,
          reinterpret_cast<ULONG_PTR>(mf_dxgi_device_manager.Get()));
      RETURN_ON_HR_FAILURE(
          hr, "Couldn't set ProcessMessage MFT_MESSAGE_SET_D3D_MANAGER", false);
    }
    hr = encoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    RETURN_ON_HR_FAILURE(
        hr, "Couldn't set ProcessMessage MFT_MESSAGE_COMMAND_FLUSH", false);
    hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    RETURN_ON_HR_FAILURE(
        hr, "Couldn't set ProcessMessage MFT_MESSAGE_NOTIFY_BEGIN_STREAMING",
        false);
    hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    RETURN_ON_HR_FAILURE(
        hr, "Couldn't set ProcessMessage MFT_MESSAGE_NOTIFY_START_OF_STREAM",
        false);
    hr = encoder_->QueryInterface(IID_PPV_ARGS(&event_generator_));
    RETURN_ON_HR_FAILURE(hr, "Couldn't get event generator", false);
  } else {
    // Create output sample for synchronous processing model
    MFT_OUTPUT_STREAM_INFO output_stream_info;
    hr = encoder_->GetOutputStreamInfo(output_stream_id_, &output_stream_info);
    RETURN_ON_HR_FAILURE(hr, "Couldn't get output stream info", false);
    output_sample_ = CreateEmptySampleWithBuffer(
        output_stream_info.cbSize
            ? output_stream_info.cbSize
            : bitstream_buffer_size_ * kOutputSampleBufferSizeRatio,
        output_stream_info.cbAlignment);

    hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    RETURN_ON_HR_FAILURE(
        hr, "Couldn't set ProcessMessage MFT_MESSAGE_NOTIFY_BEGIN_STREAMING",
        false);
  }

  main_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::RequireBitstreamBuffers, main_client_,
                                kNumInputBuffers, input_visible_size_,
                                bitstream_buffer_size_));
  return SUCCEEDED(hr);
}

void MediaFoundationVideoEncodeAccelerator::Encode(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaFoundationVideoEncodeAccelerator::EncodeTask,
                     encoder_task_weak_factory_.GetWeakPtr(), std::move(frame),
                     force_keyframe));
}

void MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOG(3) << __func__ << ": buffer size=" << buffer.size();
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  if (buffer.size() < bitstream_buffer_size_) {
    DLOG(ERROR) << "Output BitstreamBuffer isn't big enough: " << buffer.size()
                << " vs. " << bitstream_buffer_size_;
    NotifyError(kInvalidArgumentError);
    return;
  }

  auto region =
      base::UnsafeSharedMemoryRegion::Deserialize(buffer.TakeRegion());
  auto mapping = region.Map();
  if (!region.IsValid() || !mapping.IsValid()) {
    DLOG(ERROR) << "Failed mapping shared memory.";
    NotifyError(kPlatformFailureError);
    return;
  }
  // After mapping, |region| is no longer necessary and it can be
  // destroyed. |mapping| will keep the shared memory region open.

  std::unique_ptr<BitstreamBufferRef> buffer_ref(
      new BitstreamBufferRef(buffer.id(), std::move(mapping), buffer.size()));
  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBufferTask,
          encoder_task_weak_factory_.GetWeakPtr(), std::move(buffer_ref)));
}

void MediaFoundationVideoEncodeAccelerator::RequestEncodingParametersChange(
    const media::Bitrate& bitrate,
    uint32_t framerate) {
  DVLOG(3) << __func__ << ": bitrate=" << bitrate.ToString()
           << ": framerate=" << framerate;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaFoundationVideoEncodeAccelerator::
                                    RequestEncodingParametersChangeTask,
                                encoder_task_weak_factory_.GetWeakPtr(),
                                bitrate, framerate));
}

void MediaFoundationVideoEncodeAccelerator::Destroy() {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  // Cancel all callbacks.
  main_client_weak_factory_.reset();

  if (encoder_thread_.IsRunning()) {
    encoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaFoundationVideoEncodeAccelerator::DestroyTask,
                       encoder_task_weak_factory_.GetWeakPtr()));
    encoder_thread_.Stop();
  }

  delete this;
}

bool MediaFoundationVideoEncodeAccelerator::IsGpuFrameResizeSupported() {
  return true;
}

// static
bool MediaFoundationVideoEncodeAccelerator::PreSandboxInitialization() {
  bool result = true;
  for (const wchar_t* mfdll : kMediaFoundationVideoEncoderDLLs) {
    if (::LoadLibrary(mfdll) == nullptr) {
      result = false;
    }
  }
  return result;
}

uint32_t MediaFoundationVideoEncodeAccelerator::EnumerateHardwareEncoders(
    IMFActivate*** pp_activate) {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  if (!compatible_with_win7_ &&
      base::win::GetVersion() < base::win::Version::WIN8) {
    DVLOG(ERROR) << "Windows versions earlier than 8 are not supported.";
    return 0;
  }

  for (const wchar_t* mfdll : kMediaFoundationVideoEncoderDLLs) {
    if (!::GetModuleHandle(mfdll)) {
      DVLOG(ERROR) << mfdll << " is required for encoding";
      return 0;
    }
  }

  if (!InitializeMediaFoundation())
    return 0;

  uint32_t flags = MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER;
  MFT_REGISTER_TYPE_INFO input_info;
  input_info.guidMajorType = MFMediaType_Video;
  input_info.guidSubtype = MFVideoFormat_NV12;
  MFT_REGISTER_TYPE_INFO output_info;
  output_info.guidMajorType = MFMediaType_Video;
  output_info.guidSubtype = MFVideoFormat_H264;

  uint32_t count = 0;
  HRESULT hr = E_FAIL;
  if (enable_async_mft_) {
    // Use MFTEnumEx to find hardware encoder.
    hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, flags, &input_info, &output_info,
                   pp_activate, &count);
    RETURN_ON_HR_FAILURE(
        hr, "Couldn't enumerate hardware encoder from MFTEnumEx", 0);
    RETURN_ON_FAILURE((count > 0), "No asynchronous MFT encoder found", 0);
    DVLOG(3) << "Hardware encoder(s) available found from MFTEnumEx: " << count;
    is_async_mft_ = true;
  } else {
    // Use MFTEnum to find hardware encoder.
    base::win::ScopedCoMem<CLSID> CLSIDs;
    hr = MFTEnum(MFT_CATEGORY_VIDEO_ENCODER, flags, &input_info, &output_info,
                 nullptr, &CLSIDs, &count);
    RETURN_ON_HR_FAILURE(hr, "Couldn't enumerate hardware encoder from MFTEnum",
                         0);
    RETURN_ON_FAILURE((count > 0), "No legacy MFT encoder found", 0);
    DVLOG(3) << "Hardware encoder(s) available found from MFTEnum: " << count;
    hr = ::CoCreateInstance(CLSIDs[0], nullptr, CLSCTX_ALL,
                            IID_PPV_ARGS(&encoder_));
    RETURN_ON_HR_FAILURE(hr, "Couldn't create legacy MFT encoder", 0);
    RETURN_ON_FAILURE((encoder_.Get() != nullptr),
                      "No legacy MFT encoder instance created", 0);
    is_async_mft_ = false;
  }

  return count;
}

bool MediaFoundationVideoEncodeAccelerator::ActivateAsyncEncoder(
    IMFActivate** pp_activate,
    uint32_t encoder_count) {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  // Try to create the encoder with priority according to merit value.
  HRESULT hr = E_FAIL;
  for (UINT32 i = 0; i < encoder_count; i++) {
    if (FAILED(hr)) {
      DCHECK(!encoder_);
      DCHECK(!activate_);
      hr = pp_activate[i]->ActivateObject(IID_PPV_ARGS(&encoder_));
      if (encoder_.Get() != nullptr) {
        DCHECK(SUCCEEDED(hr));

        activate_ = pp_activate[i];
        pp_activate[i] = nullptr;

        // Print the friendly name.
        base::win::ScopedCoMem<WCHAR> friendly_name;
        UINT32 name_length;
        activate_->GetAllocatedString(MFT_FRIENDLY_NAME_Attribute,
                                      &friendly_name, &name_length);
        DVLOG(3) << "Selected asynchronous hardware encoder's friendly name: "
                 << friendly_name;
      } else {
        DCHECK(FAILED(hr));

        // The component that calls ActivateObject is
        // responsible for calling ShutdownObject,
        // https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfactivate-shutdownobject.
        pp_activate[i]->ShutdownObject();
      }
    }
  }

  RETURN_ON_HR_FAILURE(hr, "Couldn't activate asynchronous hardware encoder",
                       false);
  RETURN_ON_FAILURE((encoder_.Get() != nullptr),
                    "No asynchronous hardware encoder instance created", false);

  Microsoft::WRL::ComPtr<IMFAttributes> all_attributes;
  hr = encoder_->GetAttributes(&all_attributes);
  if (SUCCEEDED(hr)) {
    // An asynchronous MFT must support dynamic format changes,
    // https://docs.microsoft.com/en-us/windows/win32/medfound/asynchronous-mfts#format-changes.
    UINT32 dynamic = FALSE;
    hr = all_attributes->GetUINT32(MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE, &dynamic);
    if (!dynamic) {
      DLOG(ERROR) << "Couldn't support dynamic format change.";
      return false;
    }

    // Unlock the selected asynchronous MFTs,
    // https://docs.microsoft.com/en-us/windows/win32/medfound/asynchronous-mfts#unlocking-asynchronous-mfts.
    UINT32 async = FALSE;
    hr = all_attributes->GetUINT32(MF_TRANSFORM_ASYNC, &async);
    if (!async) {
      DLOG(ERROR) << "MFT encoder is not asynchronous.";
      return false;
    }

    hr = all_attributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
    RETURN_ON_HR_FAILURE(hr, "Couldn't unlock transform async", false);
  }

  return true;
}

bool MediaFoundationVideoEncodeAccelerator::InitializeInputOutputParameters(
    VideoCodecProfile output_profile,
    bool is_constrained_h264) {
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());
  DCHECK(encoder_);

  DWORD input_count = 0;
  DWORD output_count = 0;
  HRESULT hr = encoder_->GetStreamCount(&input_count, &output_count);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get stream count", false);
  if (input_count < 1 || output_count < 1) {
    DLOG(ERROR) << "Stream count too few: input " << input_count << ", output "
                << output_count;
    return false;
  }

  std::vector<DWORD> input_ids(input_count, 0);
  std::vector<DWORD> output_ids(output_count, 0);
  hr = encoder_->GetStreamIDs(input_count, input_ids.data(), output_count,
                              output_ids.data());
  if (hr == S_OK) {
    input_stream_id_ = input_ids[0];
    output_stream_id_ = output_ids[0];
  } else if (hr == E_NOTIMPL) {
    input_stream_id_ = 0;
    output_stream_id_ = 0;
  } else {
    DLOG(ERROR) << "Couldn't find stream ids from hardware encoder.";
    return false;
  }

  // Initialize output parameters.
  hr = MFCreateMediaType(&imf_output_media_type_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't create output media type", false);
  hr = imf_output_media_type_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set media type", false);
  hr = imf_output_media_type_->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set video format", false);
  hr = imf_output_media_type_->SetUINT32(MF_MT_AVG_BITRATE, bitrate_.target());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
  hr = MFSetAttributeRatio(imf_output_media_type_.Get(), MF_MT_FRAME_RATE,
                           frame_rate_, 1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate", false);
  hr = MFSetAttributeSize(imf_output_media_type_.Get(), MF_MT_FRAME_SIZE,
                          input_visible_size_.width(),
                          input_visible_size_.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = imf_output_media_type_->SetUINT32(MF_MT_INTERLACE_MODE,
                                         MFVideoInterlace_Progressive);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set interlace mode", false);
  hr = imf_output_media_type_->SetUINT32(
      MF_MT_MPEG2_PROFILE,
      GetH264VProfile(output_profile, is_constrained_h264));
  RETURN_ON_HR_FAILURE(hr, "Couldn't set codec profile", false);
  hr = encoder_->SetOutputType(output_stream_id_, imf_output_media_type_.Get(),
                               0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set output media type", false);

  // Initialize input parameters.
  hr = MFCreateMediaType(&imf_input_media_type_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't create input media type", false);
  hr = imf_input_media_type_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set media type", false);
  hr = imf_input_media_type_->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set video format", false);
  hr = MFSetAttributeRatio(imf_input_media_type_.Get(), MF_MT_FRAME_RATE,
                           frame_rate_, 1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate", false);
  hr = MFSetAttributeSize(imf_input_media_type_.Get(), MF_MT_FRAME_SIZE,
                          input_visible_size_.width(),
                          input_visible_size_.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = imf_input_media_type_->SetUINT32(MF_MT_INTERLACE_MODE,
                                        MFVideoInterlace_Progressive);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set interlace mode", false);
  hr = encoder_->SetInputType(input_stream_id_, imf_input_media_type_.Get(), 0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set input media type", false);

  return true;
}

bool MediaFoundationVideoEncodeAccelerator::SetEncoderModes() {
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());
  DCHECK(encoder_);

  HRESULT hr = encoder_.As(&codec_api_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get ICodecAPI", false);

  VARIANT var;
  var.vt = VT_UI4;
  switch (bitrate_.mode()) {
    case Bitrate::Mode::kConstant:
      var.ulVal = eAVEncCommonRateControlMode_CBR;
      break;
    case Bitrate::Mode::kVariable:
      var.ulVal = eAVEncCommonRateControlMode_PeakConstrainedVBR;
      break;
  }
  hr = codec_api_->SetValue(&CODECAPI_AVEncCommonRateControlMode, &var);
  if (!compatible_with_win7_) {
    // Though CODECAPI_AVEncCommonRateControlMode is supported by Windows 7, but
    // according to a discussion on MSDN,
    // https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/6da521e9-7bb3-4b79-a2b6-b31509224638/win7-h264-encoder-imfsinkwriter-cant-use-quality-vbr-encoding?forum=mediafoundationdevelopment
    // setting it on Windows 7 returns error.
    RETURN_ON_HR_FAILURE(hr, "Couldn't set CommonRateControlMode", false);
  }

  if (is_async_mft_ && S_OK == codec_api_->IsModifiable(
                                   &CODECAPI_AVEncVideoTemporalLayerCount)) {
    var.ulVal = num_temporal_layers_;
    hr = codec_api_->SetValue(&CODECAPI_AVEncVideoTemporalLayerCount, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't set temporal layer count", false);
    }
  }

  var.ulVal = bitrate_.target();
  hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
  if (!compatible_with_win7_) {
    RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
  }

  if (bitrate_.mode() == Bitrate::Mode::kVariable) {
    var.ulVal = bitrate_.peak();
    hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMaxBitRate, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
    }
  }

  if (!is_async_mft_ ||
      (is_async_mft_ &&
       S_OK == codec_api_->IsModifiable(&CODECAPI_AVEncAdaptiveMode))) {
    var.ulVal = eAVEncAdaptiveMode_Resolution;
    hr = codec_api_->SetValue(&CODECAPI_AVEncAdaptiveMode, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't set adaptive mode", false);
    }
  }

  if (gop_length_.has_value()) {
    var.ulVal = gop_length_.value();
    hr = codec_api_->SetValue(&CODECAPI_AVEncMPVGOPSize, &var);
    RETURN_ON_HR_FAILURE(hr, "Couldn't set low keyframe interval", false);
  }

  if (!is_async_mft_ ||
      (is_async_mft_ &&
       S_OK == codec_api_->IsModifiable(&CODECAPI_AVLowLatencyMode))) {
    var.vt = VT_BOOL;
    var.boolVal = low_latency_mode_ ? VARIANT_TRUE : VARIANT_FALSE;
    hr = codec_api_->SetValue(&CODECAPI_AVLowLatencyMode, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't set low latency mode", false);
    }
  }

  return true;
}

void MediaFoundationVideoEncodeAccelerator::NotifyError(
    VideoEncodeAccelerator::Error error) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread() ||
         main_client_task_runner_->BelongsToCurrentThread());

  main_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::NotifyError, main_client_, error));
}

void MediaFoundationVideoEncodeAccelerator::EncodeTask(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (is_async_mft_) {
    AsyncEncodeTask(std::move(frame), force_keyframe);
  } else {
    SyncEncodeTask(std::move(frame), force_keyframe);
  }
}

void MediaFoundationVideoEncodeAccelerator::AsyncEncodeTask(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  bool input_delivered = false;
  HRESULT hr = E_FAIL;
  if (input_required_) {
    // Hardware MFT is waiting for this coming input.
    hr = ProcessInput(std::move(frame), force_keyframe);
    if (FAILED(hr)) {
      NotifyError(kPlatformFailureError);
      RETURN_ON_HR_FAILURE(hr, "Couldn't encode", );
    }

    DVLOG(3) << "Sent for encode " << hr;
    input_delivered = true;
    input_required_ = false;
  } else {
    Microsoft::WRL::ComPtr<IMFMediaEvent> media_event;
    hr = event_generator_->GetEvent(MF_EVENT_FLAG_NO_WAIT, &media_event);
    if (FAILED(hr)) {
      DLOG(WARNING) << "Abandoned input frame for video encoder.";
      return;
    }

    MediaEventType event_type;
    hr = media_event->GetType(&event_type);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get the type of media event.";
      return;
    }

    // Always deliver the current input into HMFT.
    if (event_type == METransformNeedInput) {
      hr = ProcessInput(std::move(frame), force_keyframe);
      if (FAILED(hr)) {
        NotifyError(kPlatformFailureError);
        RETURN_ON_HR_FAILURE(hr, "Couldn't encode", );
      }

      DVLOG(3) << "Sent for encode " << hr;
      input_delivered = true;
    } else if (event_type == METransformHaveOutput) {
      ProcessOutputAsync();
      input_delivered =
          TryToDeliverInputFrame(std::move(frame), force_keyframe);
    }
  }

  if (!input_delivered) {
    DLOG(ERROR) << "Failed to deliver input frame to video encoder";
    return;
  }

  TryToReturnBitstreamBuffer();
}

void MediaFoundationVideoEncodeAccelerator::SyncEncodeTask(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  HRESULT hr = E_FAIL;
  hr = ProcessInput(std::move(frame), force_keyframe);

  // According to MSDN, if encoder returns MF_E_NOTACCEPTING, we need to try
  // processing the output. This error indicates that encoder does not accept
  // any more input data.
  if (hr == MF_E_NOTACCEPTING) {
    DVLOG(3) << "MF_E_NOTACCEPTING";
    ProcessOutputSync();
    hr = encoder_->ProcessInput(input_stream_id_, input_sample_.Get(), 0);
    if (FAILED(hr)) {
      NotifyError(kPlatformFailureError);
      RETURN_ON_HR_FAILURE(hr, "Couldn't encode", );
    }
  } else if (FAILED(hr)) {
    NotifyError(kPlatformFailureError);
    RETURN_ON_HR_FAILURE(hr, "Couldn't encode", );
  }
  DVLOG(3) << "Sent for encode " << hr;

  ProcessOutputSync();
}

HRESULT MediaFoundationVideoEncodeAccelerator::ProcessInput(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  HRESULT hr = PopulateInputSampleBuffer(frame);
  RETURN_ON_HR_FAILURE(hr, "Couldn't populate input sample buffer", hr);

  input_sample_->SetSampleTime(frame->timestamp().InMicroseconds() *
                               kOneMicrosecondInMFSampleTimeUnits);
  UINT64 sample_duration = 0;
  hr = MFFrameRateToAverageTimePerFrame(frame_rate_, 1, &sample_duration);
  RETURN_ON_HR_FAILURE(hr, "Couldn't calculate sample duration", E_FAIL);
  input_sample_->SetSampleDuration(sample_duration);

  if (force_keyframe) {
    VARIANT var;
    var.vt = VT_UI4;
    var.ulVal = 1;
    hr = codec_api_->SetValue(&CODECAPI_AVEncVideoForceKeyFrame, &var);
    if (!compatible_with_win7_ && FAILED(hr)) {
      LOG(WARNING) << "Failed to set CODECAPI_AVEncVideoForceKeyFrame, "
                      "HRESULT: 0x"
                   << std::hex << hr;
    }
  }

  return encoder_->ProcessInput(input_stream_id_, input_sample_.Get(), 0);
}

HRESULT MediaFoundationVideoEncodeAccelerator::PopulateInputSampleBuffer(
    scoped_refptr<VideoFrame> frame) {
  // Handle case where video frame is backed by a GPU texture
  if (frame->storage_type() ==
      VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER) {
    DCHECK_EQ(frame->format(), PIXEL_FORMAT_NV12);

    gfx::GpuMemoryBuffer* gmb = frame->GetGpuMemoryBuffer();
    if (!gmb) {
      DLOG(ERROR) << "Failed to get GMB for input frame";
      return MF_E_INVALID_STREAM_DATA;
    }

    gfx::GpuMemoryBufferHandle buffer_handle = gmb->CloneHandle();
    DCHECK_EQ(gmb->GetType(), gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE);

    auto d3d_device = dxgi_device_manager_->GetDevice();
    if (!d3d_device) {
      DLOG(ERROR) << "Failed to get device from MF DXGI device manager";
      return E_HANDLE;
    }

    Microsoft::WRL::ComPtr<ID3D11Device1> device1;
    HRESULT hr = d3d_device.As(&device1);
    RETURN_ON_HR_FAILURE(hr, "Failed to query ID3D11Device1", hr);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture;
    hr = device1->OpenSharedResource1(buffer_handle.dxgi_handle.Get(),
                                      IID_PPV_ARGS(&input_texture));
    RETURN_ON_HR_FAILURE(hr, "Failed to open shared GMB D3D texture", hr);

    // Check if we need to scale the input texture
    D3D11_TEXTURE2D_DESC input_desc = {};
    input_texture->GetDesc(&input_desc);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> sample_texture;
    if (input_desc.Width !=
            static_cast<uint32_t>(input_visible_size_.width()) ||
        input_desc.Height !=
            static_cast<uint32_t>(input_visible_size_.height())) {
      hr = PerformD3DScaling(input_texture.Get());
      RETURN_ON_HR_FAILURE(hr, "Failed to perform D3D video processing", hr);
      sample_texture = scaled_d3d11_texture_;
    } else {
      sample_texture = input_texture;
    }

    Microsoft::WRL::ComPtr<IMFMediaBuffer> input_buffer;
    hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D),
                                   sample_texture.Get(), 0, FALSE,
                                   &input_buffer);
    RETURN_ON_HR_FAILURE(hr, "Failed to create MF DXGI surface buffer", hr);

    // Some encoder MFTs (e.g. Qualcomm) depend on the sample buffer having a
    // valid current length. Call GetMaxLength() to compute the plane size.
    DWORD buffer_length = 0;
    hr = input_buffer->GetMaxLength(&buffer_length);
    RETURN_ON_HR_FAILURE(hr, "Failed to get max buffer length", hr);
    hr = input_buffer->SetCurrentLength(buffer_length);
    RETURN_ON_HR_FAILURE(hr, "Failed to set current buffer length", hr);

    hr = input_sample_->RemoveAllBuffers();
    RETURN_ON_HR_FAILURE(hr, "Failed to remove buffers from sample", hr);
    hr = input_sample_->AddBuffer(input_buffer.Get());
    RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample", hr);
    return S_OK;
  }

  Microsoft::WRL::ComPtr<IMFMediaBuffer> input_buffer;
  HRESULT hr = input_sample_->GetBufferByIndex(0, &input_buffer);
  if (FAILED(hr)) {
    // Allocate a new buffer.
    MFT_INPUT_STREAM_INFO input_stream_info;
    hr = encoder_->GetInputStreamInfo(input_stream_id_, &input_stream_info);
    RETURN_ON_HR_FAILURE(hr, "Couldn't get input stream info", hr);

    hr = MFCreateAlignedMemoryBuffer(
        input_stream_info.cbSize ? input_stream_info.cbSize
                                 : VideoFrame::AllocationSize(
                                       PIXEL_FORMAT_NV12, input_visible_size_),
        input_stream_info.cbAlignment == 0 ? input_stream_info.cbAlignment
                                           : input_stream_info.cbAlignment - 1,
        &input_buffer);
    RETURN_ON_HR_FAILURE(hr, "Failed to create memory buffer", hr);
    hr = input_sample_->AddBuffer(input_buffer.Get());
    RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample", hr);
  }

  MediaBufferScopedPointer scoped_buffer(input_buffer.Get());
  DCHECK(scoped_buffer.get());
  uint8_t* dst_y = scoped_buffer.get();
  uint8_t* dst_uv =
      scoped_buffer.get() +
      frame->row_bytes(VideoFrame::kYPlane) * frame->rows(VideoFrame::kYPlane);
  uint8_t* end = dst_uv + frame->row_bytes(VideoFrame::kUVPlane) *
                              frame->rows(VideoFrame::kUVPlane);
  DCHECK_GE(static_cast<ptrdiff_t>(scoped_buffer.max_length()),
            end - scoped_buffer.get());

  if (frame->format() == PIXEL_FORMAT_NV12) {
    // Copy NV12 pixel data from |frame| to |input_buffer|.
    int error = libyuv::NV12Copy(frame->visible_data(VideoFrame::kYPlane),
                                 frame->stride(VideoFrame::kYPlane),
                                 frame->visible_data(VideoFrame::kUVPlane),
                                 frame->stride(VideoFrame::kUVPlane), dst_y,
                                 frame->row_bytes(VideoFrame::kYPlane), dst_uv,
                                 frame->row_bytes(VideoFrame::kUPlane),
                                 input_visible_size_.width(),
                                 input_visible_size_.height());
    if (error)
      return E_FAIL;
  } else if (frame->format() == PIXEL_FORMAT_I420) {
    // Convert I420 to NV12 as input.
    int error = libyuv::I420ToNV12(
        frame->visible_data(VideoFrame::kYPlane),
        frame->stride(VideoFrame::kYPlane),
        frame->visible_data(VideoFrame::kUPlane),
        frame->stride(VideoFrame::kUPlane),
        frame->visible_data(VideoFrame::kVPlane),
        frame->stride(VideoFrame::kVPlane), dst_y,
        frame->row_bytes(VideoFrame::kYPlane), dst_uv,
        frame->row_bytes(VideoFrame::kUPlane) * 2, input_visible_size_.width(),
        input_visible_size_.height());
    if (error)
      return E_FAIL;
  } else {
    NOTREACHED();
  }

  return S_OK;
}

int MediaFoundationVideoEncodeAccelerator::AssignTemporalId(bool keyframe) {
  int result = 0;

  if (keyframe)
    outputs_since_keyframe_count_ = 0;

  switch (num_temporal_layers_) {
    case 1:
      return 0;
    case 2: {
      const static std::array<int, 2> kTwoTemporalLayers = {0, 1};
      result = kTwoTemporalLayers[outputs_since_keyframe_count_ %
                                  kTwoTemporalLayers.size()];
      break;
    }
    case 3: {
      const static std::array<int, 4> kThreeTemporalLayers = {0, 2, 1, 2};
      result = kThreeTemporalLayers[outputs_since_keyframe_count_ %
                                    kThreeTemporalLayers.size()];
      break;
    }
  }
  outputs_since_keyframe_count_++;
  return result;
}

void MediaFoundationVideoEncodeAccelerator::ProcessOutputAsync() {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  output_data_buffer.dwStreamID = output_stream_id_;
  output_data_buffer.dwStatus = 0;
  output_data_buffer.pEvents = nullptr;
  output_data_buffer.pSample = nullptr;
  DWORD status = 0;
  HRESULT hr = encoder_->ProcessOutput(0, 1, &output_data_buffer, &status);
  if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
    hr = S_OK;
    Microsoft::WRL::ComPtr<IMFMediaType> media_type;
    for (DWORD type_index = 0; SUCCEEDED(hr); ++type_index) {
      hr = encoder_->GetOutputAvailableType(output_stream_id_, type_index,
                                            &media_type);
      if (SUCCEEDED(hr)) {
        break;
      }
    }
    hr = encoder_->SetOutputType(output_stream_id_, media_type.Get(), 0);
    return;
  }

  RETURN_ON_HR_FAILURE(hr, "Couldn't get encoded data", );
  DVLOG(3) << "Got encoded data " << hr;

  Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer;
  hr = output_data_buffer.pSample->GetBufferByIndex(0, &output_buffer);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get buffer by index", );

  DWORD size = 0;
  hr = output_buffer->GetCurrentLength(&size);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get buffer length", );

  base::TimeDelta timestamp;
  LONGLONG sample_time;
  hr = output_data_buffer.pSample->GetSampleTime(&sample_time);
  if (SUCCEEDED(hr)) {
    timestamp =
        base::Microseconds(sample_time / kOneMicrosecondInMFSampleTimeUnits);
  }

  const bool keyframe = MFGetAttributeUINT32(
      output_data_buffer.pSample, MFSampleExtension_CleanPoint, false);

  int temporal_id = AssignTemporalId(keyframe);
  DVLOG(3) << "Encoded data with size:" << size << " keyframe " << keyframe;

  // If no bit stream buffer presents, queue the output first.
  if (bitstream_buffer_queue_.empty()) {
    DVLOG(3) << "No bitstream buffers.";

    // We need to copy the output so that encoding can continue.
    auto encode_output =
        std::make_unique<EncodeOutput>(size, keyframe, timestamp, temporal_id);
    {
      MediaBufferScopedPointer scoped_buffer(output_buffer.Get());
      memcpy(encode_output->memory(), scoped_buffer.get(), size);
    }
    encoder_output_queue_.push_back(std::move(encode_output));
    output_data_buffer.pSample->Release();
    output_data_buffer.pSample = nullptr;
    return;
  }

  // Immediately return encoded buffer with BitstreamBuffer to client.
  std::unique_ptr<MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef>
      buffer_ref = std::move(bitstream_buffer_queue_.front());
  bitstream_buffer_queue_.pop_front();

  {
    MediaBufferScopedPointer scoped_buffer(output_buffer.Get());
    memcpy(buffer_ref->mapping.memory(), scoped_buffer.get(), size);
  }

  output_data_buffer.pSample->Release();
  output_data_buffer.pSample = nullptr;

  BitstreamBufferMetadata md(size, keyframe, timestamp);
  if (temporalScalableCoding())
    md.h264.emplace().temporal_idx = temporal_id;
  main_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::BitstreamBufferReady, main_client_,
                                buffer_ref->id, md));
}

void MediaFoundationVideoEncodeAccelerator::ProcessOutputSync() {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  DWORD output_status = 0;
  HRESULT hr = encoder_->GetOutputStatus(&output_status);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get output status", );
  if (output_status != MFT_OUTPUT_STATUS_SAMPLE_READY) {
    DVLOG(3) << "Output isnt ready";
    return;
  }

  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  output_data_buffer.dwStreamID = 0;
  output_data_buffer.dwStatus = 0;
  output_data_buffer.pEvents = NULL;
  output_data_buffer.pSample = output_sample_.Get();
  DWORD status = 0;
  hr = encoder_->ProcessOutput(output_stream_id_, 1, &output_data_buffer,
                               &status);
  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    DVLOG(3) << "MF_E_TRANSFORM_NEED_MORE_INPUT" << status;
    return;
  }
  RETURN_ON_HR_FAILURE(hr, "Couldn't get encoded data", );
  DVLOG(3) << "Got encoded data " << hr;

  Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer;
  hr = output_sample_->GetBufferByIndex(0, &output_buffer);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get buffer by index", );
  DWORD size = 0;
  hr = output_buffer->GetCurrentLength(&size);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get buffer length", );

  base::TimeDelta timestamp;
  LONGLONG sample_time;
  hr = output_sample_->GetSampleTime(&sample_time);
  if (SUCCEEDED(hr)) {
    timestamp =
        base::Microseconds(sample_time / kOneMicrosecondInMFSampleTimeUnits);
  }

  const bool keyframe = MFGetAttributeUINT32(
      output_sample_.Get(), MFSampleExtension_CleanPoint, false);
  DVLOG(3) << "We HAVE encoded data with size:" << size << " keyframe "
           << keyframe;

  if (bitstream_buffer_queue_.empty()) {
    DVLOG(3) << "No bitstream buffers.";
    // We need to copy the output so that encoding can continue.
    std::unique_ptr<EncodeOutput> encode_output(
        new EncodeOutput(size, keyframe, timestamp));
    {
      MediaBufferScopedPointer scoped_buffer(output_buffer.Get());
      memcpy(encode_output->memory(), scoped_buffer.get(), size);
    }
    encoder_output_queue_.push_back(std::move(encode_output));
    return;
  }

  std::unique_ptr<MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef>
      buffer_ref = std::move(bitstream_buffer_queue_.front());
  bitstream_buffer_queue_.pop_front();

  {
    MediaBufferScopedPointer scoped_buffer(output_buffer.Get());
    memcpy(buffer_ref->mapping.memory(), scoped_buffer.get(), size);
  }

  main_client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Client::BitstreamBufferReady, main_client_,
                     buffer_ref->id,
                     BitstreamBufferMetadata(size, keyframe, timestamp)));

  // Keep calling ProcessOutput recursively until MF_E_TRANSFORM_NEED_MORE_INPUT
  // is returned to flush out all the output.
  ProcessOutputSync();
}

bool MediaFoundationVideoEncodeAccelerator::TryToDeliverInputFrame(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  bool input_delivered = false;
  Microsoft::WRL::ComPtr<IMFMediaEvent> media_event;
  MediaEventType event_type;
  do {
    HRESULT hr =
        event_generator_->GetEvent(MF_EVENT_FLAG_NO_WAIT, &media_event);
    if (FAILED(hr)) {
      break;
    }

    hr = media_event->GetType(&event_type);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get the type of media event.";
      break;
    }

    switch (event_type) {
      case METransformHaveOutput: {
        ProcessOutputAsync();
        continue;
      }
      case METransformNeedInput: {
        hr = ProcessInput(std::move(frame), force_keyframe);
        if (FAILED(hr)) {
          NotifyError(kPlatformFailureError);
          RETURN_ON_HR_FAILURE(hr, "Couldn't encode", false);
        }

        DVLOG(3) << "Sent for encode " << hr;
        return true;
      }
      default:
        break;
    }
  } while (true);

  return input_delivered;
}

void MediaFoundationVideoEncodeAccelerator::TryToReturnBitstreamBuffer() {
  // Try to fetch the encoded frame in time.
  bool output_processed = false;
  do {
    Microsoft::WRL::ComPtr<IMFMediaEvent> media_event;
    MediaEventType event_type;
    HRESULT hr =
        event_generator_->GetEvent(MF_EVENT_FLAG_NO_WAIT, &media_event);
    if (FAILED(hr)) {
      if (!output_processed) {
        continue;
      } else {
        break;
      }
    }

    hr = media_event->GetType(&event_type);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get the type of media event.";
      break;
    }

    switch (event_type) {
      case METransformHaveOutput: {
        ProcessOutputAsync();
        output_processed = true;
        break;
      }
      case METransformNeedInput: {
        input_required_ = true;
        continue;
      }
      default:
        break;
    }
  } while (true);
}

void MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // If there is already EncodeOutput waiting, copy its output first.
  if (!encoder_output_queue_.empty()) {
    std::unique_ptr<MediaFoundationVideoEncodeAccelerator::EncodeOutput>
        encode_output = std::move(encoder_output_queue_.front());
    encoder_output_queue_.pop_front();
    memcpy(buffer_ref->mapping.memory(), encode_output->memory(),
           encode_output->size());

    BitstreamBufferMetadata md(encode_output->size(), encode_output->keyframe,
                               encode_output->capture_timestamp);
    if (temporalScalableCoding())
      md.h264.emplace().temporal_idx = encode_output->temporal_layer_id;
    main_client_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Client::BitstreamBufferReady, main_client_,
                                  buffer_ref->id, md));
    return;
  }

  bitstream_buffer_queue_.push_back(std::move(buffer_ref));
}

void MediaFoundationVideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    const media::Bitrate& bitrate,
    uint32_t framerate) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(imf_output_media_type_);
  DCHECK(imf_input_media_type_);
  DCHECK(encoder_);
  RETURN_ON_FAILURE(bitrate.mode() == bitrate_.mode(),
                    "Invalid bitrate mode", );

  framerate = base::clamp(framerate, 1u, uint32_t{kMaxFrameRateNumerator});

  if (frame_rate_ != framerate) {
    HRESULT hr = MFSetAttributeRatio(imf_output_media_type_.Get(),
                                     MF_MT_FRAME_RATE, framerate, 1);
    RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate for output type", );

    imf_output_media_type_->SetUINT32(MF_MT_AVG_BITRATE, bitrate.target());
    RETURN_ON_HR_FAILURE(hr, "Couldn't set average bitrate for output type", );

    hr = MFSetAttributeRatio(imf_input_media_type_.Get(), MF_MT_FRAME_RATE,
                             framerate, 1);
    RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate for input type", );

    if (is_async_mft_) {
      // Some HMFTs will reject output type change with MF_E_INVALIDTYPE due
      // to temporary mismatch between output/input media types, so we always
      // clear the input/output media types before reconfiguring them
      // dynamically.
      hr = encoder_->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
      RETURN_ON_HR_FAILURE(
          hr, "Couldn't process message MFT_MESSAGE_COMMAND_DRAIN", );

      DrainPendingOutputs();

      hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
      RETURN_ON_HR_FAILURE(
          hr, "Couldn't process message MFT_MESSAGE_NOTIFY_END_OF_STREAM", );

      hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
      RETURN_ON_HR_FAILURE(
          hr, "Couldn't process message MFT_MESSAGE_NOTIFY_END_STREAMING", );

      hr = encoder_->SetInputType(input_stream_id_, nullptr, 0);
      RETURN_ON_HR_FAILURE(hr, "Couldn't clear input media type.", );

      hr = encoder_->SetOutputType(output_stream_id_, nullptr, 0);
      RETURN_ON_HR_FAILURE(hr, "Couldn't clear ouput media type.", );

      hr = encoder_->SetOutputType(output_stream_id_,
                                   imf_output_media_type_.Get(), 0);
      RETURN_ON_HR_FAILURE(hr, "Couldn't set output media type", );

      hr = encoder_->SetInputType(input_stream_id_, imf_input_media_type_.Get(),
                                  0);
      RETURN_ON_HR_FAILURE(hr, "Couldn't set input media type", );

      hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
      RETURN_ON_HR_FAILURE(
          hr, "Couldn't process message MFT_MESSAGE_NOTIFY_BEGIN_STREAMING", );

      hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
      RETURN_ON_HR_FAILURE(
          hr, "Couldn't process message MFT_MESSAGE_NOTIFY_START_OF_STREAM", );
    } else {
      hr = encoder_->SetOutputType(output_stream_id_,
                                   imf_output_media_type_.Get(), 0);
      RETURN_ON_HR_FAILURE(hr, "Couldn't set output media type", );

      hr = encoder_->SetInputType(input_stream_id_, imf_input_media_type_.Get(),
                                  0);
      RETURN_ON_HR_FAILURE(hr, "Couldn't set input media type", );
    }
    frame_rate_ = framerate;
  }

  if (bitrate_ != bitrate) {
    bitrate_ = bitrate;
    VARIANT var;
    var.vt = VT_UI4;
    var.ulVal = bitrate.target();
    HRESULT hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't update mean bitrate", );
    }

    if (bitrate.mode() == Bitrate::Mode::kVariable) {
      var.ulVal = bitrate.peak();
      hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMaxBitRate, &var);
      if (!compatible_with_win7_) {
        RETURN_ON_HR_FAILURE(hr, "Couldn't set max bitrate", );
      }
    }
  }
}

void MediaFoundationVideoEncodeAccelerator::DestroyTask() {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // Cancel all encoder thread callbacks.
  encoder_task_weak_factory_.InvalidateWeakPtrs();

  ReleaseEncoderResources();
}

void MediaFoundationVideoEncodeAccelerator::ReleaseEncoderResources() {
  while (!bitstream_buffer_queue_.empty())
    bitstream_buffer_queue_.pop_front();
  while (!encoder_output_queue_.empty())
    encoder_output_queue_.pop_front();

  if (activate_.Get() != nullptr) {
    activate_->ShutdownObject();
    activate_->Release();
    activate_.Reset();
  }
  encoder_.Reset();
  codec_api_.Reset();
  event_generator_.Reset();
  imf_input_media_type_.Reset();
  imf_output_media_type_.Reset();
  input_sample_.Reset();
  output_sample_.Reset();
}

HRESULT MediaFoundationVideoEncodeAccelerator::InitializeD3DVideoProcessing(
    ID3D11Texture2D* input_texture) {
  D3D11_TEXTURE2D_DESC input_desc = {};
  input_texture->GetDesc(&input_desc);
  if (vp_desc_.InputWidth == input_desc.Width &&
      vp_desc_.InputHeight == input_desc.Height) {
    return S_OK;
  }

  // Input/output framerates are dummy values for passthrough.
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC vp_desc = {
      .InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
      .InputFrameRate = {60, 1},
      .InputWidth = input_desc.Width,
      .InputHeight = input_desc.Height,
      .OutputFrameRate = {60, 1},
      .OutputWidth = static_cast<UINT>(input_visible_size_.width()),
      .OutputHeight = static_cast<UINT>(input_visible_size_.height()),
      .Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL};

  Microsoft::WRL::ComPtr<ID3D11Device> texture_device;
  input_texture->GetDevice(&texture_device);
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
  HRESULT hr = texture_device.As(&video_device);
  RETURN_ON_HR_FAILURE(hr, "Failed to query for ID3D11VideoDevice", hr);

  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>
      video_processor_enumerator;
  hr = video_device->CreateVideoProcessorEnumerator(
      &vp_desc, &video_processor_enumerator);
  RETURN_ON_HR_FAILURE(hr, "CreateVideoProcessorEnumerator failed", hr);

  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor;
  hr = video_device->CreateVideoProcessor(video_processor_enumerator.Get(), 0,
                                          &video_processor);
  RETURN_ON_HR_FAILURE(hr, "CreateVideoProcessor failed", hr);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  texture_device->GetImmediateContext(&device_context);
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context;
  hr = device_context.As(&video_context);
  RETURN_ON_HR_FAILURE(hr, "Failed to query for ID3D11VideoContext", hr);

  // Auto stream processing (the default) can hurt power consumption.
  video_context->VideoProcessorSetStreamAutoProcessingMode(
      video_processor.Get(), 0, FALSE);

  D3D11_TEXTURE2D_DESC scaled_desc = {
      .Width = static_cast<UINT>(input_visible_size_.width()),
      .Height = static_cast<UINT>(input_visible_size_.height()),
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_NV12,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
      .CPUAccessFlags = 0,
      .MiscFlags = 0};
  Microsoft::WRL::ComPtr<ID3D11Texture2D> scaled_d3d11_texture;
  hr = texture_device->CreateTexture2D(&scaled_desc, nullptr,
                                       &scaled_d3d11_texture);
  RETURN_ON_HR_FAILURE(hr, "Failed to create texture", hr);

  hr = media::SetDebugName(scaled_d3d11_texture.Get(),
                           "MFVideoEncodeAccelerator_ScaledTexture");
  RETURN_ON_HR_FAILURE(hr, "Failed to set debug name", hr);

  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_desc = {};
  output_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
  output_desc.Texture2D.MipSlice = 0;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> vp_output_view;
  hr = video_device->CreateVideoProcessorOutputView(
      scaled_d3d11_texture.Get(), video_processor_enumerator.Get(),
      &output_desc, &vp_output_view);
  RETURN_ON_HR_FAILURE(hr, "CreateVideoProcessorOutputView failed", hr);

  video_device_ = std::move(video_device);
  video_processor_enumerator_ = std::move(video_processor_enumerator);
  video_processor_ = std::move(video_processor);
  video_context_ = std::move(video_context);
  vp_desc_ = std::move(vp_desc);
  scaled_d3d11_texture_ = std::move(scaled_d3d11_texture);
  vp_output_view_ = std::move(vp_output_view);
  return S_OK;
}

HRESULT MediaFoundationVideoEncodeAccelerator::PerformD3DScaling(
    ID3D11Texture2D* input_texture) {
  HRESULT hr = InitializeD3DVideoProcessing(input_texture);
  RETURN_ON_HR_FAILURE(hr, "Couldn't initialize D3D video processing", hr);

  // Set the color space for passthrough.
  auto src_color_space = gfx::ColorSpace::CreateSRGB();
  auto output_color_space = gfx::ColorSpace::CreateSRGB();

  D3D11_VIDEO_PROCESSOR_COLOR_SPACE src_d3d11_color_space =
      gfx::ColorSpaceWin::GetD3D11ColorSpace(src_color_space);
  video_context_->VideoProcessorSetStreamColorSpace(video_processor_.Get(), 0,
                                                    &src_d3d11_color_space);
  D3D11_VIDEO_PROCESSOR_COLOR_SPACE output_d3d11_color_space =
      gfx::ColorSpaceWin::GetD3D11ColorSpace(output_color_space);
  video_context_->VideoProcessorSetOutputColorSpace(video_processor_.Get(),
                                                    &output_d3d11_color_space);

  {
    absl::optional<gpu::DXGIScopedReleaseKeyedMutex> release_keyed_mutex;
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
    hr = input_texture->QueryInterface(IID_PPV_ARGS(&keyed_mutex));
    if (SUCCEEDED(hr)) {
      // The producer may still be using this texture for a short period of
      // time, so wait long enough to hopefully avoid glitches. For example,
      // all levels of the texture share the same keyed mutex, so if the
      // hardware decoder acquired the mutex to decode into a different array
      // level then it still may block here temporarily.
      constexpr int kMaxSyncTimeMs = 100;
      hr = keyed_mutex->AcquireSync(0, kMaxSyncTimeMs);
      RETURN_ON_HR_FAILURE(hr, "Failed to acquire keyed mutex", hr);
      release_keyed_mutex.emplace(std::move(keyed_mutex), 0);
    }

    // Setup |video_context_| for VPBlt operation.
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_desc = {};
    input_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    input_desc.Texture2D.ArraySlice = 0;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> input_view;
    hr = video_device_->CreateVideoProcessorInputView(
        input_texture, video_processor_enumerator_.Get(), &input_desc,
        &input_view);
    RETURN_ON_HR_FAILURE(hr, "CreateVideoProcessorInputView failed", hr);

    D3D11_VIDEO_PROCESSOR_STREAM stream = {.Enable = true,
                                           .OutputIndex = 0,
                                           .InputFrameOrField = 0,
                                           .PastFrames = 0,
                                           .FutureFrames = 0,
                                           .pInputSurface = input_view.Get()};

    D3D11_TEXTURE2D_DESC input_texture_desc = {};
    input_texture->GetDesc(&input_texture_desc);
    RECT source_rect = {0, 0, static_cast<LONG>(input_texture_desc.Width),
                        static_cast<LONG>(input_texture_desc.Height)};
    video_context_->VideoProcessorSetStreamSourceRect(video_processor_.Get(), 0,
                                                      TRUE, &source_rect);

    D3D11_TEXTURE2D_DESC output_texture_desc = {};
    scaled_d3d11_texture_->GetDesc(&output_texture_desc);
    RECT dest_rect = {0, 0, static_cast<LONG>(output_texture_desc.Width),
                      static_cast<LONG>(output_texture_desc.Height)};
    video_context_->VideoProcessorSetOutputTargetRect(video_processor_.Get(),
                                                      TRUE, &dest_rect);
    video_context_->VideoProcessorSetStreamDestRect(video_processor_.Get(), 0,
                                                    TRUE, &dest_rect);

    hr = video_context_->VideoProcessorBlt(
        video_processor_.Get(), vp_output_view_.Get(), 0, 1, &stream);
    RETURN_ON_HR_FAILURE(hr, "VideoProcessorBlt failed", hr);
  }

  return hr;
}

void MediaFoundationVideoEncodeAccelerator::DrainPendingOutputs() {
  Microsoft::WRL::ComPtr<IMFMediaEvent> media_event;

  while ((SUCCEEDED(
      event_generator_->GetEvent(MF_EVENT_FLAG_NO_WAIT, &media_event)))) {
    MediaEventType event_type;
    HRESULT hr = media_event->GetType(&event_type);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get the type of media event.";
      continue;
    }

    if (event_type == METransformHaveOutput) {
      ProcessOutputAsync();
    }
  }
}

}  // namespace media
