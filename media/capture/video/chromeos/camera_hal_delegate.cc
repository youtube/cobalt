// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_hal_delegate.h"

#include <fcntl.h>
#include <sys/uio.h>

#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/posix/safe_strerror.h"
#include "base/process/launch.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/system_monitor.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/device_event_log/device_event_log.h"
#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "media/capture/video/chromeos/video_capture_device_chromeos_delegate.h"
#include "media/capture/video/chromeos/video_capture_device_chromeos_halv3.h"

namespace media {

namespace {

constexpr int32_t kDefaultFps = 30;
constexpr char kVirtualPrefix[] = "VIRTUAL_";

const std::unordered_set<int32_t> module_id_set = {
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kLifeCamHD3000_Microsoft),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kC270_Logitech),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kHDC615_Logitech),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kHDProC920_Logitech),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kC930e_Logitech),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kC925e_Logitech),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kC922ProStream_Logitech),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kBRIOUltraHD_Logitech),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kC920HDPro_Logitech),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kC920PROHD_Logitech),
    static_cast<int32_t>(CameraHalDelegate::PopularCamPeriphModuleID::kCam_ARC),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kLiveStreamer313_Sunplus),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kVitadeAF_Microdia),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kCam_Sonix),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kVZR_IPEVO),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::k808Camera9_Generalplus),
    static_cast<int32_t>(
        CameraHalDelegate::PopularCamPeriphModuleID::kNexiGoN60FHD_2MUVC),
};

constexpr base::TimeDelta kEventWaitTimeoutSecs = base::Seconds(1);

class LocalCameraClientObserver : public CameraClientObserver {
 public:
  LocalCameraClientObserver() = delete;

  explicit LocalCameraClientObserver(CameraHalDelegate* camera_hal_delegate,
                                     cros::mojom::CameraClientType type,
                                     base::UnguessableToken auth_token)
      : CameraClientObserver(type, std::move(auth_token)),
        camera_hal_delegate_(camera_hal_delegate) {}

  LocalCameraClientObserver(const LocalCameraClientObserver&) = delete;
  LocalCameraClientObserver& operator=(const LocalCameraClientObserver&) =
      delete;

  void OnChannelCreated(
      mojo::PendingRemote<cros::mojom::CameraModule> camera_module) override {
    camera_hal_delegate_->SetCameraModule(std::move(camera_module));
  }

 private:
  raw_ptr<CameraHalDelegate, ExperimentalAsh> camera_hal_delegate_;
};

// ash::system::StatisticsProvider::IsRunningOnVM() isn't available in unittest.
bool IsRunningOnVM() {
  static bool is_vm = []() {
    std::string output;
    if (!base::GetAppOutput({"crossystem", "inside_vm"}, &output)) {
      return false;
    }
    return output == "1";
  }();
  return is_vm;
}

bool IsVividLoaded() {
  std::string output;
  if (!base::GetAppOutput({"lsmod"}, &output)) {
    return false;
  }

  std::vector<base::StringPiece> lines = base::SplitStringPieceUsingSubstr(
      output, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  return base::ranges::any_of(lines, [](const auto& line) {
    return base::StartsWith(line, "vivid", base::CompareCase::SENSITIVE);
  });
}

void NotifyVideoCaptureDevicesChanged() {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  // |monitor| might be nullptr in unittest.
  if (monitor) {
    monitor->ProcessDevicesChanged(
        base::SystemMonitor::DeviceType::DEVTYPE_VIDEO_CAPTURE);
  }
}

base::flat_set<int32_t> GetAvailableFramerates(
    const cros::mojom::CameraInfoPtr& camera_info) {
  base::flat_set<int32_t> candidates;
  auto available_fps_ranges = GetMetadataEntryAsSpan<int32_t>(
      camera_info->static_camera_characteristics,
      cros::mojom::CameraMetadataTag::
          ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
  if (available_fps_ranges.empty()) {
    // If there is no available target fps ranges listed in metadata, we set a
    // default fps as candidate.
    LOG(WARNING) << "No available fps ranges in metadata. Set default fps as "
                    "candidate.";
    candidates.insert(kDefaultFps);
    return candidates;
  }

  // The available target fps ranges are stored as pairs int32s: (min, max) x n.
  const size_t kRangeMaxOffset = 1;
  const size_t kRangeSize = 2;

  for (size_t i = 0; i < available_fps_ranges.size(); i += kRangeSize) {
    candidates.insert(available_fps_ranges[i + kRangeMaxOffset]);
  }
  return candidates;
}

}  // namespace

class CameraHalDelegate::PowerManagerClientProxy
    : public chromeos::PowerManagerClient::Observer {
 public:
  PowerManagerClientProxy() = default;
  PowerManagerClientProxy(const PowerManagerClientProxy&) = delete;
  PowerManagerClientProxy& operator=(const PowerManagerClientProxy&) = delete;

  void Init(scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
    ui_task_runner_ = std::move(ui_task_runner);
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PowerManagerClientProxy::InitOnDBusThread,
                                  GetWeakPtr()));
  }

  void Shutdown() {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PowerManagerClientProxy::ShutdownOnDBusThread,
                       GetWeakPtr()));
  }

  chromeos::PowerManagerClient::LidState GetLidState() { return lid_state_; }

 private:
  void InitOnDBusThread() {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    chromeos::PowerManagerClient* power_manager_client =
        chromeos::PowerManagerClient::Get();
    // power_manager_client may be NULL in unittests.
    if (power_manager_client)
      power_manager_client->AddObserver(this);
  }

  void ShutdownOnDBusThread() {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    chromeos::PowerManagerClient* power_manager_client =
        chromeos::PowerManagerClient::Get();
    // power_manager_client may be NULL in unittests.
    if (power_manager_client)
      power_manager_client->RemoveObserver(this);
  }

  // chromeos::PowerManagerClient::Observer:
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) final {
    if (lid_state_ != state) {
      lid_state_ = state;
      NotifyVideoCaptureDevicesChanged();
    }
  }

  base::WeakPtr<CameraHalDelegate::PowerManagerClientProxy> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  chromeos::PowerManagerClient::LidState lid_state_ =
      chromeos::PowerManagerClient::LidState::OPEN;

  base::WeakPtrFactory<CameraHalDelegate::PowerManagerClientProxy>
      weak_ptr_factory_{this};
};

class CameraHalDelegate::VideoCaptureDeviceDelegateMap {
 public:
  VideoCaptureDeviceDelegateMap() = default;
  VideoCaptureDeviceDelegateMap(const VideoCaptureDeviceDelegateMap&) = delete;
  VideoCaptureDeviceDelegateMap& operator=(
      const VideoCaptureDeviceDelegateMap&) = delete;
  ~VideoCaptureDeviceDelegateMap() = default;

  bool HasVCDDelegate(int camera_id) {
    return vcd_delegates_.find(camera_id) != vcd_delegates_.end();
  }

  void Insert(const int camera_id,
              std::unique_ptr<VideoCaptureDeviceChromeOSDelegate> delegate) {
    vcd_delegates_[camera_id] = std::move(delegate);
  }

  void Erase(const int camera_id) { vcd_delegates_.erase(camera_id); }

  VideoCaptureDeviceChromeOSDelegate* Get(const int camera_id) {
    DCHECK(HasVCDDelegate(camera_id));
    return vcd_delegates_[camera_id].get();
  }

  base::WeakPtr<CameraHalDelegate::VideoCaptureDeviceDelegateMap> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::flat_map<int, std::unique_ptr<VideoCaptureDeviceChromeOSDelegate>>
      vcd_delegates_;
  base::WeakPtrFactory<CameraHalDelegate::VideoCaptureDeviceDelegateMap>
      weak_ptr_factory_{this};
};

CameraHalDelegate::CameraHalDelegate(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : authenticated_(false),
      camera_module_has_been_set_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      builtin_camera_info_updated_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      external_camera_info_updated_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::SIGNALED),
      has_camera_connected_(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED),
      num_builtin_cameras_(0),
      camera_buffer_factory_(new CameraBufferFactory()),
      camera_hal_ipc_thread_("CameraHalIpcThread"),
      camera_module_callbacks_(this),
      vcd_delegate_map_(new VideoCaptureDeviceDelegateMap()),
      power_manager_client_proxy_(new PowerManagerClientProxy()),
      ui_task_runner_(std::move(ui_task_runner)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  power_manager_client_proxy_->Init(ui_task_runner_);
}

bool CameraHalDelegate::Init() {
  if (!camera_hal_ipc_thread_.Start()) {
    LOG(ERROR) << "CameraHalDelegate IPC thread failed to start";
    return false;
  }
  ipc_task_runner_ = camera_hal_ipc_thread_.task_runner();
  vendor_tag_ops_delegate_ =
      std::make_unique<VendorTagOpsDelegate>(ipc_task_runner_);
  return true;
}

CameraHalDelegate::~CameraHalDelegate() {
  camera_hal_ipc_thread_.Stop();
  power_manager_client_proxy_->Shutdown();
  ui_task_runner_->DeleteSoon(FROM_HERE,
                              std::move(power_manager_client_proxy_));

  if (vcd_task_runner_) {
    vcd_task_runner_->DeleteSoon(FROM_HERE, std::move(vcd_delegate_map_));
  }
}

bool CameraHalDelegate::RegisterCameraClient() {
  auto* dispatcher = CameraHalDispatcherImpl::GetInstance();
  auto type = cros::mojom::CameraClientType::CHROME;
  auto client_observer = std::make_unique<LocalCameraClientObserver>(
      this, type, dispatcher->GetTokenForTrustedClient(type));
  dispatcher->AddClientObserver(
      client_observer.get(),
      base::BindOnce(&CameraHalDelegate::OnRegisteredCameraHalClient,
                     base::Unretained(this)));
  camera_hal_client_registered_.Wait();
  local_client_observers_.emplace_back(std::move(client_observer));
  return authenticated_;
}

void CameraHalDelegate::OnRegisteredCameraHalClient(int32_t result) {
  if (result != 0) {
    LOG(ERROR) << "Failed to register camera HAL client";
    camera_hal_client_registered_.Signal();
    return;
  }
  CAMERA_LOG(EVENT) << "Registered camera HAL client";
  authenticated_ = true;
  camera_hal_client_registered_.Signal();
}

void CameraHalDelegate::SetCameraModule(
    mojo::PendingRemote<cros::mojom::CameraModule> camera_module) {
  ipc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDelegate::SetCameraModuleOnIpcThread,
                     base::Unretained(this), std::move(camera_module)));
}

void CameraHalDelegate::Reset() {
  if (ipc_task_runner_) {
    ipc_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CameraHalDelegate::ResetMojoInterfaceOnIpcThread,
                       base::Unretained(this)));
  }
  std::vector<CameraClientObserver*> observers;
  for (auto& client_observer : local_client_observers_) {
    observers.emplace_back(client_observer.get());
  }
  auto* dispatcher = CameraHalDispatcherImpl::GetInstance();
  dispatcher->RemoveClientObservers(observers);
  local_client_observers_.clear();
}

std::unique_ptr<VideoCaptureDevice> CameraHalDelegate::CreateDevice(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_screen_observer,
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!UpdateBuiltInCameraInfo()) {
    return nullptr;
  }
  int camera_id = GetCameraIdFromDeviceId(device_descriptor.device_id);
  if (camera_id == -1) {
    LOG(ERROR) << "Invalid camera device: " << device_descriptor.device_id;
    return nullptr;
  }

  auto* delegate =
      GetVCDDelegate(task_runner_for_screen_observer, device_descriptor);
  return std::make_unique<VideoCaptureDeviceChromeOSHalv3>(delegate,
                                                           device_descriptor);
}

void CameraHalDelegate::GetSupportedFormats(
    const cros::mojom::CameraInfoPtr& camera_info,
    VideoCaptureFormats* supported_formats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::flat_set<int32_t> candidate_fps_set =
      GetAvailableFramerates(camera_info);

  const cros::mojom::CameraMetadataEntryPtr* min_frame_durations =
      GetMetadataEntry(camera_info->static_camera_characteristics,
                       cros::mojom::CameraMetadataTag::
                           ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS);
  if (!min_frame_durations) {
    LOG(ERROR)
        << "Failed to get available min frame durations from camera info";
    return;
  }
  // The min frame durations are stored as tuples of four int64s:
  // (hal_pixel_format, width, height, ns) x n
  const size_t kStreamFormatOffset = 0;
  const size_t kStreamWidthOffset = 1;
  const size_t kStreamHeightOffset = 2;
  const size_t kStreamDurationOffset = 3;
  const size_t kStreamDurationSize = 4;
  int64_t* iter =
      reinterpret_cast<int64_t*>((*min_frame_durations)->data.data());
  for (size_t i = 0; i < (*min_frame_durations)->count;
       i += kStreamDurationSize) {
    auto hal_format =
        static_cast<cros::mojom::HalPixelFormat>(iter[kStreamFormatOffset]);
    int32_t width = base::checked_cast<int32_t>(iter[kStreamWidthOffset]);
    int32_t height = base::checked_cast<int32_t>(iter[kStreamHeightOffset]);
    int64_t duration = iter[kStreamDurationOffset];
    iter += kStreamDurationSize;

    if (hal_format == cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB) {
      // Skip BLOB formats and use it only for TakePicture() since it's
      // inefficient to stream JPEG frames for CrOS camera HAL.
      continue;
    }

    if (duration <= 0) {
      LOG(ERROR) << "Ignoring invalid frame duration: " << duration;
      continue;
    }
    float max_fps = 1.0 * 1000000000LL / duration;

    // There's no consumer information here to determine the buffer usage, so
    // hard-code the usage that all the clients should be using.
    constexpr gfx::BufferUsage kClientBufferUsage =
        gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE;
    const ChromiumPixelFormat cr_format =
        camera_buffer_factory_->ResolveStreamBufferFormat(hal_format,
                                                          kClientBufferUsage);
    if (cr_format.video_format == PIXEL_FORMAT_UNKNOWN) {
      continue;
    }

    for (auto fps : candidate_fps_set) {
      if (fps > max_fps) {
        continue;
      }

      CAMERA_LOG(DEBUG) << "Supported format: " << width << "x" << height
                        << " fps=" << fps
                        << " format=" << cr_format.video_format;
      supported_formats->emplace_back(gfx::Size(width, height), fps,
                                      cr_format.video_format);
    }
  }
}

void CameraHalDelegate::GetDevicesInfo(
    VideoCaptureDeviceFactory::GetDevicesInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!UpdateBuiltInCameraInfo()) {
    std::move(callback).Run({});
    return;
  }

  if (!external_camera_info_updated_.TimedWait(kEventWaitTimeoutSecs)) {
    LOG(ERROR) << "Failed to get camera info from all external cameras";
  }

  if (IsRunningOnVM() && IsVividLoaded()) {
    has_camera_connected_.TimedWait(kEventWaitTimeoutSecs);
  }

  std::vector<VideoCaptureDeviceInfo> devices_info;

  {
    base::AutoLock info_lock(camera_info_lock_);
    base::AutoLock id_map_lock(device_id_to_camera_id_lock_);
    base::AutoLock virtual_lock(enable_virtual_device_lock_);
    for (const auto& it : camera_info_) {
      int camera_id = it.first;
      const cros::mojom::CameraInfoPtr& camera_info = it.second;
      if (!camera_info) {
        continue;
      }

      auto get_vendor_string = [&](const std::string& key) -> const char* {
        const VendorTagInfo* info =
            vendor_tag_ops_delegate_->GetInfoByName(key);
        if (info == nullptr) {
          return nullptr;
        }
        auto val = GetMetadataEntryAsSpan<char>(
            camera_info->static_camera_characteristics, info->tag);
        return val.empty() ? nullptr : val.data();
      };

      VideoCaptureDeviceDescriptor desc;
      desc.capture_api = VideoCaptureApi::ANDROID_API2_LIMITED;
      desc.transport_type = VideoCaptureTransportType::OTHER_TRANSPORT;
      switch (camera_info->facing) {
        case cros::mojom::CameraFacing::CAMERA_FACING_BACK:
          desc.facing = VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT;
          desc.device_id = base::NumberToString(camera_id);
          desc.set_display_name("Back Camera");
          break;
        case cros::mojom::CameraFacing::CAMERA_FACING_FRONT:
          desc.facing = VideoFacingMode::MEDIA_VIDEO_FACING_USER;
          desc.device_id = base::NumberToString(camera_id);
          desc.set_display_name("Front Camera");
          break;
        case cros::mojom::CameraFacing::CAMERA_FACING_EXTERNAL: {
          desc.facing = VideoFacingMode::MEDIA_VIDEO_FACING_NONE;

          // The webcam_private api expects that |device_id| to be set as the
          // corresponding device path for external cameras used in GVC system.
          auto* path = get_vendor_string("com.google.usb.devicePath");
          desc.device_id =
              path != nullptr ? path : base::NumberToString(camera_id);

          auto* name = get_vendor_string("com.google.usb.modelName");
          desc.set_display_name(name != nullptr ? name : "External Camera");

          break;
          // Mojo validates the input parameters for us so we don't need to
          // worry about malformed values.
        }
        case cros::mojom::CameraFacing::CAMERA_FACING_VIRTUAL_BACK:
        case cros::mojom::CameraFacing::CAMERA_FACING_VIRTUAL_FRONT:
        case cros::mojom::CameraFacing::CAMERA_FACING_VIRTUAL_EXTERNAL:
          // |camera_info_| should not have these facing types.
          LOG(ERROR) << "Invalid facing type: " << camera_info->facing;
          break;
      }
      auto* vid = get_vendor_string("com.google.usb.vendorId");
      auto* pid = get_vendor_string("com.google.usb.productId");
      if (vid != nullptr && pid != nullptr) {
        desc.model_id = base::StrCat({vid, ":", pid});
      }
      desc.set_control_support(GetControlSupport(camera_info));
      device_id_to_camera_id_[desc.device_id] = camera_id;
      auto* dispatcher = CameraHalDispatcherImpl::GetInstance();
      dispatcher->AddCameraIdToDeviceIdEntry(camera_id, desc.device_id);
      devices_info.emplace_back(desc);
      GetSupportedFormats(camera_info_[camera_id],
                          &devices_info.back().supported_formats);

      // Create a virtual device when multiple streams are enabled.
      if (enable_virtual_device_[camera_id]) {
        desc.facing = VideoFacingMode::MEDIA_VIDEO_FACING_NONE;
        desc.device_id =
            std::string(kVirtualPrefix) + base::NumberToString(camera_id);
        desc.set_display_name("Virtual Camera");
        device_id_to_camera_id_[desc.device_id] = camera_id;
        dispatcher->AddCameraIdToDeviceIdEntry(camera_id, desc.device_id);
        devices_info.emplace_back(desc);
        GetSupportedFormats(camera_info_[camera_id],
                            &devices_info.back().supported_formats);
      }
    }
  }
  // TODO(jcliang): Remove this after JS API supports query camera facing
  // (http://crbug.com/543997).
  std::sort(
      devices_info.begin(), devices_info.end(),
      [&](const VideoCaptureDeviceInfo& a, const VideoCaptureDeviceInfo& b) {
        auto IsExternalCamera = [](const VideoCaptureDeviceInfo& vcd_info) {
          return vcd_info.descriptor.facing ==
                 VideoFacingMode::MEDIA_VIDEO_FACING_NONE;
        };
        chromeos::PowerManagerClient::LidState lid_state =
            power_manager_client_proxy_->GetLidState();
        if (lid_state == chromeos::PowerManagerClient::LidState::CLOSED) {
          if (IsExternalCamera(a) == IsExternalCamera(b))
            return a.descriptor < b.descriptor;
          return IsExternalCamera(a);
        }
        return a.descriptor < b.descriptor;
      });
  DVLOG(1) << "Number of devices: " << devices_info.size();

  std::move(callback).Run(std::move(devices_info));
}

VideoCaptureControlSupport CameraHalDelegate::GetControlSupport(
    const cros::mojom::CameraInfoPtr& camera_info) {
  VideoCaptureControlSupport control_support;

  auto is_vendor_range_valid = [&](const std::string& key) -> bool {
    const VendorTagInfo* info = vendor_tag_ops_delegate_->GetInfoByName(key);
    if (info == nullptr)
      return false;
    auto range = GetMetadataEntryAsSpan<int32_t>(
        camera_info->static_camera_characteristics, info->tag);
    return range.size() == 3 && range[0] < range[1];
  };

  if (is_vendor_range_valid("com.google.control.panRange"))
    control_support.pan = true;

  if (is_vendor_range_valid("com.google.control.tiltRange"))
    control_support.tilt = true;

  if (is_vendor_range_valid("com.google.control.zoomRange"))
    control_support.zoom = true;

  auto max_digital_zoom = GetMetadataEntryAsSpan<float>(
      camera_info->static_camera_characteristics,
      cros::mojom::CameraMetadataTag::
          ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM);
  if (max_digital_zoom.size() == 1 && max_digital_zoom[0] > 1) {
    control_support.zoom = true;
  }

  return control_support;
}

cros::mojom::CameraInfoPtr CameraHalDelegate::GetCameraInfoFromDeviceId(
    const std::string& device_id) {
  base::AutoLock lock(camera_info_lock_);
  int camera_id = GetCameraIdFromDeviceId(device_id);
  if (camera_id == -1) {
    return {};
  }
  auto it = camera_info_.find(camera_id);
  if (it == camera_info_.end()) {
    return {};
  }
  auto info = it->second.Clone();
  if (base::StartsWith(device_id, std::string(kVirtualPrefix))) {
    switch (it->second->facing) {
      case cros::mojom::CameraFacing::CAMERA_FACING_BACK:
        info->facing = cros::mojom::CameraFacing::CAMERA_FACING_VIRTUAL_BACK;
        break;
      case cros::mojom::CameraFacing::CAMERA_FACING_FRONT:
        info->facing = cros::mojom::CameraFacing::CAMERA_FACING_VIRTUAL_FRONT;
        break;
      case cros::mojom::CameraFacing::CAMERA_FACING_EXTERNAL:
        info->facing =
            cros::mojom::CameraFacing::CAMERA_FACING_VIRTUAL_EXTERNAL;
        break;
      default:
        break;
    }
  }
  return info;
}

void CameraHalDelegate::EnableVirtualDevice(const std::string& device_id,
                                            bool enable) {
  if (base::StartsWith(device_id, std::string(kVirtualPrefix))) {
    return;
  }
  auto camera_id = GetCameraIdFromDeviceId(device_id);
  if (camera_id != -1) {
    base::AutoLock lock(enable_virtual_device_lock_);
    enable_virtual_device_[camera_id] = enable;
  }
}

void CameraHalDelegate::DisableAllVirtualDevices() {
  base::AutoLock lock(enable_virtual_device_lock_);
  for (auto& it : enable_virtual_device_) {
    it.second = false;
  }
}

const VendorTagInfo* CameraHalDelegate::GetVendorTagInfoByName(
    const std::string& full_name) {
  return vendor_tag_ops_delegate_->GetInfoByName(full_name);
}

void CameraHalDelegate::OpenDevice(
    int32_t camera_id,
    const std::string& module_id,
    mojo::PendingReceiver<cros::mojom::Camera3DeviceOps> device_ops_receiver,
    OpenDeviceCallback callback) {
  DCHECK(!ipc_task_runner_->BelongsToCurrentThread());
  // This method may be called on any thread except |ipc_task_runner_|.
  // Currently this method is used by CameraDeviceDelegate to open a camera
  // device.
  camera_module_has_been_set_.Wait();
  ipc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDelegate::OpenDeviceOnIpcThread,
                     base::Unretained(this), camera_id, module_id,
                     std::move(device_ops_receiver), std::move(callback)));
}

int CameraHalDelegate::GetCameraIdFromDeviceId(const std::string& device_id) {
  base::AutoLock lock(device_id_to_camera_id_lock_);
  auto it = device_id_to_camera_id_.find(device_id);
  if (it == device_id_to_camera_id_.end()) {
    return -1;
  }
  return it->second;
}

VideoCaptureDeviceChromeOSDelegate* CameraHalDelegate::GetVCDDelegate(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_screen_observer,
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!vcd_task_runner_) {
    vcd_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  }
  auto camera_id = GetCameraIdFromDeviceId(device_descriptor.device_id);
  if (!vcd_delegate_map_->HasVCDDelegate(camera_id) ||
      vcd_delegate_map_->Get(camera_id)->HasDeviceClient() == 0) {
    auto cleanup_callback = base::BindPostTaskToCurrentDefault(
        base::BindOnce(&VideoCaptureDeviceDelegateMap::Erase,
                       vcd_delegate_map_->GetWeakPtr(), camera_id));
    auto delegate = std::make_unique<VideoCaptureDeviceChromeOSDelegate>(
        std::move(task_runner_for_screen_observer), device_descriptor, this,
        std::move(cleanup_callback));
    vcd_delegate_map_->Insert(camera_id, std::move(delegate));
  }
  return vcd_delegate_map_->Get(camera_id);
}

void CameraHalDelegate::SetCameraModuleOnIpcThread(
    mojo::PendingRemote<cros::mojom::CameraModule> camera_module) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  if (camera_module_.is_bound()) {
    LOG(ERROR) << "CameraModule is already bound";
    return;
  }
  if (!camera_module.is_valid()) {
    LOG(ERROR) << "Invalid pending camera module remote";
    return;
  }
  camera_module_.Bind(std::move(camera_module));
  camera_module_.set_disconnect_handler(
      base::BindOnce(&CameraHalDelegate::ResetMojoInterfaceOnIpcThread,
                     base::Unretained(this)));
  camera_module_has_been_set_.Signal();

  // Trigger ondevicechange event to notify clients that built-in camera device
  // info can now be queried.
  NotifyVideoCaptureDevicesChanged();
}

void CameraHalDelegate::ResetMojoInterfaceOnIpcThread() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  camera_module_.reset();
  camera_module_callbacks_.reset();
  vendor_tag_ops_delegate_->Reset();
  builtin_camera_info_updated_.Reset();
  camera_module_has_been_set_.Reset();
  has_camera_connected_.Reset();
  external_camera_info_updated_.Signal();

  // Clear all cached camera info, especially external cameras.
  base::AutoLock lock(camera_info_lock_);
  camera_info_.clear();
  pending_external_camera_info_.clear();
  NotifyVideoCaptureDevicesChanged();
}

bool CameraHalDelegate::UpdateBuiltInCameraInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!ipc_task_runner_->BelongsToCurrentThread());

  if (!camera_module_has_been_set_.TimedWait(kEventWaitTimeoutSecs)) {
    LOG(ERROR) << "Camera module not set; platform camera service might not be "
                  "ready yet";
    return false;
  }
  if (builtin_camera_info_updated_.IsSignaled()) {
    return true;
  }
  // The built-in camera are static per specification of the Android camera HAL
  // v3 specification.  We only update the built-in camera info once.
  ipc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDelegate::UpdateBuiltInCameraInfoOnIpcThread,
                     base::Unretained(this)));
  if (!builtin_camera_info_updated_.TimedWait(kEventWaitTimeoutSecs)) {
    LOG(ERROR) << "Timed out getting camera info";
    return false;
  }
  return true;
}

void CameraHalDelegate::UpdateBuiltInCameraInfoOnIpcThread() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  camera_module_->GetNumberOfCameras(
      base::BindOnce(&CameraHalDelegate::OnGotNumberOfCamerasOnIpcThread,
                     base::Unretained(this)));
}

void CameraHalDelegate::OnGotNumberOfCamerasOnIpcThread(int32_t num_cameras) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(camera_info_lock_);
  if (num_cameras < 0) {
    builtin_camera_info_updated_.Signal();
    LOG(ERROR) << "Failed to get number of cameras: " << num_cameras;
    return;
  }
  CAMERA_LOG(EVENT) << "Number of built-in cameras: " << num_cameras;
  num_builtin_cameras_ = num_cameras;
  // Per camera HAL v3 specification SetCallbacks() should be called after the
  // first time GetNumberOfCameras() is called, and before other CameraModule
  // functions are called.
  camera_module_->SetCallbacksAssociated(
      camera_module_callbacks_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&CameraHalDelegate::OnSetCallbacksOnIpcThread,
                     base::Unretained(this)));

  camera_module_->GetVendorTagOps(
      vendor_tag_ops_delegate_->MakeReceiver(),
      base::BindOnce(&CameraHalDelegate::OnGotVendorTagOpsOnIpcThread,
                     base::Unretained(this)));
}

void CameraHalDelegate::OnSetCallbacksOnIpcThread(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(camera_info_lock_);
  if (result) {
    num_builtin_cameras_ = 0;
    builtin_camera_info_updated_.Signal();
    LOG(ERROR) << "Failed to set camera module callbacks: "
               << base::safe_strerror(-result);
    return;
  }

  if (num_builtin_cameras_ == 0) {
    builtin_camera_info_updated_.Signal();
    return;
  }

  for (size_t camera_id = 0; camera_id < num_builtin_cameras_; ++camera_id) {
    GetCameraInfoOnIpcThread(
        camera_id,
        base::BindOnce(&CameraHalDelegate::OnGotCameraInfoOnIpcThread,
                       base::Unretained(this), camera_id));
  }
}

void CameraHalDelegate::OnGotVendorTagOpsOnIpcThread() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  vendor_tag_ops_delegate_->Initialize();
}

void CameraHalDelegate::GetCameraInfoOnIpcThread(
    int32_t camera_id,
    GetCameraInfoCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  camera_module_->GetCameraInfo(camera_id, std::move(callback));
}

void CameraHalDelegate::OnGotCameraInfoOnIpcThread(
    int32_t camera_id,
    int32_t result,
    cros::mojom::CameraInfoPtr camera_info) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "Got camera info of camera " << camera_id;
  if (result) {
    LOG(ERROR) << "Failed to get camera info. Camera id: " << camera_id;
    return;
  }
  SortCameraMetadata(&camera_info->static_camera_characteristics);

  base::AutoLock lock(camera_info_lock_);
  camera_info_[camera_id] = std::move(camera_info);

  if (camera_id < base::checked_cast<int32_t>(num_builtin_cameras_)) {
    // |camera_info_| might contain some entries for external cameras as well,
    // we should check all built-in cameras explicitly.
    bool all_updated = [&]() {
      camera_info_lock_.AssertAcquired();
      for (size_t i = 0; i < num_builtin_cameras_; i++) {
        if (camera_info_.find(i) == camera_info_.end()) {
          return false;
        }
      }
      return true;
    }();

    if (all_updated) {
      builtin_camera_info_updated_.Signal();
    }
  } else {
    // It's an external camera.
    pending_external_camera_info_.erase(camera_id);
    if (pending_external_camera_info_.empty()) {
      external_camera_info_updated_.Signal();
    }
    NotifyVideoCaptureDevicesChanged();
  }

  if (camera_info_.size() == 1) {
    has_camera_connected_.Signal();
  }
}

int32_t CameraHalDelegate::GetMaskedModuleID(const std::string module_id) {
  if (module_id.size() == 9) {
    int vid = strtol(module_id.substr(0, 4).c_str(), nullptr, 16);
    int pid = strtol(module_id.substr(5, 8).c_str(), nullptr, 16);
    int decimal_module_id = (vid << 16) + pid;
    if (base::Contains(module_id_set, decimal_module_id)) {
      return decimal_module_id;
    }
  }
  return static_cast<int32_t>(PopularCamPeriphModuleID::kOthers);
}

void CameraHalDelegate::OpenDeviceOnIpcThread(
    int32_t camera_id,
    const std::string& module_id, /* such as abcd:1234, 8 digits hex string */
    mojo::PendingReceiver<cros::mojom::Camera3DeviceOps> device_ops_receiver,
    OpenDeviceCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  base::UmaHistogramSparse("ChromeOS.Camera.ModuleID",
                           GetMaskedModuleID(module_id));

  camera_module_->OpenDevice(camera_id, std::move(device_ops_receiver),
                             std::move(callback));
}

// CameraModuleCallbacks implementations.
void CameraHalDelegate::CameraDeviceStatusChange(
    int32_t camera_id,
    cros::mojom::CameraDeviceStatus new_status) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  CAMERA_LOG(EVENT) << "camera_id = " << camera_id
                    << ", new_status = " << new_status;
  base::AutoLock lock(camera_info_lock_);
  auto it = camera_info_.find(camera_id);
  switch (new_status) {
    case cros::mojom::CameraDeviceStatus::CAMERA_DEVICE_STATUS_PRESENT:
      if (it == camera_info_.end()) {
        // Get info for the newly connected external camera.
        // |has_camera_connected_| might be signaled in
        // OnGotCameraInfoOnIpcThread().
        pending_external_camera_info_.insert(camera_id);
        if (pending_external_camera_info_.size() == 1) {
          external_camera_info_updated_.Reset();
        }
        GetCameraInfoOnIpcThread(
            camera_id,
            base::BindOnce(&CameraHalDelegate::OnGotCameraInfoOnIpcThread,
                           base::Unretained(this), camera_id));
      } else {
        LOG(WARNING) << "Ignore duplicated camera_id = " << camera_id;
      }
      break;
    case cros::mojom::CameraDeviceStatus::CAMERA_DEVICE_STATUS_NOT_PRESENT:
      if (it != camera_info_.end()) {
        camera_info_.erase(it);
        if (camera_info_.empty()) {
          has_camera_connected_.Reset();
        }
        NotifyVideoCaptureDevicesChanged();
      } else {
        LOG(WARNING) << "Ignore nonexistent camera_id = " << camera_id;
      }
      break;
    default:
      NOTREACHED() << "Unexpected new status " << new_status;
  }
}

void CameraHalDelegate::TorchModeStatusChange(
    int32_t camera_id,
    cros::mojom::TorchModeStatus new_status) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  // Do nothing here as we don't care about torch mode status.
}

}  // namespace media
