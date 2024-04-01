// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_device_win.h"

#include <ks.h>
#include <ksmedia.h>
#include <objbase.h>

#include <algorithm>
#include <list>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "base/win/win_util.h"
#include "media/base/media_switches.h"
#include "media/base/timestamp_constants.h"
#include "media/capture/mojom/image_capture_types.h"
#include "media/capture/video/blob_utils.h"
#include "media/capture/video/win/metrics.h"
#include "media/capture/video/win/video_capture_device_utils_win.h"

using base::win::ScopedCoMem;
using base::win::ScopedVariant;
using Microsoft::WRL::ComPtr;

namespace media {

#if DCHECK_IS_ON()
#define DLOG_IF_FAILED_WITH_HRESULT(message, hr)                      \
  {                                                                   \
    DLOG_IF(ERROR, FAILED(hr))                                        \
        << (message) << ": " << logging::SystemErrorCodeToString(hr); \
  }
#else
#define DLOG_IF_FAILED_WITH_HRESULT(message, hr) \
  {}
#endif

// Check if a Pin matches a category.
bool PinMatchesCategory(IPin* pin, REFGUID category) {
  DCHECK(pin);
  bool found = false;
  ComPtr<IKsPropertySet> ks_property;
  HRESULT hr = pin->QueryInterface(IID_PPV_ARGS(&ks_property));
  if (SUCCEEDED(hr)) {
    GUID pin_category;
    DWORD return_value;
    hr = ks_property->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, nullptr, 0,
                          &pin_category, sizeof(pin_category), &return_value);
    if (SUCCEEDED(hr) && (return_value == sizeof(pin_category))) {
      found = (pin_category == category);
    }
  }
  return found;
}

// Check if a Pin's MediaType matches a given |major_type|.
bool PinMatchesMajorType(IPin* pin, REFGUID major_type) {
  DCHECK(pin);
  AM_MEDIA_TYPE connection_media_type;
  const HRESULT hr = pin->ConnectionMediaType(&connection_media_type);
  return SUCCEEDED(hr) && connection_media_type.majortype == major_type;
}

// Check if the video capture device supports pan, tilt and zoom controls.
// static
VideoCaptureControlSupport VideoCaptureDeviceWin::GetControlSupport(
    ComPtr<IBaseFilter> capture_filter) {
  VideoCaptureControlSupport control_support;
  ComPtr<ICameraControl> camera_control;
  ComPtr<IVideoProcAmp> video_control;

  if (GetCameraAndVideoControls(capture_filter, &camera_control,
                                &video_control)) {
    long min, max, step, default_value, flags;
    control_support.pan = SUCCEEDED(camera_control->getRange_Pan(
                              &min, &max, &step, &default_value, &flags)) &&
                          min < max;
    control_support.tilt = SUCCEEDED(camera_control->getRange_Tilt(
                               &min, &max, &step, &default_value, &flags)) &&
                           min < max;
    control_support.zoom = SUCCEEDED(camera_control->getRange_Zoom(
                               &min, &max, &step, &default_value, &flags)) &&
                           min < max;
  }

  return control_support;
}

// static
void VideoCaptureDeviceWin::GetDeviceCapabilityList(
    ComPtr<IBaseFilter> capture_filter,
    bool query_detailed_frame_rates,
    CapabilityList* out_capability_list) {
  ComPtr<IPin> output_capture_pin(VideoCaptureDeviceWin::GetPin(
      capture_filter.Get(), PINDIR_OUTPUT, PIN_CATEGORY_CAPTURE, GUID_NULL));
  if (!output_capture_pin.Get()) {
    DLOG(ERROR) << "Failed to get capture output pin";
    return;
  }

  GetPinCapabilityList(capture_filter, output_capture_pin,
                       query_detailed_frame_rates, out_capability_list);
}

// static
void VideoCaptureDeviceWin::GetPinCapabilityList(
    ComPtr<IBaseFilter> capture_filter,
    ComPtr<IPin> output_capture_pin,
    bool query_detailed_frame_rates,
    CapabilityList* out_capability_list) {
  ComPtr<IAMStreamConfig> stream_config;
  HRESULT hr = output_capture_pin.As(&stream_config);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get IAMStreamConfig interface from "
                   "capture device: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  // Get interface used for getting the frame rate.
  ComPtr<IAMVideoControl> video_control;
  hr = capture_filter.As(&video_control);

  int count = 0, byte_size = 0;
  hr = stream_config->GetNumberOfCapabilities(&count, &byte_size);
  if (FAILED(hr)) {
    DLOG(ERROR) << "GetNumberOfCapabilities failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  std::unique_ptr<BYTE[]> caps(new BYTE[byte_size]);
  for (int i = 0; i < count; ++i) {
    VideoCaptureDeviceWin::ScopedMediaType media_type;
    hr = stream_config->GetStreamCaps(i, media_type.Receive(), caps.get());
    // GetStreamCaps() may return S_FALSE, so don't use FAILED() or SUCCEED()
    // macros here since they'll trigger incorrectly.
    if (hr != S_OK || !media_type.get()) {
      DLOG(ERROR) << "GetStreamCaps failed: "
                  << logging::SystemErrorCodeToString(hr);
      return;
    }

    if (media_type->majortype == MEDIATYPE_Video &&
        media_type->formattype == FORMAT_VideoInfo) {
      VideoCaptureFormat format;
      format.pixel_format =
          VideoCaptureDeviceWin::TranslateMediaSubtypeToPixelFormat(
              media_type->subtype);
      if (format.pixel_format == PIXEL_FORMAT_UNKNOWN)
        continue;
      VIDEOINFOHEADER* h =
          reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat);
      format.frame_size.SetSize(h->bmiHeader.biWidth, h->bmiHeader.biHeight);

      std::vector<float> frame_rates;
      if (query_detailed_frame_rates && video_control.Get()) {
        // Try to get a better |time_per_frame| from IAMVideoControl. If not,
        // use the value from VIDEOINFOHEADER.
        ScopedCoMem<LONGLONG> time_per_frame_list;
        LONG list_size = 0;
        const SIZE size = {format.frame_size.width(),
                           format.frame_size.height()};
        hr = video_control->GetFrameRateList(output_capture_pin.Get(), i, size,
                                             &list_size, &time_per_frame_list);
        // Sometimes |list_size| will be > 0, but time_per_frame_list will be
        // NULL. Some drivers may return an HRESULT of S_FALSE which
        // SUCCEEDED() translates into success, so explicitly check S_OK. See
        // http://crbug.com/306237.
        if (hr == S_OK && list_size > 0 && time_per_frame_list) {
          for (int k = 0; k < list_size; k++) {
            LONGLONG time_per_frame = *(time_per_frame_list.get() + k);
            if (time_per_frame <= 0)
              continue;
            frame_rates.push_back(kSecondsToReferenceTime /
                                  static_cast<float>(time_per_frame));
          }
        }
      }

      if (frame_rates.empty() && h->AvgTimePerFrame > 0) {
        frame_rates.push_back(kSecondsToReferenceTime /
                              static_cast<float>(h->AvgTimePerFrame));
      }
      if (frame_rates.empty())
        frame_rates.push_back(0.0f);

      for (const auto& frame_rate : frame_rates) {
        format.frame_rate = frame_rate;
        out_capability_list->emplace_back(i, format, h->bmiHeader);
      }
    }
  }
}

// Finds an IPin on an IBaseFilter given the direction, Category and/or Major
// Type. If either |category| or |major_type| are GUID_NULL, they are ignored.
// static
ComPtr<IPin> VideoCaptureDeviceWin::GetPin(ComPtr<IBaseFilter> capture_filter,
                                           PIN_DIRECTION pin_dir,
                                           REFGUID category,
                                           REFGUID major_type) {
  ComPtr<IPin> pin;
  ComPtr<IEnumPins> pin_enum;
  HRESULT hr = capture_filter->EnumPins(&pin_enum);
  if (pin_enum.Get() == nullptr)
    return pin;

  // Get first unconnected pin.
  hr = pin_enum->Reset();  // set to first pin
  while ((hr = pin_enum->Next(1, &pin, nullptr)) == S_OK) {
    PIN_DIRECTION this_pin_dir = static_cast<PIN_DIRECTION>(-1);
    hr = pin->QueryDirection(&this_pin_dir);
    if (pin_dir == this_pin_dir) {
      if ((category == GUID_NULL || PinMatchesCategory(pin.Get(), category)) &&
          (major_type == GUID_NULL ||
           PinMatchesMajorType(pin.Get(), major_type))) {
        return pin;
      }
    }
    pin.Reset();
  }

  DCHECK(!pin.Get());
  return pin;
}

// static
VideoPixelFormat VideoCaptureDeviceWin::TranslateMediaSubtypeToPixelFormat(
    const GUID& sub_type) {
  static struct {
    const GUID& sub_type;
    VideoPixelFormat format;
  } const kMediaSubtypeToPixelFormatCorrespondence[] = {
      {kMediaSubTypeI420, PIXEL_FORMAT_I420},
      {MEDIASUBTYPE_IYUV, PIXEL_FORMAT_I420},
      {MEDIASUBTYPE_RGB24, PIXEL_FORMAT_RGB24},
      {MEDIASUBTYPE_RGB32, PIXEL_FORMAT_ARGB},
      {MEDIASUBTYPE_YUY2, PIXEL_FORMAT_YUY2},
      {MEDIASUBTYPE_MJPG, PIXEL_FORMAT_MJPEG},
      {MEDIASUBTYPE_UYVY, PIXEL_FORMAT_UYVY},
      {MEDIASUBTYPE_ARGB32, PIXEL_FORMAT_ARGB},
      {kMediaSubTypeHDYC, PIXEL_FORMAT_UYVY},
      {kMediaSubTypeY16, PIXEL_FORMAT_Y16},
      {kMediaSubTypeZ16, PIXEL_FORMAT_Y16},
      {kMediaSubTypeINVZ, PIXEL_FORMAT_Y16},
  };
  for (const auto& pixel_format : kMediaSubtypeToPixelFormatCorrespondence) {
    if (sub_type == pixel_format.sub_type)
      return pixel_format.format;
  }
  DVLOG(2) << "Device (also) supports an unknown media type "
           << base::win::WStringFromGUID(sub_type);
  return PIXEL_FORMAT_UNKNOWN;
}

void VideoCaptureDeviceWin::ScopedMediaType::Free() {
  if (!media_type_)
    return;

  DeleteMediaType(media_type_);
  media_type_ = nullptr;
}

AM_MEDIA_TYPE** VideoCaptureDeviceWin::ScopedMediaType::Receive() {
  DCHECK(!media_type_);
  return &media_type_;
}

// Release the format block for a media type.
// http://msdn.microsoft.com/en-us/library/dd375432(VS.85).aspx
void VideoCaptureDeviceWin::ScopedMediaType::FreeMediaType(AM_MEDIA_TYPE* mt) {
  if (mt->cbFormat != 0) {
    CoTaskMemFree(mt->pbFormat);
    mt->cbFormat = 0;
    mt->pbFormat = nullptr;
  }
  if (mt->pUnk != nullptr) {
    NOTREACHED();
    // pUnk should not be used.
    mt->pUnk->Release();
    mt->pUnk = nullptr;
  }
}

// Delete a media type structure that was allocated on the heap.
// http://msdn.microsoft.com/en-us/library/dd375432(VS.85).aspx
void VideoCaptureDeviceWin::ScopedMediaType::DeleteMediaType(
    AM_MEDIA_TYPE* mt) {
  if (mt != nullptr) {
    FreeMediaType(mt);
    CoTaskMemFree(mt);
  }
}

VideoCaptureDeviceWin::VideoCaptureDeviceWin(
    const VideoCaptureDeviceDescriptor& device_descriptor,
    ComPtr<IBaseFilter> capture_filter)
    : device_descriptor_(device_descriptor),
      state_(kIdle),
      capture_filter_(std::move(capture_filter)),
      white_balance_mode_manual_(false),
      exposure_mode_manual_(false),
      focus_mode_manual_(false),
      enable_get_photo_state_(
          base::FeatureList::IsEnabled(media::kDirectShowGetPhotoState)) {
  // TODO(mcasas): Check that CoInitializeEx() has been called with the
  // appropriate Apartment model, i.e., Single Threaded.
}

VideoCaptureDeviceWin::~VideoCaptureDeviceWin() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (media_control_.Get())
    media_control_->Stop();

  if (graph_builder_.Get()) {
    if (sink_filter_.get()) {
      graph_builder_->RemoveFilter(sink_filter_.get());
      sink_filter_.reset();
    }

    if (capture_filter_.Get())
      graph_builder_->RemoveFilter(capture_filter_.Get());
  }

  if (capture_graph_builder_.Get())
    capture_graph_builder_.Reset();

  if (!take_photo_callbacks_.empty()) {
    for (size_t k = 0; k < take_photo_callbacks_.size(); k++) {
      LogWindowsImageCaptureOutcome(
          VideoCaptureWinBackend::kDirectShow,
          ImageCaptureOutcome::kFailedUsingVideoStream,
          IsHighResolution(capture_format_));
    }
  }
}

bool VideoCaptureDeviceWin::Init() {
  DCHECK(thread_checker_.CalledOnValidThread());
  output_capture_pin_ = GetPin(capture_filter_.Get(), PINDIR_OUTPUT,
                               PIN_CATEGORY_CAPTURE, GUID_NULL);
  if (!output_capture_pin_.Get()) {
    DLOG(ERROR) << "Failed to get capture output pin";
    return false;
  }

  // Create the sink filter used for receiving Captured frames.
  sink_filter_ = new SinkFilter(this);
  if (sink_filter_.get() == nullptr) {
    DLOG(ERROR) << "Failed to create sink filter";
    return false;
  }

  input_sink_pin_ = sink_filter_->GetPin(0);

  HRESULT hr =
      ::CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&graph_builder_));
  DLOG_IF_FAILED_WITH_HRESULT("Failed to create capture filter", hr);
  if (FAILED(hr))
    return false;

  hr = ::CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC,
                          IID_PPV_ARGS(&capture_graph_builder_));
  DLOG_IF_FAILED_WITH_HRESULT("Failed to create the Capture Graph Builder", hr);
  if (FAILED(hr))
    return false;

  hr = capture_graph_builder_->SetFiltergraph(graph_builder_.Get());
  DLOG_IF_FAILED_WITH_HRESULT("Failed to give graph to capture graph builder",
                              hr);
  if (FAILED(hr))
    return false;

  hr = graph_builder_.As(&media_control_);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to create media control builder", hr);
  if (FAILED(hr))
    return false;

  hr = graph_builder_->AddFilter(capture_filter_.Get(), nullptr);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to add the capture device to the graph",
                              hr);
  if (FAILED(hr))
    return false;

  hr = graph_builder_->AddFilter(sink_filter_.get(), nullptr);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to add the sink filter to the graph", hr);
  if (FAILED(hr))
    return false;

  // The following code builds the upstream portions of the graph, for example
  // if a capture device uses a Windows Driver Model (WDM) driver, the graph may
  // require certain filters upstream from the WDM Video Capture filter, such as
  // a TV Tuner filter or an Analog Video Crossbar filter. We try using the more
  // prevalent MEDIATYPE_Interleaved first.
  ComPtr<IAMStreamConfig> stream_config;

  hr = capture_graph_builder_->FindInterface(
      &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, capture_filter_.Get(),
      IID_IAMStreamConfig, (void**)&stream_config);
  if (FAILED(hr)) {
    hr = capture_graph_builder_->FindInterface(
        &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, capture_filter_.Get(),
        IID_IAMStreamConfig, (void**)&stream_config);
    DLOG_IF_FAILED_WITH_HRESULT("Failed to find CapFilter:IAMStreamConfig", hr);
  }

  return CreateCapabilityMap();
}

void VideoCaptureDeviceWin::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ != kIdle)
    return;

  client_ = std::move(client);

  // Get the camera capability that best match the requested format.
  const CapabilityWin found_capability =
      GetBestMatchedCapability(params.requested_format, capabilities_);

  // Reduce the frame rate if the requested frame rate is lower
  // than the capability.
  const float frame_rate =
      std::min(params.requested_format.frame_rate,
               found_capability.supported_format.frame_rate);

  ComPtr<IAMStreamConfig> stream_config;
  HRESULT hr = output_capture_pin_.As(&stream_config);
  if (FAILED(hr)) {
    SetErrorState(
        media::VideoCaptureError::kWinDirectShowCantGetCaptureFormatSettings,
        FROM_HERE, "Can't get the Capture format settings", hr);
    return;
  }

  int count = 0, size = 0;
  hr = stream_config->GetNumberOfCapabilities(&count, &size);
  if (FAILED(hr)) {
    SetErrorState(
        media::VideoCaptureError::kWinDirectShowFailedToGetNumberOfCapabilities,
        FROM_HERE, "Failed to GetNumberOfCapabilities", hr);
    return;
  }

  std::unique_ptr<BYTE[]> caps(new BYTE[size]);
  ScopedMediaType media_type;

  // Get the windows capability from the capture device.
  // GetStreamCaps can return S_FALSE which we consider an error. Therefore the
  // FAILED macro can't be used.
  hr = stream_config->GetStreamCaps(found_capability.media_type_index,
                                    media_type.Receive(), caps.get());
  if (hr != S_OK) {
    SetErrorState(media::VideoCaptureError::
                      kWinDirectShowFailedToGetCaptureDeviceCapabilities,
                  FROM_HERE, "Failed to get capture device capabilities", hr);
    return;
  }
  if (media_type->formattype == FORMAT_VideoInfo) {
    VIDEOINFOHEADER* h =
        reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat);
    if (frame_rate > 0)
      h->AvgTimePerFrame = kSecondsToReferenceTime / frame_rate;
  }
  // Set the sink filter to request this format.
  sink_filter_->SetRequestedMediaFormat(
      found_capability.supported_format.pixel_format, frame_rate,
      found_capability.info_header);
  // Order the capture device to use this format.
  hr = stream_config->SetFormat(media_type.get());
  if (FAILED(hr)) {
    SetErrorState(media::VideoCaptureError::
                      kWinDirectShowFailedToSetCaptureDeviceOutputFormat,
                  FROM_HERE, "Failed to set capture device output format", hr);
    return;
  }
  capture_format_ = found_capability.supported_format;

  SetAntiFlickerInCaptureFilter(params);

  if (media_type->subtype == kMediaSubTypeHDYC) {
    // HDYC pixel format, used by the DeckLink capture card, needs an AVI
    // decompressor filter after source, let |graph_builder_| add it.
    hr = graph_builder_->Connect(output_capture_pin_.Get(),
                                 input_sink_pin_.Get());
  } else {
    hr = graph_builder_->ConnectDirect(output_capture_pin_.Get(),
                                       input_sink_pin_.Get(), nullptr);
  }

  if (FAILED(hr)) {
    SetErrorState(
        media::VideoCaptureError::kWinDirectShowFailedToConnectTheCaptureGraph,
        FROM_HERE, "Failed to connect the Capture graph.", hr);
    return;
  }

  hr = media_control_->Pause();
  if (FAILED(hr)) {
    SetErrorState(
        media::VideoCaptureError::kWinDirectShowFailedToPauseTheCaptureDevice,
        FROM_HERE, "Failed to pause the Capture device", hr);
    return;
  }

  // Start capturing.
  hr = media_control_->Run();
  if (FAILED(hr)) {
    SetErrorState(
        media::VideoCaptureError::kWinDirectShowFailedToStartTheCaptureDevice,
        FROM_HERE, "Failed to start the Capture device.", hr);
    return;
  }

  base::UmaHistogramEnumeration(
      "Media.VideoCapture.Win.Device.InternalPixelFormat",
      capture_format_.pixel_format, media::VideoPixelFormat::PIXEL_FORMAT_MAX);
  base::UmaHistogramEnumeration(
      "Media.VideoCapture.Win.Device.CapturePixelFormat",
      capture_format_.pixel_format, media::VideoPixelFormat::PIXEL_FORMAT_MAX);
  base::UmaHistogramEnumeration(
      "Media.VideoCapture.Win.Device.RequestedPixelFormat",
      params.requested_format.pixel_format,
      media::VideoPixelFormat::PIXEL_FORMAT_MAX);

  client_->OnStarted();
  state_ = kCapturing;
}

void VideoCaptureDeviceWin::StopAndDeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ != kCapturing)
    return;

  HRESULT hr = media_control_->Stop();
  if (FAILED(hr)) {
    SetErrorState(
        media::VideoCaptureError::kWinDirectShowFailedToStopTheCaptureGraph,
        FROM_HERE, "Failed to stop the capture graph.", hr);
    return;
  }

  graph_builder_->Disconnect(output_capture_pin_.Get());
  graph_builder_->Disconnect(input_sink_pin_.Get());

  client_.reset();
  state_ = kIdle;
}

void VideoCaptureDeviceWin::TakePhoto(TakePhotoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // DirectShow has other means of capturing still pictures, e.g. connecting a
  // SampleGrabber filter to a PIN_CATEGORY_STILL of |capture_filter_|. This
  // way, however, is not widespread and proves too cumbersome, so we just grab
  // the next captured frame instead.
  take_photo_callbacks_.push(std::move(callback));
}

void VideoCaptureDeviceWin::GetPhotoState(GetPhotoStateCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!enable_get_photo_state_)
    return;

  if (!camera_control_ || !video_control_) {
    if (!GetCameraAndVideoControls(capture_filter_, &camera_control_,
                                   &video_control_)) {
      return;
    }
  }

  auto photo_capabilities = mojo::CreateEmptyPhotoState();

  photo_capabilities->exposure_time = RetrieveControlRangeAndCurrent(
      [this](auto... args) {
        return this->camera_control_->getRange_Exposure(args...);
      },
      [this](auto... args) {
        return this->camera_control_->get_Exposure(args...);
      },
      &photo_capabilities->supported_exposure_modes,
      &photo_capabilities->current_exposure_mode,
      PlatformExposureTimeToCaptureValue, PlatformExposureTimeToCaptureStep);

  photo_capabilities->color_temperature = RetrieveControlRangeAndCurrent(
      [this](auto... args) {
        return this->video_control_->getRange_WhiteBalance(args...);
      },
      [this](auto... args) {
        return this->video_control_->get_WhiteBalance(args...);
      },
      &photo_capabilities->supported_white_balance_modes,
      &photo_capabilities->current_white_balance_mode);

  photo_capabilities->focus_distance = RetrieveControlRangeAndCurrent(
      [this](auto... args) {
        return this->camera_control_->getRange_Focus(args...);
      },
      [this](auto... args) {
        return this->camera_control_->get_Focus(args...);
      },
      &photo_capabilities->supported_focus_modes,
      &photo_capabilities->current_focus_mode);

  photo_capabilities->iso = mojom::Range::New();

  photo_capabilities->brightness = RetrieveControlRangeAndCurrent(
      [this](auto... args) {
        return this->video_control_->getRange_Brightness(args...);
      },
      [this](auto... args) {
        return this->video_control_->get_Brightness(args...);
      });
  photo_capabilities->contrast = RetrieveControlRangeAndCurrent(
      [this](auto... args) {
        return this->video_control_->getRange_Contrast(args...);
      },
      [this](auto... args) {
        return this->video_control_->get_Contrast(args...);
      });
  photo_capabilities->saturation = RetrieveControlRangeAndCurrent(
      [this](auto... args) {
        return this->video_control_->getRange_Saturation(args...);
      },
      [this](auto... args) {
        return this->video_control_->get_Saturation(args...);
      });
  photo_capabilities->sharpness = RetrieveControlRangeAndCurrent(
      [this](auto... args) {
        return this->video_control_->getRange_Sharpness(args...);
      },
      [this](auto... args) {
        return this->video_control_->get_Sharpness(args...);
      });
  photo_capabilities->pan = RetrieveControlRangeAndCurrent(
      [this](auto... args) {
        return this->camera_control_->getRange_Pan(args...);
      },
      [this](auto... args) { return this->camera_control_->get_Pan(args...); },
      nullptr, nullptr, PlatformAngleToCaptureValue,
      PlatformAngleToCaptureStep);
  photo_capabilities->tilt = RetrieveControlRangeAndCurrent(
      [this](auto... args) {
        return this->camera_control_->getRange_Tilt(args...);
      },
      [this](auto... args) { return this->camera_control_->get_Tilt(args...); },
      nullptr, nullptr, PlatformAngleToCaptureValue,
      PlatformAngleToCaptureStep);
  photo_capabilities->zoom = RetrieveControlRangeAndCurrent(
      [this](auto... args) {
        return this->camera_control_->getRange_Zoom(args...);
      },
      [this](auto... args) {
        return this->camera_control_->get_Zoom(args...);
      });

  photo_capabilities->red_eye_reduction = mojom::RedEyeReduction::NEVER;
  photo_capabilities->height = mojom::Range::New(
      capture_format_.frame_size.height(), capture_format_.frame_size.height(),
      capture_format_.frame_size.height(), 0 /* step */);
  photo_capabilities->width = mojom::Range::New(
      capture_format_.frame_size.width(), capture_format_.frame_size.width(),
      capture_format_.frame_size.width(), 0 /* step */);
  photo_capabilities->torch = false;

  std::move(callback).Run(std::move(photo_capabilities));
}

void VideoCaptureDeviceWin::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!camera_control_ || !video_control_) {
    if (!GetCameraAndVideoControls(capture_filter_, &camera_control_,
                                   &video_control_)) {
      return;
    }
  }

  HRESULT hr;

  if (settings->has_white_balance_mode) {
    if (settings->white_balance_mode == mojom::MeteringMode::CONTINUOUS) {
      hr = video_control_->put_WhiteBalance(0L, VideoProcAmp_Flags_Auto);
      DLOG_IF_FAILED_WITH_HRESULT("Auto white balance config failed", hr);
      if (FAILED(hr))
        return;

      white_balance_mode_manual_ = false;
    } else {
      white_balance_mode_manual_ = true;
    }
  }
  if (white_balance_mode_manual_ && settings->has_color_temperature) {
    hr = video_control_->put_WhiteBalance(settings->color_temperature,
                                          VideoProcAmp_Flags_Manual);
    DLOG_IF_FAILED_WITH_HRESULT("Color temperature config failed", hr);
    if (FAILED(hr))
      return;
  }

  if (settings->has_focus_mode) {
    if (settings->focus_mode == mojom::MeteringMode::CONTINUOUS) {
      hr = camera_control_->put_Focus(0L, CameraControl_Flags_Auto);
      DLOG_IF_FAILED_WITH_HRESULT("Auto focus config failed", hr);
      if (FAILED(hr))
        return;

      focus_mode_manual_ = false;
    } else {
      focus_mode_manual_ = true;
    }
  }
  if (focus_mode_manual_ && settings->has_focus_distance) {
    hr = camera_control_->put_Focus(settings->focus_distance,
                                    CameraControl_Flags_Manual);
    DLOG_IF_FAILED_WITH_HRESULT("Focus Distance config failed", hr);
    if (FAILED(hr))
      return;
  }

  if (settings->has_exposure_mode) {
    if (settings->exposure_mode == mojom::MeteringMode::CONTINUOUS) {
      hr = camera_control_->put_Exposure(0L, CameraControl_Flags_Auto);
      DLOG_IF_FAILED_WITH_HRESULT("Auto exposure config failed", hr);
      if (FAILED(hr))
        return;

      exposure_mode_manual_ = false;
    } else {
      exposure_mode_manual_ = true;
    }
  }
  if (exposure_mode_manual_ && settings->has_exposure_time) {
    // Windows expects the exposure time in log base 2 seconds.
    hr = camera_control_->put_Exposure(
        CaptureExposureTimeToPlatformValue(settings->exposure_time),
        CameraControl_Flags_Manual);
    DLOG_IF_FAILED_WITH_HRESULT("Exposure Time config failed", hr);
    if (FAILED(hr))
      return;
  }
  if (settings->has_brightness) {
    hr = video_control_->put_Brightness(settings->brightness,
                                        VideoProcAmp_Flags_Manual);
    DLOG_IF_FAILED_WITH_HRESULT("Brightness config failed", hr);
    if (FAILED(hr))
      return;
  }
  if (settings->has_contrast) {
    hr = video_control_->put_Contrast(settings->contrast,
                                      VideoProcAmp_Flags_Manual);
    DLOG_IF_FAILED_WITH_HRESULT("Contrast config failed", hr);
    if (FAILED(hr))
      return;
  }
  if (settings->has_saturation) {
    hr = video_control_->put_Saturation(settings->saturation,
                                        VideoProcAmp_Flags_Manual);
    DLOG_IF_FAILED_WITH_HRESULT("Saturation config failed", hr);
    if (FAILED(hr))
      return;
  }
  if (settings->has_sharpness) {
    hr = video_control_->put_Sharpness(settings->sharpness,
                                       VideoProcAmp_Flags_Manual);
    DLOG_IF_FAILED_WITH_HRESULT("Sharpness config failed", hr);
    if (FAILED(hr))
      return;
  }
  if (settings->has_pan) {
    hr = camera_control_->put_Pan(CaptureAngleToPlatformValue(settings->pan),
                                  CameraControl_Flags_Manual);
    DLOG_IF_FAILED_WITH_HRESULT("Pan config failed", hr);
    if (FAILED(hr))
      return;
  }
  if (settings->has_tilt) {
    hr = camera_control_->put_Tilt(CaptureAngleToPlatformValue(settings->tilt),
                                   CameraControl_Flags_Manual);
    DLOG_IF_FAILED_WITH_HRESULT("Tilt config failed", hr);
    if (FAILED(hr))
      return;
  }
  if (settings->has_zoom) {
    hr = camera_control_->put_Zoom(settings->zoom, CameraControl_Flags_Manual);
    DLOG_IF_FAILED_WITH_HRESULT("Zoom config failed", hr);
    if (FAILED(hr))
      return;
  }

  std::move(callback).Run(true);
}

// static
bool VideoCaptureDeviceWin::GetCameraAndVideoControls(
    ComPtr<IBaseFilter> capture_filter,
    ICameraControl** camera_control,
    IVideoProcAmp** video_control) {
  DCHECK(camera_control);
  DCHECK(video_control);
  DCHECK(!*camera_control);
  DCHECK(!*video_control);

  ComPtr<IKsTopologyInfo> info;
  HRESULT hr = capture_filter.As(&info);
  if (FAILED(hr)) {
    DLOG_IF_FAILED_WITH_HRESULT("Failed to obtain the topology info.", hr);
    return false;
  }

  DWORD num_nodes = 0;
  hr = info->get_NumNodes(&num_nodes);
  if (FAILED(hr)) {
    DLOG_IF_FAILED_WITH_HRESULT("Failed to obtain the number of nodes.", hr);
    return false;
  }

  // Every UVC camera is expected to have a single ICameraControl and a single
  // IVideoProcAmp nodes, and both are needed; ignore any unlikely later ones.
  GUID node_type;
  for (size_t i = 0; i < num_nodes; i++) {
    info->get_NodeType(i, &node_type);
    if (IsEqualGUID(node_type, KSNODETYPE_VIDEO_CAMERA_TERMINAL)) {
      hr = info->CreateNodeInstance(i, IID_PPV_ARGS(camera_control));
      if (SUCCEEDED(hr))
        break;
      DLOG_IF_FAILED_WITH_HRESULT("Failed to retrieve the ICameraControl.", hr);
      return false;
    }
  }
  for (size_t i = 0; i < num_nodes; i++) {
    info->get_NodeType(i, &node_type);
    if (IsEqualGUID(node_type, KSNODETYPE_VIDEO_PROCESSING)) {
      hr = info->CreateNodeInstance(i, IID_PPV_ARGS(video_control));
      if (SUCCEEDED(hr))
        break;
      DLOG_IF_FAILED_WITH_HRESULT("Failed to retrieve the IVideoProcAmp.", hr);
      return false;
    }
  }
  return *camera_control && *video_control;
}

// Implements SinkFilterObserver::SinkFilterObserver.
void VideoCaptureDeviceWin::FrameReceived(const uint8_t* buffer,
                                          int length,
                                          const VideoCaptureFormat& format,
                                          base::TimeDelta timestamp,
                                          bool flip_y) {
  if (first_ref_time_.is_null())
    first_ref_time_ = base::TimeTicks::Now();

  // There is a chance that the platform does not provide us with the timestamp,
  // in which case, we use reference time to calculate a timestamp.
  if (timestamp == kNoTimestamp)
    timestamp = base::TimeTicks::Now() - first_ref_time_;

  // We always calculate camera rotation for the first frame. We also cache the
  // latest value to use when AutoRotation is turned off.
  if (!camera_rotation_.has_value() || IsAutoRotationEnabled())
    camera_rotation_ = GetCameraRotation(device_descriptor_.facing);

  // TODO(julien.isorce): retrieve the color space information using the
  // DirectShow api, AM_MEDIA_TYPE::VIDEOINFOHEADER2::dwControlFlags. If
  // AMCONTROL_COLORINFO_PRESENT, then reinterpret dwControlFlags as a
  // DXVA_ExtendedFormat. Then use its fields DXVA_VideoPrimaries,
  // DXVA_VideoTransferMatrix, DXVA_VideoTransferFunction and
  // DXVA_NominalRangeto build a gfx::ColorSpace. See http://crbug.com/959992.
  client_->OnIncomingCapturedData(buffer, length, format, gfx::ColorSpace(),
                                  camera_rotation_.value(), flip_y,
                                  base::TimeTicks::Now(), timestamp);

  while (!take_photo_callbacks_.empty()) {
    TakePhotoCallback cb = std::move(take_photo_callbacks_.front());
    take_photo_callbacks_.pop();

    mojom::BlobPtr blob = RotateAndBlobify(buffer, length, format, 0);
    if (blob) {
      std::move(cb).Run(std::move(blob));
      LogWindowsImageCaptureOutcome(
          VideoCaptureWinBackend::kDirectShow,
          ImageCaptureOutcome::kSucceededUsingVideoStream,
          IsHighResolution(format));
    } else {
      LogWindowsImageCaptureOutcome(
          VideoCaptureWinBackend::kDirectShow,
          ImageCaptureOutcome::kFailedUsingVideoStream,
          IsHighResolution(format));
    }
  }
}

void VideoCaptureDeviceWin::FrameDropped(VideoCaptureFrameDropReason reason) {
  client_->OnFrameDropped(reason);
}

bool VideoCaptureDeviceWin::CreateCapabilityMap() {
  DCHECK(thread_checker_.CalledOnValidThread());
  GetPinCapabilityList(capture_filter_, output_capture_pin_,
                       true /* query_detailed_frame_rates */, &capabilities_);
  return !capabilities_.empty();
}

// Set the power line frequency removal in |capture_filter_| if available.
void VideoCaptureDeviceWin::SetAntiFlickerInCaptureFilter(
    const VideoCaptureParams& params) {
  const PowerLineFrequency power_line_frequency = GetPowerLineFrequency(params);
  if (power_line_frequency != PowerLineFrequency::FREQUENCY_50HZ &&
      power_line_frequency != PowerLineFrequency::FREQUENCY_60HZ) {
    return;
  }
  ComPtr<IKsPropertySet> ks_propset;
  DWORD type_support = 0;
  HRESULT hr;
  if (SUCCEEDED(hr = capture_filter_.As(&ks_propset)) &&
      SUCCEEDED(hr = ks_propset->QuerySupported(
                    PROPSETID_VIDCAP_VIDEOPROCAMP,
                    KSPROPERTY_VIDEOPROCAMP_POWERLINE_FREQUENCY,
                    &type_support)) &&
      (type_support & KSPROPERTY_SUPPORT_SET)) {
    KSPROPERTY_VIDEOPROCAMP_S data = {};
    data.Property.Set = PROPSETID_VIDCAP_VIDEOPROCAMP;
    data.Property.Id = KSPROPERTY_VIDEOPROCAMP_POWERLINE_FREQUENCY;
    data.Property.Flags = KSPROPERTY_TYPE_SET;
    data.Value =
        (power_line_frequency == PowerLineFrequency::FREQUENCY_50HZ) ? 1 : 2;
    data.Flags = KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL;
    hr = ks_propset->Set(PROPSETID_VIDCAP_VIDEOPROCAMP,
                         KSPROPERTY_VIDEOPROCAMP_POWERLINE_FREQUENCY, &data,
                         sizeof(data), &data, sizeof(data));
    DLOG_IF_FAILED_WITH_HRESULT("Anti-flicker setting failed", hr);
  }
}

void VideoCaptureDeviceWin::SetErrorState(media::VideoCaptureError error,
                                          const base::Location& from_here,
                                          const std::string& reason,
                                          HRESULT hr) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DLOG_IF_FAILED_WITH_HRESULT(reason, hr);
  state_ = kError;
  client_->OnError(error, from_here, reason);
}
}  // namespace media
