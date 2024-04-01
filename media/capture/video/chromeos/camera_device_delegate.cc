// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_device_delegate.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/cxx17_backports.h"
#include "base/no_destructor.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/mojom/image_capture_types.h"
#include "media/capture/video/blob_utils.h"
#include "media/capture/video/chromeos/camera_3a_controller.h"
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"
#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/chromeos/camera_hal_delegate.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "media/capture/video/chromeos/request_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media {

namespace {

constexpr char kBrightness[] = "com.google.control.brightness";
constexpr char kBrightnessRange[] = "com.google.control.brightnessRange";
constexpr char kContrast[] = "com.google.control.contrast";
constexpr char kContrastRange[] = "com.google.control.contrastRange";
constexpr char kPan[] = "com.google.control.pan";
constexpr char kPanRange[] = "com.google.control.panRange";
constexpr char kSaturation[] = "com.google.control.saturation";
constexpr char kSaturationRange[] = "com.google.control.saturationRange";
constexpr char kSharpness[] = "com.google.control.sharpness";
constexpr char kSharpnessRange[] = "com.google.control.sharpnessRange";
constexpr char kTilt[] = "com.google.control.tilt";
constexpr char kTiltRange[] = "com.google.control.tiltRange";
constexpr char kZoom[] = "com.google.control.zoom";
constexpr char kZoomRange[] = "com.google.control.zoomRange";
constexpr int32_t kColorTemperatureStep = 100;
constexpr int32_t kMicroToNano = 1000;

using AwbModeTemperatureMap = std::map<uint8_t, int32_t>;

const AwbModeTemperatureMap& GetAwbModeTemperatureMap() {
  // https://source.android.com/devices/camera/camera3_3Amodes#auto-wb
  static const base::NoDestructor<AwbModeTemperatureMap> kAwbModeTemperatureMap(
      {
          {static_cast<uint8_t>(cros::mojom::AndroidControlAwbMode::
                                    ANDROID_CONTROL_AWB_MODE_INCANDESCENT),
           2700},
          {static_cast<uint8_t>(cros::mojom::AndroidControlAwbMode::
                                    ANDROID_CONTROL_AWB_MODE_FLUORESCENT),
           5000},
          {static_cast<uint8_t>(cros::mojom::AndroidControlAwbMode::
                                    ANDROID_CONTROL_AWB_MODE_WARM_FLUORESCENT),
           3000},
          {static_cast<uint8_t>(cros::mojom::AndroidControlAwbMode::
                                    ANDROID_CONTROL_AWB_MODE_DAYLIGHT),
           5500},
          {static_cast<uint8_t>(cros::mojom::AndroidControlAwbMode::
                                    ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT),
           6500},
          {static_cast<uint8_t>(cros::mojom::AndroidControlAwbMode::
                                    ANDROID_CONTROL_AWB_MODE_TWILIGHT),
           15000},
          {static_cast<uint8_t>(cros::mojom::AndroidControlAwbMode::
                                    ANDROID_CONTROL_AWB_MODE_SHADE),
           7500},
      });
  return *kAwbModeTemperatureMap;
}

std::pair<int32_t, int32_t> GetTargetFrameRateRange(
    const cros::mojom::CameraMetadataPtr& static_metadata,
    int target_frame_rate,
    bool prefer_constant_frame_rate) {
  int32_t result_min = 0;
  int32_t result_max = 0;
  auto available_frame_rates = GetMetadataEntryAsSpan<int32_t>(
      static_metadata, cros::mojom::CameraMetadataTag::
                           ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
  for (size_t idx = 0; idx < available_frame_rates.size(); idx += 2) {
    int32_t min = available_frame_rates[idx];
    int32_t max = available_frame_rates[idx + 1];
    if (max != target_frame_rate) {
      continue;
    }

    if (result_min != 0) {
      if (prefer_constant_frame_rate && min <= result_min) {
        // If we prefer constant frame rate, we prefer a smaller range.
        continue;
      } else if (!prefer_constant_frame_rate && min >= result_min) {
        // Othwewise, we prefer a larger range.
        continue;
      }
    }
    result_min = min;
    result_max = max;

    if (prefer_constant_frame_rate && min == max) {
      break;
    }
  }
  return std::make_pair(result_min, result_max);
}

// The result of max_width and max_height could be zero if the stream
// is not in the pre-defined configuration.
void GetStreamResolutions(const cros::mojom::CameraMetadataPtr& static_metadata,
                          cros::mojom::Camera3StreamType stream_type,
                          cros::mojom::HalPixelFormat stream_format,
                          std::vector<gfx::Size>* resolutions) {
  const cros::mojom::CameraMetadataEntryPtr* stream_configurations =
      GetMetadataEntry(static_metadata,
                       cros::mojom::CameraMetadataTag::
                           ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
  DCHECK(stream_configurations);
  // The available stream configurations are stored as tuples of four int32s:
  // (hal_pixel_format, width, height, type) x n
  const size_t kStreamFormatOffset = 0;
  const size_t kStreamWidthOffset = 1;
  const size_t kStreamHeightOffset = 2;
  const size_t kStreamTypeOffset = 3;
  const size_t kStreamConfigurationSize = 4;
  int32_t* iter =
      reinterpret_cast<int32_t*>((*stream_configurations)->data.data());
  for (size_t i = 0; i < (*stream_configurations)->count;
       i += kStreamConfigurationSize) {
    auto format =
        static_cast<cros::mojom::HalPixelFormat>(iter[kStreamFormatOffset]);
    int32_t width = iter[kStreamWidthOffset];
    int32_t height = iter[kStreamHeightOffset];
    auto type =
        static_cast<cros::mojom::Camera3StreamType>(iter[kStreamTypeOffset]);
    iter += kStreamConfigurationSize;

    if (type != stream_type || format != stream_format) {
      continue;
    }

    resolutions->emplace_back(width, height);
  }

  std::sort(resolutions->begin(), resolutions->end(),
            [](const gfx::Size& a, const gfx::Size& b) -> bool {
              return a.width() * a.height() < b.width() * b.height();
            });
}

// VideoCaptureDevice::TakePhotoCallback is given by the application and is used
// to return the captured JPEG blob buffer.  The second base::OnceClosure is
// created locally by the caller of TakePhoto(), and can be used to, for
// example, restore some settings to the values before TakePhoto() is called to
// facilitate the switch between photo and non-photo modes.
void TakePhotoCallbackBundle(VideoCaptureDevice::TakePhotoCallback callback,
                             base::OnceClosure on_photo_taken_callback,
                             mojom::BlobPtr blob) {
  std::move(callback).Run(std::move(blob));
  std::move(on_photo_taken_callback).Run();
}

void SetFpsRangeInMetadata(cros::mojom::CameraMetadataPtr* settings,
                           int32_t min_frame_rate,
                           int32_t max_frame_rate) {
  const int32_t entry_length = 2;

  // CameraMetadata is represented as an uint8 array. According to the
  // definition of the FPS metadata tag, its data type is int32, so we
  // reinterpret_cast here.
  std::vector<uint8_t> fps_range(sizeof(int32_t) * entry_length);
  auto* fps_ptr = reinterpret_cast<int32_t*>(fps_range.data());
  fps_ptr[0] = min_frame_rate;
  fps_ptr[1] = max_frame_rate;
  cros::mojom::CameraMetadataEntryPtr e =
      cros::mojom::CameraMetadataEntry::New();
  e->tag = cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_TARGET_FPS_RANGE;
  e->type = cros::mojom::EntryType::TYPE_INT32;
  e->count = entry_length;
  e->data = std::move(fps_range);

  AddOrUpdateMetadataEntry(settings, std::move(e));
}

}  // namespace

bool IsInputStream(StreamType stream_type) {
  // Currently there is only one input stream. Modify this method if there is
  // any other input streams.
  return stream_type == StreamType::kYUVInput;
}

StreamType StreamIdToStreamType(uint64_t stream_id) {
  switch (stream_id) {
    case 0:
      return StreamType::kPreviewOutput;
    case 1:
      return StreamType::kJpegOutput;
    case 2:
      return StreamType::kYUVInput;
    case 3:
      return StreamType::kYUVOutput;
    case 4:
      return StreamType::kRecordingOutput;
    default:
      return StreamType::kUnknown;
  }
}  // namespace media

std::string StreamTypeToString(StreamType stream_type) {
  switch (stream_type) {
    case StreamType::kPreviewOutput:
      return std::string("StreamType::kPreviewOutput");
    case StreamType::kJpegOutput:
      return std::string("StreamType::kJpegOutput");
    case StreamType::kYUVInput:
      return std::string("StreamType::kYUVInput");
    case StreamType::kYUVOutput:
      return std::string("StreamType::kYUVOutput");
    case StreamType::kRecordingOutput:
      return std::string("StreamType::kRecordingOutput");
    default:
      return std::string("Unknown StreamType value: ") +
             base::NumberToString(static_cast<int32_t>(stream_type));
  }
}  // namespace media

std::ostream& operator<<(std::ostream& os, StreamType stream_type) {
  return os << StreamTypeToString(stream_type);
}

StreamCaptureInterface::Plane::Plane() = default;

StreamCaptureInterface::Plane::~Plane() = default;

class CameraDeviceDelegate::StreamCaptureInterfaceImpl final
    : public StreamCaptureInterface {
 public:
  StreamCaptureInterfaceImpl(
      base::WeakPtr<CameraDeviceDelegate> camera_device_delegate)
      : camera_device_delegate_(std::move(camera_device_delegate)) {}

  void ProcessCaptureRequest(cros::mojom::Camera3CaptureRequestPtr request,
                             base::OnceCallback<void(int32_t)> callback) final {
    if (camera_device_delegate_) {
      camera_device_delegate_->ProcessCaptureRequest(std::move(request),
                                                     std::move(callback));
    }
  }

  void Flush(base::OnceCallback<void(int32_t)> callback) final {
    if (camera_device_delegate_) {
      camera_device_delegate_->Flush(std::move(callback));
    }
  }

 private:
  const base::WeakPtr<CameraDeviceDelegate> camera_device_delegate_;
};

ResultMetadata::ResultMetadata() = default;
ResultMetadata::~ResultMetadata() = default;

CameraDeviceDelegate::CameraDeviceDelegate(
    VideoCaptureDeviceDescriptor device_descriptor,
    scoped_refptr<CameraHalDelegate> camera_hal_delegate,
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner)
    : device_descriptor_(device_descriptor),
      camera_hal_delegate_(std::move(camera_hal_delegate)),
      ipc_task_runner_(std::move(ipc_task_runner)) {}

CameraDeviceDelegate::~CameraDeviceDelegate() = default;

void CameraDeviceDelegate::AllocateAndStart(
    const base::flat_map<ClientType, VideoCaptureParams>& params,
    CameraDeviceContext* device_context) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  result_metadata_frame_number_for_photo_state_ = 0;
  result_metadata_frame_number_ = 0;
  is_set_awb_mode_ = false;
  is_set_brightness_ = false;
  is_set_contrast_ = false;
  is_set_exposure_compensation_ = false;
  is_set_exposure_time_ = false;
  is_set_focus_distance_ = false;
  is_set_iso_ = false;
  is_set_pan_ = false;
  is_set_saturation_ = false;
  is_set_sharpness_ = false;
  is_set_tilt_ = false;
  is_set_zoom_ = false;

  chrome_capture_params_ = params;
  device_context_ = device_context;
  device_context_->SetState(CameraDeviceContext::State::kStarting);

  auto camera_app_device =
      CameraAppDeviceBridgeImpl::GetInstance()->GetWeakCameraAppDevice(
          device_descriptor_.device_id);
  if (camera_app_device) {
    camera_app_device->SetCameraDeviceContext(device_context_);
  }

  auto camera_info = camera_hal_delegate_->GetCameraInfoFromDeviceId(
      device_descriptor_.device_id);
  if (camera_info.is_null()) {
    device_context_->SetErrorState(
        media::VideoCaptureError::kCrosHalV3DeviceDelegateFailedToGetCameraInfo,
        FROM_HERE, "Failed to get camera info");
    return;
  }

  device_api_version_ = camera_info->device_version;
  SortCameraMetadata(&camera_info->static_camera_characteristics);
  static_metadata_ = std::move(camera_info->static_camera_characteristics);

  auto sensor_orientation = GetMetadataEntryAsSpan<int32_t>(
      static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_SENSOR_ORIENTATION);
  if (sensor_orientation.empty()) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3DeviceDelegateMissingSensorOrientationInfo,
        FROM_HERE, "Camera is missing required sensor orientation info");
    return;
  }
  auto rect = GetMetadataEntryAsSpan<int32_t>(
      static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
  active_array_size_ = gfx::Rect(rect[0], rect[1], rect[2], rect[3]);
  device_context_->SetSensorOrientation(sensor_orientation[0]);

  // |device_ops_| is bound after the BindNewPipeAndPassReceiver call.
  camera_hal_delegate_->OpenDevice(
      camera_hal_delegate_->GetCameraIdFromDeviceId(
          device_descriptor_.device_id),
      device_ops_.BindNewPipeAndPassReceiver(),
      BindToCurrentLoop(
          base::BindOnce(&CameraDeviceDelegate::OnOpenedDevice, GetWeakPtr())));
  device_ops_.set_disconnect_handler(base::BindOnce(
      &CameraDeviceDelegate::OnMojoConnectionError, GetWeakPtr()));
}

void CameraDeviceDelegate::StopAndDeAllocate(
    base::OnceClosure device_close_callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!device_context_ ||
      device_context_->GetState() == CameraDeviceContext::State::kStopped ||
      (device_context_->GetState() == CameraDeviceContext::State::kError &&
       !request_manager_)) {
    // In case of Mojo connection error the device may be stopped before
    // StopAndDeAllocate is called; in case of device open failure, the state
    // is set to kError and |request_manager_| is uninitialized.
    std::move(device_close_callback).Run();
    return;
  }

  // StopAndDeAllocate may be called at any state except
  // CameraDeviceContext::State::kStopping.
  DCHECK_NE(device_context_->GetState(), CameraDeviceContext::State::kStopping);

  auto camera_app_device =
      CameraAppDeviceBridgeImpl::GetInstance()->GetWeakCameraAppDevice(
          device_descriptor_.device_id);
  if (camera_app_device) {
    camera_app_device->SetCameraDeviceContext(nullptr);
  }

  device_close_callback_ = std::move(device_close_callback);
  device_context_->SetState(CameraDeviceContext::State::kStopping);
  if (device_ops_.is_bound()) {
    device_ops_->Close(
        base::BindOnce(&CameraDeviceDelegate::OnClosed, GetWeakPtr()));
  }
  // |request_manager_| might be still uninitialized if StopAndDeAllocate() is
  // called before the device is opened.
  if (request_manager_) {
    request_manager_->StopPreview(base::NullCallback());
  }
}

void CameraDeviceDelegate::TakePhoto(
    VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  take_photo_callbacks_.push(std::move(callback));

  if (!device_context_ ||
      (device_context_->GetState() !=
           CameraDeviceContext::State::kStreamConfigured &&
       device_context_->GetState() != CameraDeviceContext::State::kCapturing)) {
    return;
  }

  TakePhotoImpl();
}

void CameraDeviceDelegate::GetPhotoState(
    VideoCaptureDevice::GetPhotoStateCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (result_metadata_frame_number_ <=
      result_metadata_frame_number_for_photo_state_) {
    get_photo_state_queue_.push_back(
        base::BindOnce(&CameraDeviceDelegate::DoGetPhotoState,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  DoGetPhotoState(std::move(callback));
}

// On success, invokes |callback| with value |true|. On failure, drops
// callback without invoking it.
//
// https://www.w3.org/TR/mediacapture-streams/#dfn-applyconstraints-algorithm
// In a single operation, remove the existing constraints from object, apply
// newConstraints, and apply successfulSettings as the current settings.
void CameraDeviceDelegate::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() != CameraDeviceContext::State::kCapturing) {
    return;
  }

  bool ret = SetPointsOfInterest(settings->points_of_interest);
  if (!ret) {
    LOG(ERROR) << "Failed to set points of interest";
    return;
  }

  // Set the vendor tag into with given |name| and |value|. Returns true if
  // the vendor tag is set and false otherwise.
  auto to_uint8_vector = [](int32_t value) {
    std::vector<uint8_t> temp(sizeof(int32_t));
    auto* temp_ptr = reinterpret_cast<int32_t*>(temp.data());
    *temp_ptr = value;
    return temp;
  };

  auto set_vendor_int = [&](const std::string& name, bool has_field,
                            double value, bool is_set) {
    const VendorTagInfo* info =
        camera_hal_delegate_->GetVendorTagInfoByName(name);
    if (info == nullptr || !has_field) {
      if (is_set) {
        request_manager_->UnsetRepeatingCaptureMetadata(info->tag);
      }
      return false;
    }
    request_manager_->SetRepeatingCaptureMetadata(info->tag, info->type, 1,
                                                  to_uint8_vector(value));
    return true;
  };
  is_set_brightness_ = set_vendor_int(kBrightness, settings->has_brightness,
                                      settings->brightness, is_set_brightness_);
  is_set_contrast_ = set_vendor_int(kContrast, settings->has_contrast,
                                    settings->contrast, is_set_contrast_);
  is_set_pan_ =
      set_vendor_int(kPan, settings->has_pan, settings->pan, is_set_pan_);
  is_set_saturation_ = set_vendor_int(kSaturation, settings->has_saturation,
                                      settings->saturation, is_set_saturation_);
  is_set_sharpness_ = set_vendor_int(kSharpness, settings->has_sharpness,
                                     settings->sharpness, is_set_sharpness_);
  is_set_tilt_ =
      set_vendor_int(kTilt, settings->has_tilt, settings->tilt, is_set_tilt_);
  if (use_digital_zoom_) {
    if (settings->has_zoom && settings->zoom != 1) {
      double zoom_factor = sqrt(settings->zoom);
      int32_t crop_width = std::round(active_array_size_.width() / zoom_factor);
      int32_t crop_height =
          std::round(active_array_size_.height() / zoom_factor);
      // crop from center
      int32_t region[4] = {(active_array_size_.width() - crop_width) / 2,
                           (active_array_size_.height() - crop_height) / 2,
                           crop_width, crop_height};

      request_manager_->SetRepeatingCaptureMetadata(
          cros::mojom::CameraMetadataTag::ANDROID_SCALER_CROP_REGION,
          cros::mojom::EntryType::TYPE_INT32, 4,
          SerializeMetadataValueFromSpan(base::make_span(region, 4)));

      VLOG(1) << "zoom ratio:" << settings->zoom << " scaler.crop.region("
              << region[0] << "," << region[1] << "," << region[2] << ","
              << region[3] << ")";
      is_set_zoom_ = true;
    } else if (is_set_zoom_) {
      request_manager_->UnsetRepeatingCaptureMetadata(
          cros::mojom::CameraMetadataTag::ANDROID_SCALER_CROP_REGION);
      is_set_zoom_ = false;
    }
  } else {
    is_set_zoom_ =
        set_vendor_int(kZoom, settings->has_zoom, settings->zoom, is_set_zoom_);
  }

  if (settings->has_white_balance_mode &&
      settings->white_balance_mode == mojom::MeteringMode::MANUAL &&
      settings->has_color_temperature) {
    const AwbModeTemperatureMap& map = GetAwbModeTemperatureMap();
    cros::mojom::AndroidControlAwbMode awb_mode =
        cros::mojom::AndroidControlAwbMode::ANDROID_CONTROL_AWB_MODE_AUTO;
    auto awb_available_modes = GetMetadataEntryAsSpan<uint8_t>(
        static_metadata_,
        cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_AVAILABLE_MODES);
    int32_t min_diff = std::numeric_limits<int32_t>::max();
    for (const auto& mode : awb_available_modes) {
      auto it = map.find(mode);
      // Find the nearest awb mode
      int32_t diff = std::abs(settings->color_temperature - it->second);
      if (diff < min_diff) {
        awb_mode = static_cast<cros::mojom::AndroidControlAwbMode>(mode);
        min_diff = diff;
      }
    }
    camera_3a_controller_->SetAutoWhiteBalanceMode(awb_mode);
    is_set_awb_mode_ = true;
  } else if (is_set_awb_mode_) {
    camera_3a_controller_->SetAutoWhiteBalanceMode(
        cros::mojom::AndroidControlAwbMode::ANDROID_CONTROL_AWB_MODE_AUTO);
    is_set_awb_mode_ = false;
  }

  if (settings->has_exposure_mode &&
      settings->exposure_mode == mojom::MeteringMode::MANUAL &&
      settings->has_exposure_time) {
    int64_t exposure_time_nanoseconds_ =
        settings->exposure_time * 100 * kMicroToNano;
    camera_3a_controller_->SetExposureTime(false, exposure_time_nanoseconds_);
    is_set_exposure_time_ = true;
  } else if (is_set_exposure_time_) {
    camera_3a_controller_->SetExposureTime(true, 0);
    is_set_exposure_time_ = false;
  }

  if (settings->has_focus_mode &&
      settings->focus_mode == mojom::MeteringMode::MANUAL &&
      settings->has_focus_distance) {
    // The unit of settings is meter but it is diopter of android metadata.
    float focus_distance_diopters_ = 1.0 / settings->focus_distance;
    camera_3a_controller_->SetFocusDistance(false, focus_distance_diopters_);
    is_set_focus_distance_ = true;
  } else if (is_set_focus_distance_) {
    camera_3a_controller_->SetFocusDistance(true, 0);
    is_set_focus_distance_ = false;
  }

  if (settings->has_iso) {
    request_manager_->SetRepeatingCaptureMetadata(
        cros::mojom::CameraMetadataTag::ANDROID_SENSOR_SENSITIVITY,
        cros::mojom::EntryType::TYPE_INT32, 1, to_uint8_vector(settings->iso));
    is_set_iso_ = true;
    if (!is_set_exposure_time_) {
      LOG(WARNING) << "set iso doesn't work due to auto exposure time";
    }
  } else if (is_set_iso_) {
    request_manager_->UnsetRepeatingCaptureMetadata(
        cros::mojom::CameraMetadataTag::ANDROID_SENSOR_SENSITIVITY);
    is_set_iso_ = false;
  }

  if (settings->has_exposure_compensation) {
    int metadata_exposure_compensation =
        std::round(settings->exposure_compensation / ae_compensation_step_);
    request_manager_->SetRepeatingCaptureMetadata(
        cros::mojom::CameraMetadataTag::
            ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION,
        cros::mojom::EntryType::TYPE_INT32, 1,
        to_uint8_vector(metadata_exposure_compensation));
    is_set_exposure_compensation_ = true;
  } else if (is_set_exposure_compensation_) {
    request_manager_->UnsetRepeatingCaptureMetadata(
        cros::mojom::CameraMetadataTag::
            ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION);
    is_set_exposure_compensation_ = false;
  }

  // If there is callback of SetPhotoOptions(), the streams might being
  // reconfigured and we should notify them once the reconfiguration is done.
  auto on_reconfigured_callback = base::BindOnce(
      [](VideoCaptureDevice::SetPhotoOptionsCallback callback) {
        std::move(callback).Run(true);
      },
      std::move(callback));
  if (!on_reconfigured_callbacks_.empty()) {
    on_reconfigured_callbacks_.push(std::move(on_reconfigured_callback));
  } else {
    if (MaybeReconfigureForPhotoStream(std::move(settings))) {
      on_reconfigured_callbacks_.push(std::move(on_reconfigured_callback));
    } else {
      std::move(on_reconfigured_callback).Run();
    }
  }
  result_metadata_frame_number_for_photo_state_ = current_request_frame_number_;
}

void CameraDeviceDelegate::ReconfigureStreams(
    const base::flat_map<ClientType, VideoCaptureParams>& params) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  chrome_capture_params_ = params;
  if (request_manager_) {
    // ReconfigureStreams is used for video recording. It does not require
    // photo.
    request_manager_->StopPreview(base::BindOnce(
        &CameraDeviceDelegate::OnFlushed, GetWeakPtr(), false, absl::nullopt));
  }
}

void CameraDeviceDelegate::SetRotation(int rotation) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(rotation >= 0 && rotation < 360 && rotation % 90 == 0);
  device_context_->SetScreenRotation(rotation);
}

base::WeakPtr<CameraDeviceDelegate> CameraDeviceDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool CameraDeviceDelegate::MaybeReconfigureForPhotoStream(
    mojom::PhotoSettingsPtr settings) {
  bool is_resolution_specified = settings->has_width && settings->has_height;
  bool should_reconfigure_streams =
      (is_resolution_specified && (current_blob_resolution_.IsEmpty() ||
                                   current_blob_resolution_.width() !=
                                       static_cast<int32_t>(settings->width) ||
                                   current_blob_resolution_.height() !=
                                       static_cast<int32_t>(settings->height)));
  if (!should_reconfigure_streams) {
    return false;
  }

  if (is_resolution_specified) {
    gfx::Size new_blob_resolution(static_cast<int32_t>(settings->width),
                                  static_cast<int32_t>(settings->height));
    request_manager_->StopPreview(
        base::BindOnce(&CameraDeviceDelegate::OnFlushed, GetWeakPtr(), true,
                       std::move(new_blob_resolution)));
  } else {
    request_manager_->StopPreview(base::BindOnce(
        &CameraDeviceDelegate::OnFlushed, GetWeakPtr(), true, absl::nullopt));
  }
  return true;
}

void CameraDeviceDelegate::TakePhotoImpl() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  auto construct_request_cb =
      base::BindOnce(&CameraDeviceDelegate::ConstructDefaultRequestSettings,
                     GetWeakPtr(), StreamType::kJpegOutput);

  if (request_manager_->HasStreamsConfiguredForTakePhoto()) {
    camera_3a_controller_->Stabilize3AForStillCapture(
        std::move(construct_request_cb));
    return;
  }

  // Trigger the reconfigure process if it not yet triggered.
  if (on_reconfigured_callbacks_.empty()) {
    request_manager_->StopPreview(base::BindOnce(
        &CameraDeviceDelegate::OnFlushed, GetWeakPtr(), true, absl::nullopt));
  }
  auto on_reconfigured_callback = base::BindOnce(
      [](base::WeakPtr<Camera3AController> controller,
         base::OnceClosure callback) {
        controller->Stabilize3AForStillCapture(std::move(callback));
      },
      camera_3a_controller_->GetWeakPtr(), std::move(construct_request_cb));
  on_reconfigured_callbacks_.push(std::move(on_reconfigured_callback));
}

void CameraDeviceDelegate::OnMojoConnectionError() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() == CameraDeviceContext::State::kStopping) {
    // When in stopping state the camera HAL adapter may terminate the Mojo
    // channel before we do, in which case the OnClosed callback will not be
    // called.
    OnClosed(0);
  } else {
    // The Mojo channel terminated unexpectedly.
    if (request_manager_) {
      request_manager_->StopPreview(base::NullCallback());
    }
    device_context_->SetState(CameraDeviceContext::State::kStopped);
    device_context_->SetErrorState(
        media::VideoCaptureError::kCrosHalV3DeviceDelegateMojoConnectionError,
        FROM_HERE, "Mojo connection error");
    ResetMojoInterface();
    // We cannot reset |device_context_| here because
    // |device_context_->SetErrorState| above will call StopAndDeAllocate later
    // to handle the class destruction.
  }
}

void CameraDeviceDelegate::OnFlushed(
    bool require_photo,
    absl::optional<gfx::Size> new_blob_resolution,
    int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  if (result) {
    device_context_->SetErrorState(
        media::VideoCaptureError::kCrosHalV3DeviceDelegateFailedToFlush,
        FROM_HERE,
        std::string("Flush failed: ") + base::safe_strerror(-result));
    return;
  }
  device_context_->SetState(CameraDeviceContext::State::kInitialized);
  ConfigureStreams(require_photo, std::move(new_blob_resolution));
}

void CameraDeviceDelegate::OnClosed(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(device_context_->GetState(), CameraDeviceContext::State::kStopping);

  device_context_->SetState(CameraDeviceContext::State::kStopped);
  if (result) {
    device_context_->LogToClient(std::string("Failed to close device: ") +
                                 base::safe_strerror(-result));
  }
  if (request_manager_) {
    request_manager_->RemoveResultMetadataObserver(this);
  }
  ResetMojoInterface();
  device_context_ = nullptr;
  current_blob_resolution_.SetSize(0, 0);
  std::move(device_close_callback_).Run();
}

void CameraDeviceDelegate::ResetMojoInterface() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  device_ops_.reset();
  camera_3a_controller_.reset();
  request_manager_.reset();
}

void CameraDeviceDelegate::OnOpenedDevice(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_ == nullptr) {
    // The OnOpenedDevice() might be called after the camera device already
    // closed by StopAndDeAllocate(), because |camera_hal_delegate_| and
    // |device_ops_| are on different IPC channels. In such case,
    // OnMojoConnectionError() might call OnClosed() and the
    // |device_context_| is cleared, so we just return here.
    return;
  }

  if (device_context_->GetState() != CameraDeviceContext::State::kStarting) {
    if (device_context_->GetState() == CameraDeviceContext::State::kError) {
      // In case of camera open failed, the HAL can terminate the Mojo channel
      // before we do and set the state to kError in OnMojoConnectionError.
      return;
    }
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    OnClosed(0);
    return;
  }

  if (result) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3DeviceDelegateFailedToOpenCameraDevice,
        FROM_HERE, "Failed to open camera device");
    return;
  }
  Initialize();
}

void CameraDeviceDelegate::Initialize() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(device_context_->GetState(), CameraDeviceContext::State::kStarting);

  mojo::PendingRemote<cros::mojom::Camera3CallbackOps> callback_ops;
  // Assumes the buffer_type will be the same for all |chrome_capture_params|.
  request_manager_ = std::make_unique<RequestManager>(
      device_descriptor_.device_id,
      callback_ops.InitWithNewPipeAndPassReceiver(),
      std::make_unique<StreamCaptureInterfaceImpl>(GetWeakPtr()),
      device_context_,
      chrome_capture_params_[ClientType::kPreviewClient].buffer_type,
      std::make_unique<CameraBufferFactory>(),
      base::BindRepeating(&RotateAndBlobify), ipc_task_runner_,
      device_api_version_);
  camera_3a_controller_ = std::make_unique<Camera3AController>(
      static_metadata_, request_manager_.get(), ipc_task_runner_);
  device_ops_->Initialize(
      std::move(callback_ops),
      base::BindOnce(&CameraDeviceDelegate::OnInitialized, GetWeakPtr()));
  request_manager_->AddResultMetadataObserver(this);
}

void CameraDeviceDelegate::OnInitialized(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() != CameraDeviceContext::State::kStarting) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    return;
  }
  if (result) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice,
        FROM_HERE,
        std::string("Failed to initialize camera device: ") +
            base::safe_strerror(-result));
    return;
  }
  device_context_->SetState(CameraDeviceContext::State::kInitialized);
  bool require_photo = [&] {
    auto camera_app_device =
        CameraAppDeviceBridgeImpl::GetInstance()->GetWeakCameraAppDevice(
            device_descriptor_.device_id);
    if (!camera_app_device) {
      return false;
    }
    auto capture_intent = camera_app_device->GetCaptureIntent();
    switch (capture_intent) {
      case cros::mojom::CaptureIntent::DEFAULT:
        return false;
      case cros::mojom::CaptureIntent::STILL_CAPTURE:
        return true;
      case cros::mojom::CaptureIntent::VIDEO_RECORD:
        return false;
      case cros::mojom::CaptureIntent::DOCUMENT:
        return true;
      default:
        NOTREACHED() << "Unknown capture intent: " << capture_intent;
        return false;
    }
  }();
  ConfigureStreams(require_photo, absl::nullopt);
}

void CameraDeviceDelegate::ConfigureStreams(
    bool require_photo,
    absl::optional<gfx::Size> new_blob_resolution) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(device_context_->GetState(),
            CameraDeviceContext::State::kInitialized);

  cros::mojom::Camera3StreamConfigurationPtr stream_config =
      cros::mojom::Camera3StreamConfiguration::New();
  for (const auto& param : chrome_capture_params_) {
    // Set up context for preview stream and record stream.
    cros::mojom::Camera3StreamPtr stream = cros::mojom::Camera3Stream::New();
    StreamType stream_type = (param.first == ClientType::kPreviewClient)
                                 ? StreamType::kPreviewOutput
                                 : StreamType::kRecordingOutput;
    // TODO(henryhsu): PreviewClient should remove HW_VIDEO_ENCODER usage when
    // multiple streams enabled.
    auto usage = (param.first == ClientType::kPreviewClient)
                     ? (cros::mojom::GRALLOC_USAGE_HW_COMPOSER |
                        cros::mojom::GRALLOC_USAGE_HW_VIDEO_ENCODER)
                     : cros::mojom::GRALLOC_USAGE_HW_VIDEO_ENCODER;
    stream->id = static_cast<uint64_t>(stream_type);
    stream->stream_type = cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT;
    stream->width = param.second.requested_format.frame_size.width();
    stream->height = param.second.requested_format.frame_size.height();
    stream->format =
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888;
    stream->usage = usage;
    stream->data_space = 0;
    stream->rotation =
        cros::mojom::Camera3StreamRotation::CAMERA3_STREAM_ROTATION_0;
    if (device_api_version_ >= cros::mojom::CAMERA_DEVICE_API_VERSION_3_5) {
      stream->physical_camera_id = "";
    }

    stream_config->streams.push_back(std::move(stream));
  }

  // Set up context for still capture stream. We set still capture stream to the
  // JPEG stream configuration with maximum supported resolution.
  int32_t blob_width = 0;
  int32_t blob_height = 0;
  if (require_photo) {
    auto blob_resolution = GetBlobResolution(new_blob_resolution);
    if (blob_resolution.IsEmpty()) {
      LOG(ERROR) << "Failed to configure streans: No BLOB resolution found.";
      return;
    }

    blob_width = blob_resolution.width();
    blob_height = blob_resolution.height();

    cros::mojom::Camera3StreamPtr still_capture_stream =
        cros::mojom::Camera3Stream::New();
    still_capture_stream->id = static_cast<uint64_t>(StreamType::kJpegOutput);
    still_capture_stream->stream_type =
        cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT;
    still_capture_stream->width = blob_width;
    still_capture_stream->height = blob_height;
    still_capture_stream->format =
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB;
    // Set usage flag to allow HAL adapter to identify a still capture stream.
    still_capture_stream->usage = cros::mojom::GRALLOC_USAGE_STILL_CAPTURE;
    still_capture_stream->data_space = 0;
    still_capture_stream->rotation =
        cros::mojom::Camera3StreamRotation::CAMERA3_STREAM_ROTATION_0;
    if (device_api_version_ >= cros::mojom::CAMERA_DEVICE_API_VERSION_3_5) {
      still_capture_stream->physical_camera_id = "";
    }
    stream_config->streams.push_back(std::move(still_capture_stream));

    int32_t max_yuv_width = 0, max_yuv_height = 0;
    if (IsYUVReprocessingSupported(&max_yuv_width, &max_yuv_height)) {
      auto reprocessing_stream_input = cros::mojom::Camera3Stream::New();
      reprocessing_stream_input->id =
          static_cast<uint64_t>(StreamType::kYUVInput);
      reprocessing_stream_input->stream_type =
          cros::mojom::Camera3StreamType::CAMERA3_STREAM_INPUT;
      reprocessing_stream_input->width = max_yuv_width;
      reprocessing_stream_input->height = max_yuv_height;
      reprocessing_stream_input->format =
          cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888;
      reprocessing_stream_input->data_space = 0;
      reprocessing_stream_input->rotation =
          cros::mojom::Camera3StreamRotation::CAMERA3_STREAM_ROTATION_0;
      if (device_api_version_ >= cros::mojom::CAMERA_DEVICE_API_VERSION_3_5) {
        reprocessing_stream_input->physical_camera_id = "";
      }

      auto reprocessing_stream_output = cros::mojom::Camera3Stream::New();
      reprocessing_stream_output->id =
          static_cast<uint64_t>(StreamType::kYUVOutput);
      reprocessing_stream_output->stream_type =
          cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT;
      reprocessing_stream_output->width = max_yuv_width;
      reprocessing_stream_output->height = max_yuv_height;
      reprocessing_stream_output->format =
          cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888;
      // Set usage flag to allow HAL adapter to identify a still capture stream.
      reprocessing_stream_output->usage =
          cros::mojom::GRALLOC_USAGE_STILL_CAPTURE;
      reprocessing_stream_output->data_space = 0;
      reprocessing_stream_output->rotation =
          cros::mojom::Camera3StreamRotation::CAMERA3_STREAM_ROTATION_0;
      if (device_api_version_ >= cros::mojom::CAMERA_DEVICE_API_VERSION_3_5) {
        reprocessing_stream_output->physical_camera_id = "";
      }

      stream_config->streams.push_back(std::move(reprocessing_stream_input));
      stream_config->streams.push_back(std::move(reprocessing_stream_output));
    }
  }

  stream_config->operation_mode = cros::mojom::Camera3StreamConfigurationMode::
      CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE;
  if (device_api_version_ >= cros::mojom::CAMERA_DEVICE_API_VERSION_3_5) {
    stream_config->session_parameters = cros::mojom::CameraMetadata::New();
  }
  device_ops_->ConfigureStreams(
      std::move(stream_config),
      base::BindOnce(&CameraDeviceDelegate::OnConfiguredStreams, GetWeakPtr(),
                     gfx::Size(blob_width, blob_height)));
}

void CameraDeviceDelegate::OnConfiguredStreams(
    gfx::Size blob_resolution,
    int32_t result,
    cros::mojom::Camera3StreamConfigurationPtr updated_config) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() != CameraDeviceContext::State::kInitialized) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    return;
  }
  if (result) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3DeviceDelegateFailedToConfigureStreams,
        FROM_HERE,
        std::string("Failed to configure streams: ") +
            base::safe_strerror(-result));
    return;
  }
  if (!updated_config ||
      updated_config->streams.size() > kMaxConfiguredStreams ||
      updated_config->streams.size() < kMinConfiguredStreams) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3DeviceDelegateWrongNumberOfStreamsConfigured,
        FROM_HERE,
        std::string("Wrong number of streams configured: ") +
            base::NumberToString(updated_config->streams.size()));
    return;
  }

  current_blob_resolution_.SetSize(blob_resolution.width(),
                                   blob_resolution.height());

  request_manager_->SetUpStreamsAndBuffers(chrome_capture_params_,
                                           static_metadata_,
                                           std::move(updated_config->streams));

  device_context_->SetState(CameraDeviceContext::State::kStreamConfigured);
  // Kick off the preview stream.
  ConstructDefaultRequestSettings(StreamType::kPreviewOutput);
}

bool CameraDeviceDelegate::IsYUVReprocessingSupported(int* max_width,
                                                      int* max_height) {
  bool has_yuv_reprocessing_capability = [&] {
    auto capabilities = GetMetadataEntryAsSpan<uint8_t>(
        static_metadata_,
        cros::mojom::CameraMetadataTag::ANDROID_REQUEST_AVAILABLE_CAPABILITIES);
    auto capability_yuv_reprocessing = static_cast<uint8_t>(
        cros::mojom::AndroidRequestAvailableCapabilities::
            ANDROID_REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING);
    for (auto capability : capabilities) {
      if (capability == capability_yuv_reprocessing) {
        return true;
      }
    }
    return false;
  }();

  if (!has_yuv_reprocessing_capability) {
    return false;
  }

  bool has_yuv_input_blob_output = [&] {
    auto formats_map = GetMetadataEntryAsSpan<int32_t>(
        static_metadata_,
        cros::mojom::CameraMetadataTag::
            ANDROID_SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP);
    // The formats map looks like: [
    //   {INPUT_FORMAT, NUM_OF_OUTPUTS, OUTPUT_FORMAT_1, OUTPUT_FORMAT_2, ...},
    //   {...},
    //   ...
    // ]
    auto format_yuv = static_cast<int32_t>(
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888);
    auto format_blob = static_cast<int32_t>(
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB);

    size_t idx = 0;
    while (idx < formats_map.size()) {
      auto in_format = formats_map[idx++];
      auto out_amount = formats_map[idx++];
      if (in_format != format_yuv) {
        idx += out_amount;
        continue;
      }
      for (size_t idx_end = idx + out_amount; idx < idx_end; idx++) {
        auto out_format = formats_map[idx];
        if (out_format == format_blob) {
          return true;
        }
      }
    }
    return false;
  }();

  if (!has_yuv_input_blob_output) {
    return false;
  }

  std::vector<gfx::Size> yuv_resolutions;
  GetStreamResolutions(
      static_metadata_, cros::mojom::Camera3StreamType::CAMERA3_STREAM_INPUT,
      cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888,
      &yuv_resolutions);
  if (yuv_resolutions.empty()) {
    return false;
  }
  *max_width = yuv_resolutions.back().width();
  *max_height = yuv_resolutions.back().height();
  return true;
}

void CameraDeviceDelegate::ConstructDefaultRequestSettings(
    StreamType stream_type) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(device_context_->GetState() ==
             CameraDeviceContext::State::kStreamConfigured ||
         device_context_->GetState() == CameraDeviceContext::State::kCapturing);

  if (stream_type == StreamType::kPreviewOutput) {
    // CCA uses the same stream for preview and video recording. Choose proper
    // template here so the underlying camera HAL can set 3A tuning accordingly.
    auto camera_app_device =
        CameraAppDeviceBridgeImpl::GetInstance()->GetWeakCameraAppDevice(
            device_descriptor_.device_id);
    auto request_template =
        camera_app_device && camera_app_device->GetCaptureIntent() ==
                                 cros::mojom::CaptureIntent::VIDEO_RECORD
            ? cros::mojom::Camera3RequestTemplate::CAMERA3_TEMPLATE_VIDEO_RECORD
            : cros::mojom::Camera3RequestTemplate::CAMERA3_TEMPLATE_PREVIEW;
    device_ops_->ConstructDefaultRequestSettings(
        request_template,
        base::BindOnce(
            &CameraDeviceDelegate::OnConstructedDefaultPreviewRequestSettings,
            GetWeakPtr()));
  } else if (stream_type == StreamType::kJpegOutput) {
    device_ops_->ConstructDefaultRequestSettings(
        cros::mojom::Camera3RequestTemplate::CAMERA3_TEMPLATE_STILL_CAPTURE,
        base::BindOnce(&CameraDeviceDelegate::
                           OnConstructedDefaultStillCaptureRequestSettings,
                       GetWeakPtr()));
  } else {
    NOTREACHED() << "No default request settings for stream: " << stream_type;
  }
}

void CameraDeviceDelegate::OnConstructedDefaultPreviewRequestSettings(
    cros::mojom::CameraMetadataPtr settings) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() !=
      CameraDeviceContext::State::kStreamConfigured) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    return;
  }
  if (!settings) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3DeviceDelegateFailedToGetDefaultRequestSettings,
        FROM_HERE, "Failed to get default request settings");
    return;
  }

  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  device_context_->SetState(CameraDeviceContext::State::kCapturing);
  camera_3a_controller_->SetAutoFocusModeForStillCapture();

  auto camera_app_device =
      CameraAppDeviceBridgeImpl::GetInstance()->GetWeakCameraAppDevice(
          device_descriptor_.device_id);
  auto specified_fps_range =
      camera_app_device ? camera_app_device->GetFpsRange() : absl::nullopt;
  if (specified_fps_range) {
    SetFpsRangeInMetadata(&settings, specified_fps_range->GetMin(),
                          specified_fps_range->GetMax());
  } else {
    // Assumes the frame_rate will be the same for all |chrome_capture_params|.
    int32_t requested_frame_rate =
        std::round(chrome_capture_params_[ClientType::kPreviewClient]
                       .requested_format.frame_rate);
    bool prefer_constant_frame_rate =
        base::FeatureList::IsEnabled(
            chromeos::features::kPreferConstantFrameRate) ||
        (camera_app_device && camera_app_device->GetCaptureIntent() ==
                                  cros::mojom::CaptureIntent::VIDEO_RECORD);
    int32_t target_min, target_max;
    std::tie(target_min, target_max) = GetTargetFrameRateRange(
        static_metadata_, requested_frame_rate, prefer_constant_frame_rate);
    if (target_min == 0 || target_max == 0) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3DeviceDelegateFailedToGetDefaultRequestSettings,
          FROM_HERE, "Failed to get valid frame rate range");
      return;
    }

    SetFpsRangeInMetadata(&settings, target_min, target_max);
  }
  while (!on_reconfigured_callbacks_.empty()) {
    std::move(on_reconfigured_callbacks_.front()).Run();
    on_reconfigured_callbacks_.pop();
  }

  request_manager_->StartPreview(std::move(settings));
  if (!take_photo_callbacks_.empty()) {
    TakePhotoImpl();
  }
}

void CameraDeviceDelegate::OnConstructedDefaultStillCaptureRequestSettings(
    cros::mojom::CameraMetadataPtr settings) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  auto camera_app_device =
      CameraAppDeviceBridgeImpl::GetInstance()->GetWeakCameraAppDevice(
          device_descriptor_.device_id);

  while (!take_photo_callbacks_.empty()) {
    auto take_photo_callback = base::BindOnce(
        &TakePhotoCallbackBundle, std::move(take_photo_callbacks_.front()),
        base::BindOnce(&Camera3AController::SetAutoFocusModeForStillCapture,
                       camera_3a_controller_->GetWeakPtr()));
    if (camera_app_device) {
      camera_app_device->ConsumeReprocessOptions(
          std::move(take_photo_callback),
          media::BindToCurrentLoop(base::BindOnce(
              &RequestManager::TakePhoto, request_manager_->GetWeakPtr(),
              settings.Clone())));
    } else {
      request_manager_->TakePhoto(
          settings.Clone(), CameraAppDeviceImpl::GetSingleShotReprocessOptions(
                                std::move(take_photo_callback)));
    }
    take_photo_callbacks_.pop();
  }
}

gfx::Size CameraDeviceDelegate::GetBlobResolution(
    absl::optional<gfx::Size> new_blob_resolution) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  std::vector<gfx::Size> blob_resolutions;
  GetStreamResolutions(
      static_metadata_, cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT,
      cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB, &blob_resolutions);
  if (blob_resolutions.empty()) {
    return {};
  }

  // Try the given blob resolution first. If it is invalid, try the
  // resolution specified through Mojo API as a fallback. If it fails too,
  // use the largest resolution as default.
  if (new_blob_resolution.has_value() &&
      base::Contains(blob_resolutions, *new_blob_resolution)) {
    return *new_blob_resolution;
  }

  auto camera_app_device =
      CameraAppDeviceBridgeImpl::GetInstance()->GetWeakCameraAppDevice(
          device_descriptor_.device_id);
  if (camera_app_device) {
    auto specified_capture_resolution =
        camera_app_device->GetStillCaptureResolution();
    if (!specified_capture_resolution.IsEmpty() &&
        base::Contains(blob_resolutions, specified_capture_resolution)) {
      return specified_capture_resolution;
    }
  }
  return blob_resolutions.back();
}

void CameraDeviceDelegate::ProcessCaptureRequest(
    cros::mojom::Camera3CaptureRequestPtr request,
    base::OnceCallback<void(int32_t)> callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  for (const auto& output_buffer : request->output_buffers) {
    TRACE_EVENT2("camera", "Capture Request", "frame_number",
                 request->frame_number, "stream_id", output_buffer->stream_id);
  }
  current_request_frame_number_ = request->frame_number;

  if (device_context_->GetState() != CameraDeviceContext::State::kCapturing) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    return;
  }
  device_ops_->ProcessCaptureRequest(std::move(request), std::move(callback));
}

void CameraDeviceDelegate::Flush(base::OnceCallback<void(int32_t)> callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  device_ops_->Flush(std::move(callback));
}

bool CameraDeviceDelegate::SetPointsOfInterest(
    const std::vector<mojom::Point2DPtr>& points_of_interest) {
  if (points_of_interest.empty()) {
    return true;
  }

  if (!camera_3a_controller_->IsPointOfInterestSupported()) {
    LOG(WARNING) << "Point of interest is not supported on this device.";
    return false;
  }

  if (points_of_interest.size() > 1) {
    LOG(WARNING) << "Setting more than one point of interest is not "
                    "supported, only the first one is used.";
  }

  // A Point2D Point of Interest is interpreted to represent a pixel position in
  // a normalized square space (|{x,y} ∈ [0.0, 1.0]|). The origin of coordinates
  // |{x,y} = {0.0, 0.0}| represents the upper leftmost corner whereas the
  // |{x,y} = {1.0, 1.0}| represents the lower rightmost corner: the x
  // coordinate (columns) increases rightwards and the y coordinate (rows)
  // increases downwards. Values beyond the mentioned limits will be clamped to
  // the closest allowed value.
  // ref: https://www.w3.org/TR/image-capture/#points-of-interest

  double x = base::clamp(points_of_interest[0]->x, 0.0, 1.0);
  double y = base::clamp(points_of_interest[0]->y, 0.0, 1.0);

  // Handle rotation, still in normalized square space.
  std::tie(x, y) = [&]() -> std::pair<double, double> {
    switch (device_context_->GetCameraFrameRotation()) {
      case 0:
        return {x, y};
      case 90:
        return {y, 1.0 - x};
      case 180:
        return {1.0 - x, 1.0 - y};
      case 270:
        return {1.0 - y, x};
      default:
        NOTREACHED() << "Invalid orientation";
    }
    return {x, y};
  }();

  // TODO(shik): Respect to SCALER_CROP_REGION, which is unused now.
  x *= active_array_size_.width() - 1;
  y *= active_array_size_.height() - 1;
  gfx::Point point = {static_cast<int>(x), static_cast<int>(y)};
  camera_3a_controller_->SetPointOfInterest(point);
  return true;
}

mojom::RangePtr CameraDeviceDelegate::GetControlRangeByVendorTagName(
    const std::string& range_name,
    const absl::optional<int32_t>& current) {
  const VendorTagInfo* info =
      camera_hal_delegate_->GetVendorTagInfoByName(range_name);
  if (info == nullptr) {
    return mojom::Range::New();
  }
  auto static_val =
      GetMetadataEntryAsSpan<int32_t>(static_metadata_, info->tag);
  if (static_val.size() != 3) {
    return mojom::Range::New();
  }

  if (!current) {
    return mojom::Range::New();
  }

  mojom::RangePtr range = mojom::Range::New();

  range->min = static_val[0];
  range->max = static_val[1];
  range->step = static_val[2];
  range->current = current.value();

  return range;
}

void CameraDeviceDelegate::OnResultMetadataAvailable(
    uint32_t frame_number,
    const cros::mojom::CameraMetadataPtr& result_metadata) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  auto get_vendor_int =
      [&](const std::string& name,
          const cros::mojom::CameraMetadataPtr& result_metadata,
          absl::optional<int32_t>* returned_value) {
        returned_value->reset();
        const VendorTagInfo* info =
            camera_hal_delegate_->GetVendorTagInfoByName(name);
        if (info == nullptr)
          return;
        auto val = GetMetadataEntryAsSpan<int32_t>(result_metadata, info->tag);
        if (val.size() == 1)
          *returned_value = val[0];
      };

  get_vendor_int(kBrightness, result_metadata, &result_metadata_.brightness);
  get_vendor_int(kContrast, result_metadata, &result_metadata_.contrast);
  get_vendor_int(kPan, result_metadata, &result_metadata_.pan);
  get_vendor_int(kSaturation, result_metadata, &result_metadata_.saturation);
  get_vendor_int(kSharpness, result_metadata, &result_metadata_.sharpness);
  get_vendor_int(kTilt, result_metadata, &result_metadata_.tilt);
  get_vendor_int(kZoom, result_metadata, &result_metadata_.zoom);

  result_metadata_.scaler_crop_region.reset();
  auto rect = GetMetadataEntryAsSpan<int32_t>(
      result_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_SCALER_CROP_REGION);
  if (rect.size() == 4) {
    result_metadata_.scaler_crop_region =
        gfx::Rect(rect[0], rect[1], rect[2], rect[3]);
  }

  result_metadata_.ae_mode.reset();
  auto ae_mode = GetMetadataEntryAsSpan<uint8_t>(
      result_metadata, cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_MODE);
  if (ae_mode.size() == 1)
    result_metadata_.ae_mode = ae_mode[0];

  result_metadata_.exposure_time.reset();
  auto exposure_time = GetMetadataEntryAsSpan<int64_t>(
      result_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_SENSOR_EXPOSURE_TIME);
  if (exposure_time.size() == 1)
    result_metadata_.exposure_time = exposure_time[0];

  result_metadata_.awb_mode.reset();
  auto awb_mode = GetMetadataEntryAsSpan<uint8_t>(
      result_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_MODE);
  if (awb_mode.size() == 1)
    result_metadata_.awb_mode = awb_mode[0];

  result_metadata_.af_mode.reset();
  auto af_mode = GetMetadataEntryAsSpan<uint8_t>(
      result_metadata, cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_MODE);
  if (af_mode.size() == 1)
    result_metadata_.af_mode = af_mode[0];

  result_metadata_.focus_distance.reset();
  auto focus_distance = GetMetadataEntryAsSpan<float>(
      result_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_LENS_FOCUS_DISTANCE);
  if (focus_distance.size() == 1)
    result_metadata_.focus_distance = focus_distance[0];

  result_metadata_.sensitivity.reset();
  auto sensitivity = GetMetadataEntryAsSpan<int32_t>(
      result_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_SENSOR_SENSITIVITY);
  if (sensitivity.size() == 1)
    result_metadata_.sensitivity = sensitivity[0];

  result_metadata_.ae_compensation.reset();
  auto ae_compensation = GetMetadataEntryAsSpan<int32_t>(
      result_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION);
  if (ae_compensation.size() == 1)
    result_metadata_.ae_compensation = ae_compensation[0];

  auto lens_state = GetMetadataEntryAsSpan<uint8_t>(
      result_metadata, cros::mojom::CameraMetadataTag::ANDROID_LENS_STATE);
  result_metadata_frame_number_ = frame_number;
  // We need to wait the new result metadata for new settings.
  if (result_metadata_frame_number_ >
          result_metadata_frame_number_for_photo_state_ &&
      lens_state.size() == 1 &&
      static_cast<cros::mojom::AndroidLensState>(lens_state[0]) ==
          cros::mojom::AndroidLensState::ANDROID_LENS_STATE_STATIONARY) {
    for (auto& request : get_photo_state_queue_)
      ipc_task_runner_->PostTask(FROM_HERE, std::move(request));
    get_photo_state_queue_.clear();
  }
}

void CameraDeviceDelegate::DoGetPhotoState(
    VideoCaptureDevice::GetPhotoStateCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  auto photo_state = mojo::CreateEmptyPhotoState();

  if (!device_context_ ||
      (device_context_->GetState() !=
           CameraDeviceContext::State::kStreamConfigured &&
       device_context_->GetState() != CameraDeviceContext::State::kCapturing)) {
    std::move(callback).Run(std::move(photo_state));
    return;
  }

  std::vector<gfx::Size> blob_resolutions;
  GetStreamResolutions(
      static_metadata_, cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT,
      cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB, &blob_resolutions);
  if (blob_resolutions.empty()) {
    std::move(callback).Run(std::move(photo_state));
    return;
  }

  // Sets the correct range of min/max resolution in order to bypass checks that
  // the resolution caller request should fall within the range when taking
  // photos. And since we are not actually use the mechanism to get other
  // resolutions, we set the step to 0.0 here.
  photo_state->width->current = current_blob_resolution_.width();
  photo_state->width->min = blob_resolutions.front().width();
  photo_state->width->max = blob_resolutions.back().width();
  photo_state->width->step = 0.0;
  photo_state->height->current = current_blob_resolution_.height();
  photo_state->height->min = blob_resolutions.front().height();
  photo_state->height->max = blob_resolutions.back().height();
  photo_state->height->step = 0.0;

  photo_state->brightness = GetControlRangeByVendorTagName(
      kBrightnessRange, result_metadata_.brightness);
  photo_state->contrast =
      GetControlRangeByVendorTagName(kContrastRange, result_metadata_.contrast);
  photo_state->pan =
      GetControlRangeByVendorTagName(kPanRange, result_metadata_.pan);
  photo_state->saturation = GetControlRangeByVendorTagName(
      kSaturationRange, result_metadata_.saturation);
  photo_state->sharpness = GetControlRangeByVendorTagName(
      kSharpnessRange, result_metadata_.sharpness);
  photo_state->tilt =
      GetControlRangeByVendorTagName(kTiltRange, result_metadata_.tilt);

  // For zoom part, we check the scaler.availableMaxDigitalZoom first, if there
  // is no metadata or the value is one we use zoom vendor tag.
  //
  // https://w3c.github.io/mediacapture-image/#zoom
  //
  // scaler.availableMaxDigitalZoom:
  // We use area ratio for this type zoom.
  //
  // Vendor tag zoom:
  // It is used by UVC camera usually.
  // The zoom unit is driver-specific for V4L2_CID_ZOOM_ABSOLUTE.
  // https://www.kernel.org/doc/html/latest/media/uapi/v4l/ext-ctrls-camera.html
  auto max_digital_zoom = GetMetadataEntryAsSpan<float>(
      static_metadata_, cros::mojom::CameraMetadataTag::
                            ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM);
  if (max_digital_zoom.size() == 1 && max_digital_zoom[0] > 1 &&
      result_metadata_.scaler_crop_region) {
    photo_state->zoom->min = 1;
    photo_state->zoom->max = max_digital_zoom[0] * max_digital_zoom[0];
    photo_state->zoom->step = 0.1;
    photo_state->zoom->current =
        (active_array_size_.width() /
         (float)result_metadata_.scaler_crop_region->width()) *
        (active_array_size_.height() /
         (float)result_metadata_.scaler_crop_region->height());
    // get 0.1 precision
    photo_state->zoom->current = round(photo_state->zoom->current * 10) / 10;
    use_digital_zoom_ = true;
  } else {
    photo_state->zoom =
        GetControlRangeByVendorTagName(kZoomRange, result_metadata_.zoom);
    use_digital_zoom_ = false;
  }

  auto awb_available_modes = GetMetadataEntryAsSpan<uint8_t>(
      static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AWB_AVAILABLE_MODES);

  // Only enable white balance control when there are more than 1 control modes.
  if (awb_available_modes.size() > 1 && result_metadata_.awb_mode) {
    photo_state->supported_white_balance_modes.push_back(
        mojom::MeteringMode::MANUAL);
    photo_state->supported_white_balance_modes.push_back(
        mojom::MeteringMode::CONTINUOUS);
    const AwbModeTemperatureMap& map = GetAwbModeTemperatureMap();
    int32_t current_temperature = 0;
    if (result_metadata_.awb_mode ==
        static_cast<uint8_t>(
            cros::mojom::AndroidControlAwbMode::ANDROID_CONTROL_AWB_MODE_AUTO))
      photo_state->current_white_balance_mode = mojom::MeteringMode::CONTINUOUS;
    else {
      // Need to find current color temperature.
      photo_state->current_white_balance_mode = mojom::MeteringMode::MANUAL;
      current_temperature = map.at(result_metadata_.awb_mode.value());
    }

    int32_t min = std::numeric_limits<int32_t>::max();
    int32_t max = std::numeric_limits<int32_t>::min();
    for (const auto& mode : awb_available_modes) {
      auto it = map.find(mode);
      if (it == map.end())
        continue;
      if (it->second < min)
        min = it->second;
      else if (it->second > max)
        max = it->second;
    }
    photo_state->color_temperature->min = min;
    photo_state->color_temperature->max = max;
    photo_state->color_temperature->step = kColorTemperatureStep;
    photo_state->color_temperature->current = current_temperature;
  }

  auto ae_available_modes = GetMetadataEntryAsSpan<uint8_t>(
      static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_AVAILABLE_MODES);
  bool support_manual_exposure_time = false;
  if (ae_available_modes.size() > 1 && result_metadata_.ae_mode) {
    support_manual_exposure_time = base::Contains(
        ae_available_modes,
        static_cast<uint8_t>(
            cros::mojom::AndroidControlAeMode::ANDROID_CONTROL_AE_MODE_OFF));
  }

  auto exposure_time_range = GetMetadataEntryAsSpan<int64_t>(
      static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE);

  if (support_manual_exposure_time && exposure_time_range.size() == 2 &&
      result_metadata_.exposure_time) {
    photo_state->supported_exposure_modes.push_back(
        mojom::MeteringMode::MANUAL);
    photo_state->supported_exposure_modes.push_back(
        mojom::MeteringMode::CONTINUOUS);
    if (result_metadata_.ae_mode ==
        static_cast<uint8_t>(
            cros::mojom::AndroidControlAeMode::ANDROID_CONTROL_AE_MODE_OFF))
      photo_state->current_exposure_mode = mojom::MeteringMode::MANUAL;
    else
      photo_state->current_exposure_mode = mojom::MeteringMode::CONTINUOUS;

    // The unit of photo_state->exposure_time is 100 microseconds and from
    // metadata is nanoseconds.
    photo_state->exposure_time->min = std::ceil(
        static_cast<float>(exposure_time_range[0]) / (100 * kMicroToNano));
    photo_state->exposure_time->max =
        exposure_time_range[1] / (100 * kMicroToNano);
    photo_state->exposure_time->step = 1;  // 100 microseconds
    photo_state->exposure_time->current =
        result_metadata_.exposure_time.value() / (100 * kMicroToNano);
  }

  auto af_available_modes = GetMetadataEntryAsSpan<uint8_t>(
      static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AF_AVAILABLE_MODES);
  bool support_manual_focus_distance = false;
  if (af_available_modes.size() > 1 && result_metadata_.af_mode) {
    support_manual_focus_distance = base::Contains(
        af_available_modes,
        static_cast<uint8_t>(
            cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_OFF));
  }

  auto minimum_focus_distance = GetMetadataEntryAsSpan<float>(
      static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE);
  // If the lens is fixed-focus, minimum_focus_distance will be 0.
  if (support_manual_focus_distance && minimum_focus_distance.size() == 1 &&
      minimum_focus_distance[0] != 0 && result_metadata_.focus_distance) {
    photo_state->supported_focus_modes.push_back(mojom::MeteringMode::MANUAL);
    photo_state->supported_focus_modes.push_back(
        mojom::MeteringMode::CONTINUOUS);
    if (result_metadata_.af_mode ==
        static_cast<uint8_t>(
            cros::mojom::AndroidControlAfMode::ANDROID_CONTROL_AF_MODE_OFF))
      photo_state->current_focus_mode = mojom::MeteringMode::MANUAL;
    else
      photo_state->current_focus_mode = mojom::MeteringMode::CONTINUOUS;

    // The unit of photo_state->focus_distance is meter and from metadata is
    // diopter.
    photo_state->focus_distance->min =
        std::roundf(100.0 / minimum_focus_distance[0]) / 100.0;
    photo_state->focus_distance->max = std::numeric_limits<double>::infinity();
    photo_state->focus_distance->step = 0.01;
    if (result_metadata_.focus_distance.value() == 0) {
      photo_state->focus_distance->current =
          std::numeric_limits<double>::infinity();
    } else {
      // We want to make sure |current| is a possible value of
      // |min| + |steps(0.01)|*X.  The minimum can be divided by step(0.01). So
      // we only need to round the value less than 0.01.
      double meters = 1.0 / result_metadata_.focus_distance.value();
      photo_state->focus_distance->current = std::roundf(meters * 100) / 100.0;
    }
  }

  auto sensitivity_range = GetMetadataEntryAsSpan<int32_t>(
      static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_SENSOR_INFO_SENSITIVITY_RANGE);
  if (sensitivity_range.size() == 2 && result_metadata_.sensitivity) {
    photo_state->iso->min = sensitivity_range[0];
    photo_state->iso->max = sensitivity_range[1];
    photo_state->iso->step = 1;
    photo_state->iso->current = result_metadata_.sensitivity.value();
  }

  auto ae_compensation_range = GetMetadataEntryAsSpan<int32_t>(
      static_metadata_,
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_COMPENSATION_RANGE);
  ae_compensation_step_ = 0.0;
  if (ae_compensation_range.size() == 2) {
    if (ae_compensation_range[0] != 0 || ae_compensation_range[1] != 0) {
      auto ae_compensation_step = GetMetadataEntryAsSpan<Rational>(
          static_metadata_,
          cros::mojom::CameraMetadataTag::ANDROID_CONTROL_AE_COMPENSATION_STEP);
      if (ae_compensation_step.size() == 1) {
        if (ae_compensation_step[0].numerator == 0 ||
            ae_compensation_step[0].denominator == 0) {
          LOG(WARNING) << "AE_COMPENSATION_STEP: numerator:"
                       << ae_compensation_step[0].numerator << ", denominator:"
                       << ae_compensation_step[0].denominator;
        } else {
          ae_compensation_step_ =
              static_cast<float>(ae_compensation_step[0].numerator) /
              static_cast<float>(ae_compensation_step[0].denominator);
          photo_state->exposure_compensation->min =
              ae_compensation_range[0] * ae_compensation_step_;
          photo_state->exposure_compensation->max =
              ae_compensation_range[1] * ae_compensation_step_;
          photo_state->exposure_compensation->step = ae_compensation_step_;
          if (result_metadata_.ae_compensation)
            photo_state->exposure_compensation->current =
                result_metadata_.ae_compensation.value() *
                ae_compensation_step_;
          else
            photo_state->exposure_compensation->current = 0;
        }
      }
    }
  }

  std::move(callback).Run(std::move(photo_state));
}

}  // namespace media
