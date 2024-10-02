// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <cctype>
#include <list>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/audio_service_listener.h"
#include "content/browser/renderer_host/media/in_process_video_capture_provider.h"
#include "content/browser/renderer_host/media/media_capture_devices_impl.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/service_video_capture_provider.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/renderer_host/media/video_capture_provider_switcher.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "crypto/hmac.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/media_switches.h"
#include "media/capture/content/screen_enumerator.h"
#include "media/capture/video/create_video_capture_device_factory.h"
#include "media/capture/video/fake_video_capture_device.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/video_capture_system_impl.h"
#include "media/mojo/mojom/display_media_information.mojom.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "content/browser/gpu/chromeos/video_capture_dependencies.h"
#include "content/browser/gpu/gpu_memory_buffer_manager_singleton.h"
#include "content/public/browser/chromeos/multi_capture_service.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/public/cros_features.h"
#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

using blink::mojom::MediaDeviceType;

namespace content {

ABSL_CONST_INIT thread_local MediaStreamManager* media_stream_manager = nullptr;

using blink::MediaStreamDevice;
using blink::MediaStreamDevices;
using blink::MediaStreamRequestType;
using blink::StreamControls;
using blink::TrackControls;
using blink::mojom::GetOpenDeviceResponse;
using blink::mojom::MediaStreamRequestResult;
using blink::mojom::MediaStreamType;
using blink::mojom::StreamSelectionInfo;
using blink::mojom::StreamSelectionInfoPtr;
using blink::mojom::StreamSelectionStrategy;

namespace {
// Turns off available audio effects (removes the flag) if the options
// explicitly turn them off.
void FilterAudioEffects(const StreamControls& controls, int* effects) {
  DCHECK(effects);
  // TODO(ajm): Should we handle ECHO_CANCELLER here?
}

// Unlike other effects, hotword is off by default, so turn it on if it's
// requested and available.
void EnableHotwordEffect(const StreamControls& controls, int* effects) {
  DCHECK(effects);
  if (controls.hotword_enabled) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Only enable if a hotword device exists.
    if (ash::CrasAudioHandler::Get()->HasHotwordDevice()) {
      *effects |= media::AudioParameters::HOTWORD;
    }
#endif
  }
}

// Gets raw |device_id| and |group_id| when given a hashed device_id
// |hmac_device_id|. Both |device_id| and |group_id| could be null pointers.
bool GetDeviceIDAndGroupIDFromHMAC(
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& hmac_device_id,
    const blink::WebMediaDeviceInfoArray& devices,
    std::string* device_id,
    absl::optional<std::string>* group_id) {
  // The source_id can be empty if the constraint is set but empty.
  if (hmac_device_id.empty()) {
    return false;
  }

  for (const auto& device_info : devices) {
    if (!MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
            salt, security_origin, hmac_device_id, device_info.device_id)) {
      continue;
    }
    if (device_id) {
      *device_id = device_info.device_id;
    }
    if (group_id) {
      *group_id = device_info.group_id.empty()
                      ? absl::nullopt
                      : absl::make_optional<std::string>(device_info.group_id);
    }
    return true;
  }
  return false;
}

MediaStreamType ConvertToMediaStreamType(MediaDeviceType type) {
  switch (type) {
    case MediaDeviceType::MEDIA_AUDIO_INPUT:
      return MediaStreamType::DEVICE_AUDIO_CAPTURE;
    case MediaDeviceType::MEDIA_VIDEO_INPUT:
      return MediaStreamType::DEVICE_VIDEO_CAPTURE;
    default:
      NOTREACHED();
  }

  return MediaStreamType::NO_SERVICE;
}

MediaDeviceType ConvertToMediaDeviceType(MediaStreamType stream_type) {
  switch (stream_type) {
    case MediaStreamType::DEVICE_AUDIO_CAPTURE:
      return MediaDeviceType::MEDIA_AUDIO_INPUT;
    case MediaStreamType::DEVICE_VIDEO_CAPTURE:
      return MediaDeviceType::MEDIA_VIDEO_INPUT;
    default:
      NOTREACHED();
  }

  return MediaDeviceType::NUM_MEDIA_DEVICE_TYPES;
}

const char* DeviceTypeToString(MediaDeviceType type) {
  switch (type) {
    case MediaDeviceType::MEDIA_AUDIO_INPUT:
      return "DEVICE_AUDIO_INPUT";
    case MediaDeviceType::MEDIA_AUDIO_OUTPUT:
      return "DEVICE_AUDIO_OUTPUT";
    case MediaDeviceType::MEDIA_VIDEO_INPUT:
      return "DEVICE_VIDEO_INPUT";
    default:
      NOTREACHED();
  }
  return "INVALID";
}

const char* RequestTypeToString(blink::MediaStreamRequestType type) {
  switch (type) {
    case blink::MEDIA_DEVICE_ACCESS:
      return "MEDIA_DEVICE_ACCESS";
    case blink::MEDIA_DEVICE_UPDATE:
      return "MEDIA_DEVICE_UPDATE";
    case blink::MEDIA_GENERATE_STREAM:
      return "MEDIA_GENERATE_STREAM";
    case blink::MEDIA_GET_OPEN_DEVICE:
      return "MEDIA_GET_OPEN_DEVICE";
    case blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY:
      return "MEDIA_OPEN_DEVICE_PEPPER_ONLY";
    default:
      NOTREACHED();
  }
  return "INVALID";
}

const char* StreamTypeToString(blink::mojom::MediaStreamType type) {
  switch (type) {
    case blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE:
      return "DEVICE_AUDIO_CAPTURE";
    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      return "DEVICE_VIDEO_CAPTURE";
    case blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE:
      return "GUM_TAB_AUDIO_CAPTURE";
    case blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE:
      return "GUM_TAB_VIDEO_CAPTURE";
    case blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE:
      return "GUM_DESKTOP_AUDIO_CAPTURE";
    case blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
      return "GUM_DESKTOP_VIDEO_CAPTURE";
    case blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE:
      return "DISPLAY_AUDIO_CAPTURE";
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
      return "DISPLAY_VIDEO_CAPTURE";
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB:
      return "DISPLAY_VIDEO_CAPTURE_THIS_TAB";
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET:
      return "DISPLAY_VIDEO_CAPTURE_SET";
    case blink::mojom::MediaStreamType::NO_SERVICE:
      return "NO_SERVICE";
    case blink::mojom::MediaStreamType::NUM_MEDIA_TYPES:
      return "NUM_MEDIA_TYPES";
    default:
      NOTREACHED();
  }
  return "INVALID";
}

const char* RequestStateToString(MediaRequestState state) {
  switch (state) {
    case MEDIA_REQUEST_STATE_NOT_REQUESTED:
      return "STATE_NOT_REQUESTED";
    case MEDIA_REQUEST_STATE_REQUESTED:
      return "STATE_REQUESTED";
    case MEDIA_REQUEST_STATE_PENDING_APPROVAL:
      return "STATE_PENDING_APPROVAL";
    case MEDIA_REQUEST_STATE_OPENING:
      return "STATE_OPENING";
    case MEDIA_REQUEST_STATE_DONE:
      return "STATE_DONE";
    case MEDIA_REQUEST_STATE_CLOSING:
      return "STATE_CLOSING";
    case MEDIA_REQUEST_STATE_ERROR:
      return "STATE_ERROR";
    default:
      NOTREACHED();
  }
  return "INVALID";
}

const char* RequestResultToString(
    blink::mojom::MediaStreamRequestResult result) {
  switch (result) {
    case blink::mojom::MediaStreamRequestResult::OK:
      return "OK";
    case blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED:
      return "PERMISSION_DENIED";
    case blink::mojom::MediaStreamRequestResult::PERMISSION_DISMISSED:
      return "PERMISSION_DISMISSED";
    case blink::mojom::MediaStreamRequestResult::INVALID_STATE:
      return "INVALID_STATE";
    case blink::mojom::MediaStreamRequestResult::NO_HARDWARE:
      return "NO_HARDWARE";
    case blink::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN:
      return "INVALID_SECURITY_ORIGIN";
    case blink::mojom::MediaStreamRequestResult::TAB_CAPTURE_FAILURE:
      return "INVALID_STATE";
    case blink::mojom::MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE:
      return "TAB_CAPTURE_FAILURE";
    case blink::mojom::MediaStreamRequestResult::CAPTURE_FAILURE:
      return "CAPTURE_FAILURE";
    case blink::mojom::MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED:
      return "CONSTRAINT_NOT_SATISFIED";
    case blink::mojom::MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO:
      return "TRACK_START_FAILURE_AUDIO";
    case blink::mojom::MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO:
      return "TRACK_START_FAILURE_VIDEO";
    case blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED:
      return "NOT_SUPPORTED";
    case blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN:
      return "FAILED_DUE_TO_SHUTDOWN";
    case blink::mojom::MediaStreamRequestResult::KILL_SWITCH_ON:
      return "KILL_SWITCH_ON";
    case blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED:
      return "SYSTEM_PERMISSION_DENIED";
    case blink::mojom::MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS:
      return "NUM_MEDIA_REQUEST_RESULTS";
    default:
      NOTREACHED();
  }
  return "INVALID";
}

std::string GetGenerateStreamsLogString(int render_process_id,
                                        int render_frame_id,
                                        int requester_id,
                                        int page_request_id) {
  return base::StringPrintf(
      "GenerateStreams({render_process_id=%d}, {render_frame_id=%d}, "
      "{requester_id=%d}, {page_request_id=%d})",
      render_process_id, render_frame_id, requester_id, page_request_id);
}

std::string GetOpenDeviceLogString(int render_process_id,
                                   int render_frame_id,
                                   int requester_id,
                                   int page_request_id,
                                   const std::string& device_id,
                                   MediaStreamType type) {
  return base::StringPrintf(
      "OpenDevice({render_process_id=%d}, {render_frame_id=%d}, "
      "{requester_id=%d}, {page_request_id=%d}, {device_id=%s}, {type=%s})",
      render_process_id, render_frame_id, requester_id, page_request_id,
      device_id.c_str(), StreamTypeToString(type));
}

std::string GetStopStreamDeviceLogString(
    int render_process_id,
    int render_frame_id,
    int requester_id,
    const std::string& device_id,
    const base::UnguessableToken& session_id) {
  return base::StringPrintf(
      "StopStreamDevice({render_process_id=%d}, {render_frame_id=%d}, "
      "{requester_id=%d}, {device_id=%s}, {session_id=%s})",
      render_process_id, render_frame_id, requester_id, device_id.c_str(),
      session_id.ToString().c_str());
}

void SendLogMessage(const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog("MSM::" + message);
}

void SendVideoCaptureLogMessage(const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog("video capture: " + message);
}

// Returns MediaStreamDevices for getDisplayMedia() calls.
// Returns a video device built with DesktopMediaID with fake initializers if
// |kUseFakeDeviceForMediaStream| is set and |preferred_display_surface| is no
// preference. Otherwise, if |preferred_display_surface| specifies a screen,
// window, or tab, returns a video device with matching DesktopMediaID. Returns
// a video device with default DesktopMediaID otherwise.
// Returns an audio device with default device parameters.
// If |kUseFakeDeviceForMediaStream| specifies a
// browser window, use |render_process_id| and |render_frame_id| as the browser
// window identifier.
MediaStreamDevices DisplayMediaDevicesFromFakeDeviceConfig(
    blink::mojom::MediaStreamType media_type,
    bool request_audio,
    int render_process_id,
    int render_frame_id,
    blink::mojom::PreferredDisplaySurface preferred_display_surface) {
  MediaStreamDevices devices;
  DesktopMediaID::Type desktop_media_type = DesktopMediaID::TYPE_SCREEN;
  DesktopMediaID::Id desktop_media_id_id = DesktopMediaID::kNullId;
  WebContentsMediaCaptureId web_contents_id;
  media::mojom::DisplayCaptureSurfaceType display_surface =
      media::mojom::DisplayCaptureSurfaceType::MONITOR;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line &&
      command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream)) {
    std::vector<media::FakeVideoCaptureDeviceSettings> config;
    media::FakeVideoCaptureDeviceFactory::
        ParseFakeDevicesConfigFromOptionsString(
            command_line->GetSwitchValueASCII(
                switches::kUseFakeDeviceForMediaStream),
            &config);
    if (!config.empty()) {
      desktop_media_type = DesktopMediaID::TYPE_NONE;
      desktop_media_id_id = DesktopMediaID::kFakeId;
      switch (config[0].display_media_type) {
        case media::FakeVideoCaptureDevice::DisplayMediaType::ANY:
        case media::FakeVideoCaptureDevice::DisplayMediaType::MONITOR:
          desktop_media_type = DesktopMediaID::TYPE_SCREEN;
          display_surface = media::mojom::DisplayCaptureSurfaceType::MONITOR;
          break;
        case media::FakeVideoCaptureDevice::DisplayMediaType::WINDOW:
          desktop_media_type = DesktopMediaID::TYPE_WINDOW;
          display_surface = media::mojom::DisplayCaptureSurfaceType::WINDOW;
          break;
        case media::FakeVideoCaptureDevice::DisplayMediaType::BROWSER:
          desktop_media_type = DesktopMediaID::TYPE_WEB_CONTENTS;
          display_surface = media::mojom::DisplayCaptureSurfaceType::BROWSER;
          web_contents_id = {render_process_id, render_frame_id};
          break;
      }
    }
  }
  switch (preferred_display_surface) {
    case blink::mojom::PreferredDisplaySurface::NO_PREFERENCE:
      break;
    case blink::mojom::PreferredDisplaySurface::MONITOR:
      desktop_media_type = DesktopMediaID::TYPE_SCREEN;
      display_surface = media::mojom::DisplayCaptureSurfaceType::MONITOR;
      break;
    case blink::mojom::PreferredDisplaySurface::WINDOW:
      desktop_media_type = DesktopMediaID::TYPE_WINDOW;
      display_surface = media::mojom::DisplayCaptureSurfaceType::WINDOW;
      break;
    case blink::mojom::PreferredDisplaySurface::BROWSER:
      desktop_media_type = DesktopMediaID::TYPE_WEB_CONTENTS;
      display_surface = media::mojom::DisplayCaptureSurfaceType::BROWSER;
      web_contents_id = {render_process_id, render_frame_id};
      break;
  }
  DesktopMediaID media_id(desktop_media_type, desktop_media_id_id,
                          web_contents_id);
  MediaStreamDevice device(media_type, media_id.ToString(),
                           media_id.ToString());
  device.display_media_info = media::mojom::DisplayMediaInformation::New(
      display_surface, /*logical_surface=*/true,
      media::mojom::CursorCaptureType::NEVER, /*capture_handle=*/nullptr);
  devices.push_back(device);
  if (!request_audio) {
    return devices;
  }

  MediaStreamDevice audio_device(
      MediaStreamType::DISPLAY_AUDIO_CAPTURE,
      media::AudioDeviceDescription::kDefaultDeviceId, "Fake audio");
  audio_device.display_media_info = media::mojom::DisplayMediaInformation::New(
      display_surface, /*logical_surface=*/true,
      media::mojom::CursorCaptureType::NEVER, /*capture_handle=*/nullptr);
  devices.emplace_back(audio_device);
  return devices;
}

void FinalizeGetMediaDeviceIDForHMAC(
    MediaDeviceType type,
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& source_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceCallback<void(const absl::optional<std::string>&)> callback,
    const MediaDeviceEnumeration& enumeration) {
  for (const auto& device : enumeration[static_cast<size_t>(type)]) {
    if (MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
            salt, security_origin, source_id, device.device_id)) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), device.device_id));
      return;
    }
  }
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), absl::nullopt));
}

bool ChangeSourceSupported(const MediaStreamDevices& devices) {
  for (const MediaStreamDevice& device : devices) {
    DesktopMediaID media_id = DesktopMediaID::Parse(device.id);
    if (media_id.type != DesktopMediaID::TYPE_WEB_CONTENTS) {
      return false;  // Change of source only supported between tabs.
    }
  }

  for (const MediaStreamDevice& device : devices) {
    if (device.type == MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE) {
      return true;  // Established API supporting share-this-tab-instead.
    }
  }

  if (!base::FeatureList::IsEnabled(
          media::kShareThisTabInsteadButtonGetDisplayMedia)) {
    return false;  // Killswitch engaged.
  }

  if (!base::Contains(devices, MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                      &MediaStreamDevice::type) &&
      !base::Contains(devices, MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB,
                      &MediaStreamDevice::type)) {
    return false;  // Not an API call that supports share-this-tab-instead.
  }

  if (!base::FeatureList::IsEnabled(
          media::kShareThisTabInsteadButtonGetDisplayMediaAudio) &&
      base::Contains(devices, MediaStreamType::DISPLAY_AUDIO_CAPTURE,
                     &MediaStreamDevice::type)) {
    // The user chose to capture audio, but the killswitch against
    // share-this-tab-instead with audio is engaged.
    return false;
  }

  return true;  // getDisplayMedia() and killswitches did not trigger.
}

#if !BUILDFLAG(IS_ANDROID)
base::TimeDelta GetConditionalFocusWindow() {
  const std::string custom_window =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          blink::switches::kConditionalFocusWindowMs);

  if (!custom_window.empty()) {
    int64_t ms;
    if (base::StringToInt64(custom_window, &ms) && ms >= 0) {
      return base::Milliseconds(ms);
    } else {
      LOG(ERROR) << "Could not parse custom conditional focus window.";
    }
  }

  // If this value is changed, some of the histograms associated with
  // Conditional Focus should also change.
  return base::Seconds(1);
}
#endif

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MediaStreamRequestResult2 {
  kOk = 0,
  kPermissionDenied = 1,
  kPermissionDismissed = 2,
  kInvalidState = 3,
  kNoHardware = 4,
  kInvalidSecurityOrigin = 5,
  kTabCaptureFailure = 6,
  kScreenCaptureFailure = 7,
  kCaptureFailure = 8,
  kConstraintNotSatisfied = 9,
  kTrackStartFailureAudio = 10,
  kTrackStartFailureVideo = 11,
  kNotSupported = 12,
  kFailedDueToShutdown = 13,
  kKillSwitchOn = 14,
  kSystemPermissionDenied = 15,
  kDeviceInUse = 16,
  kMaxValue = kDeviceInUse
};

void RecordMediaStreamRequestResult2(blink::mojom::MediaStreamType video_type,
                                     MediaStreamRequestResult2 result2) {
  switch (video_type) {
    case blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
      base::UmaHistogramEnumeration(
          "Media.MediaStreamManager.DesktopVideoDeviceUpdate", result2);
      return;
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET:
      base::UmaHistogramEnumeration(
          "Media.MediaStreamManager.DisplayVideoDeviceUpdate", result2);
      return;
    default:
      return;
  }
}

void RecordMediaDeviceUpdateResponseMetric(
    blink::mojom::MediaStreamType video_type,
    MediaStreamRequestResult result) {
  switch (result) {
    case MediaStreamRequestResult::OK:
      RecordMediaStreamRequestResult2(video_type,
                                      MediaStreamRequestResult2::kOk);
      return;
    case MediaStreamRequestResult::PERMISSION_DENIED:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kPermissionDenied);
      return;
    case MediaStreamRequestResult::PERMISSION_DISMISSED:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kPermissionDismissed);
      return;
    case MediaStreamRequestResult::INVALID_STATE:
      RecordMediaStreamRequestResult2(video_type,
                                      MediaStreamRequestResult2::kInvalidState);
      return;
    case MediaStreamRequestResult::NO_HARDWARE:
      RecordMediaStreamRequestResult2(video_type,
                                      MediaStreamRequestResult2::kNoHardware);
      return;
    case MediaStreamRequestResult::INVALID_SECURITY_ORIGIN:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kInvalidSecurityOrigin);
      return;
    case MediaStreamRequestResult::TAB_CAPTURE_FAILURE:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kTabCaptureFailure);
      return;
    case MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kScreenCaptureFailure);
      return;
    case MediaStreamRequestResult::CAPTURE_FAILURE:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kCaptureFailure);
      return;
    case MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kConstraintNotSatisfied);
      return;
    case MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kTrackStartFailureAudio);
      return;
    case MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kTrackStartFailureVideo);
      return;
    case MediaStreamRequestResult::NOT_SUPPORTED:
      RecordMediaStreamRequestResult2(video_type,
                                      MediaStreamRequestResult2::kNotSupported);
      return;
    case MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kFailedDueToShutdown);
      return;
    case MediaStreamRequestResult::KILL_SWITCH_ON:
      RecordMediaStreamRequestResult2(video_type,
                                      MediaStreamRequestResult2::kKillSwitchOn);
      return;
    case MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED:
      RecordMediaStreamRequestResult2(
          video_type, MediaStreamRequestResult2::kSystemPermissionDenied);
      return;
    case MediaStreamRequestResult::DEVICE_IN_USE:
      RecordMediaStreamRequestResult2(video_type,
                                      MediaStreamRequestResult2::kDeviceInUse);
      return;
    case MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS:
      break;
  }
  NOTREACHED();
}

void RecordMediaStreamRequestResponseMetric(
    blink::mojom::MediaStreamType video_type,
    blink::MediaStreamRequestType request_type,
    MediaStreamRequestResult result) {
  switch (request_type) {
    case blink::MEDIA_DEVICE_UPDATE:
      RecordMediaDeviceUpdateResponseMetric(video_type, result);
      return;
    case blink::MEDIA_DEVICE_ACCESS:
    case blink::MEDIA_GENERATE_STREAM:
    case blink::MEDIA_GET_OPEN_DEVICE:
    case blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY:
      return;
  }
}

const blink::MediaStreamDevice* GetStreamDevice(
    const blink::mojom::StreamDevices& stream_devices,
    const base::UnguessableToken& session_id) {
  for (const absl::optional<blink::MediaStreamDevice>* device_ptr : {
           &stream_devices.audio_device,
           &stream_devices.video_device,
       }) {
    if (!device_ptr->has_value()) {
      continue;
    }
    const blink::MediaStreamDevice& device = device_ptr->value();
    if (device.session_id() == session_id) {
      return &device;
    }
  }
  return nullptr;
}

#if BUILDFLAG(IS_CHROMEOS)
static void NotifyMultiCaptureStopped(const std::string& label) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* const service = chromeos::LacrosService::Get();
  if (!service->IsRegistered<crosapi::mojom::MultiCaptureService>() ||
      !service->IsAvailable<crosapi::mojom::MultiCaptureService>()) {
    LOG(ERROR) << "chrome.MultiCaptureService is not available in Lacros.";
    return;
  }
  crosapi::mojom::MultiCaptureService* const multi_capture_service =
      service->GetRemote<crosapi::mojom::MultiCaptureService>().get();
  multi_capture_service->MultiCaptureStopped(label);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  content::GetMultiCaptureService().NotifyMultiCaptureStopped(label);
#endif
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

// MediaStreamManager::DeviceRequest represents a request to either enumerate
// available devices or open one or more devices.
// TODO(perkj): MediaStreamManager still needs refactoring. I propose we create
// several subclasses of DeviceRequest and move some of the responsibility of
// the MediaStreamManager to the subclasses to get rid of the way too many if
// statements in MediaStreamManager.
class MediaStreamManager::DeviceRequest {
 public:
  DeviceRequest(
      int requesting_process_id,
      int requesting_frame_id,
      int requester_id,
      int page_request_id,
      bool user_gesture,
      StreamSelectionInfoPtr audio_stream_selection_info_ptr,
      MediaStreamRequestType request_type,
      const StreamControls& stream_controls,
      MediaDeviceSaltAndOrigin salt_and_origin,
      DeviceStoppedCallback device_stopped_cb = DeviceStoppedCallback())
      : requesting_process_id(requesting_process_id),
        requesting_frame_id(requesting_frame_id),
        requester_id(requester_id),
        page_request_id(page_request_id),
        user_gesture(user_gesture),
        audio_stream_selection_info_ptr(
            std::move(audio_stream_selection_info_ptr)),
        salt_and_origin(std::move(salt_and_origin)),
        device_stopped_cb(std::move(device_stopped_cb)),
        should_stop_in_future_(
            /*size=*/static_cast<size_t>(MediaStreamType::NUM_MEDIA_TYPES),
            /*value=*/false),
        state_(/*size=*/static_cast<size_t>(MediaStreamType::NUM_MEDIA_TYPES),
               /*value=*/MEDIA_REQUEST_STATE_NOT_REQUESTED),
        devices_opened_count_(
            /*size=*/static_cast<size_t>(MediaStreamType::NUM_MEDIA_TYPES),
            /*value=*/0u),
        transfer_status_map_(
            /*size=*/static_cast<size_t>(MediaStreamType::NUM_MEDIA_TYPES)),
        request_type_(request_type),
        stream_controls_(stream_controls),
        audio_type_(MediaStreamType::NO_SERVICE),
        video_type_(MediaStreamType::NO_SERVICE),
        target_process_id_(-1),
        target_frame_id_(-1) {
    SendLogMessage(base::StringPrintf(
        "DR::DeviceRequest({requesting_process_id=%d}, "
        "{requesting_frame_id=%d}, {requester_id=%d}, {request_type=%s})",
        requesting_process_id, requesting_frame_id, requester_id,
        RequestTypeToString(request_type)));
  }

  virtual ~DeviceRequest() = default;

  void set_request_type(MediaStreamRequestType type) { request_type_ = type; }
  MediaStreamRequestType request_type() const { return request_type_; }

  const StreamControls& stream_controls() const { return stream_controls_; }

  void SetAudioType(MediaStreamType audio_type) {
    DCHECK(blink::IsAudioInputMediaType(audio_type) ||
           audio_type == MediaStreamType::NO_SERVICE);
    SendLogMessage(base::StringPrintf(
        "DR::SetAudioType([requester_id=%d] {audio_type=%s})", requester_id,
        StreamTypeToString(audio_type)));
    audio_type_ = audio_type;
  }

  MediaStreamType audio_type() const { return audio_type_; }

  void SetVideoType(MediaStreamType video_type) {
    DCHECK(blink::IsVideoInputMediaType(video_type) ||
           video_type == MediaStreamType::NO_SERVICE);
    video_type_ = video_type;
  }

  MediaStreamType video_type() const { return video_type_; }

  // Creates a MediaStreamRequest object that is used by this request when UI
  // is asked for permission and device selection.
  void CreateUIRequest(const std::string& requested_audio_device_id,
                       const std::string& requested_video_device_id) {
    DCHECK(!ui_request_);
    SendLogMessage(base::StringPrintf(
        "DR::CreateUIRequest([requester_id=%d] {requested_audio_device_id=%s}, "
        "{requested_video_device_id=%s})",
        requester_id, requested_audio_device_id.c_str(),
        requested_video_device_id.c_str()));
    target_process_id_ = requesting_process_id;
    target_frame_id_ = requesting_frame_id;
    ui_request_ = std::make_unique<MediaStreamRequest>(
        requesting_process_id, requesting_frame_id, page_request_id,
        salt_and_origin.origin.GetURL(), user_gesture, request_type_,
        requested_audio_device_id, requested_video_device_id, audio_type_,
        video_type_, stream_controls_.disable_local_echo,
        stream_controls_.request_pan_tilt_zoom_permission);
    ui_request_->suppress_local_audio_playback =
        stream_controls_.suppress_local_audio_playback;
    ui_request_->exclude_system_audio = stream_controls_.exclude_system_audio;
    ui_request_->exclude_self_browser_surface =
        stream_controls_.exclude_self_browser_surface;
    ui_request_->preferred_display_surface =
        stream_controls_.preferred_display_surface;
  }

  // Creates a tab capture specific MediaStreamRequest object that is used by
  // this request when UI is asked for permission and device selection.
  void CreateTabCaptureUIRequest(int target_render_process_id,
                                 int target_render_frame_id) {
    DCHECK(!ui_request_);
    target_process_id_ = target_render_process_id;
    target_frame_id_ = target_render_frame_id;
    ui_request_ = std::make_unique<MediaStreamRequest>(
        target_render_process_id, target_render_frame_id, page_request_id,
        salt_and_origin.origin.GetURL(), user_gesture, request_type_, "", "",
        audio_type_, video_type_, stream_controls_.disable_local_echo,
        /*request_pan_tilt_zoom_permission=*/false);
    ui_request_->exclude_system_audio = stream_controls_.exclude_system_audio;
  }

  bool HasUIRequest() const { return ui_request_.get() != nullptr; }
  std::unique_ptr<MediaStreamRequest> DetachUIRequest() {
    return std::move(ui_request_);
  }

  // Update the request state and notify observers.
  void SetState(MediaStreamType stream_type, MediaRequestState new_state) {
    SendLogMessage(base::StringPrintf(
        "DR::SetState([requester_id=%d] {stream_type=%s}, {new_state=%s})",
        requester_id, StreamTypeToString(stream_type),
        RequestStateToString(new_state)));

    if (stream_type == MediaStreamType::NUM_MEDIA_TYPES) {
      for (int i = static_cast<int>(MediaStreamType::NO_SERVICE) + 1;
           i < static_cast<int>(MediaStreamType::NUM_MEDIA_TYPES); ++i) {
        state_[i] = new_state;
      }
    } else {
      state_[static_cast<int>(stream_type)] = new_state;
    }

#if BUILDFLAG(IS_CHROMEOS)
    NotifyMultiCaptureStateChanged(new_state);
#endif  // BUILDFLAG(IS_CHROMEOS)

    MediaObserver* media_observer =
        GetContentClient()->browser()->GetMediaObserver();
    if (!media_observer) {
      return;
    }

    if (stream_type == MediaStreamType::NUM_MEDIA_TYPES) {
      for (int i = static_cast<int>(MediaStreamType::NO_SERVICE) + 1;
           i < static_cast<int>(MediaStreamType::NUM_MEDIA_TYPES); ++i) {
        media_observer->OnMediaRequestStateChanged(
            target_process_id_, target_frame_id_, page_request_id,
            salt_and_origin.origin.GetURL(), static_cast<MediaStreamType>(i),
            new_state);
      }
    } else {
      media_observer->OnMediaRequestStateChanged(
          target_process_id_, target_frame_id_, page_request_id,
          salt_and_origin.origin.GetURL(), stream_type, new_state);
    }
  }

  bool ShouldStopInFuture(MediaStreamType stream_type) {
    return should_stop_in_future_[static_cast<int>(stream_type)];
  }

  void SetShouldStopInFuture(MediaStreamType stream_type,
                             bool should_be_stopped) {
    should_stop_in_future_[static_cast<int>(stream_type)] = should_be_stopped;
  }

  MediaRequestState state(MediaStreamType stream_type) const {
    return state_[static_cast<int>(stream_type)];
  }

  void ResetDevicesOpened(MediaStreamType stream_type) {
    devices_opened_count_[static_cast<int>(stream_type)] = 0;
  }

  void SetDeviceOpened(MediaStreamType stream_type) {
    devices_opened_count_[static_cast<int>(stream_type)]++;
  }

  size_t devices_opened_count(MediaStreamType stream_type) const {
    return devices_opened_count_[static_cast<int>(stream_type)];
  }

  absl::optional<TransferState> GetTransferState(
      MediaStreamType stream_type,
      const base::UnguessableToken& transfer_id) {
    auto transfer_map = transfer_status_map_[static_cast<int>(stream_type)];
    auto it = transfer_map.find(transfer_id);
    if (it == transfer_map.end()) {
      return absl::nullopt;
    }
    return it->second.state;
  }

  void SetTransferState(MediaStreamType stream_type,
                        const base::UnguessableToken& transfer_id,
                        TransferState transfer_state) {
    auto& transfer_map = transfer_status_map_[static_cast<int>(stream_type)];
    transfer_map[transfer_id] = {transfer_state,
                                 /*start_time=*/base::TimeTicks::Now()};
  }

  bool IsTransferMapEmpty(MediaStreamType stream_type) const {
    return transfer_status_map_[static_cast<int>(stream_type)].empty();
  }

  void RemoveEntryInTransferMap(MediaStreamType stream_type,
                                const base::UnguessableToken& transfer_id) {
    auto& transfer_map = transfer_status_map_[static_cast<int>(stream_type)];
    transfer_map.erase(transfer_id);
  }

  void SetCapturingLinkSecured(bool is_secure) {
    MediaObserver* media_observer =
        GetContentClient()->browser()->GetMediaObserver();
    if (!media_observer) {
      return;
    }

    media_observer->OnSetCapturingLinkSecured(target_process_id_,
                                              target_frame_id_, page_request_id,
                                              video_type_, is_secure);
  }

  // This function checks if the request is for the getDisplayMediaSet API.
  bool IsGetDisplayMediaSet() const {
    return stream_controls_.video.stream_type ==
           blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET;
  }

  void SetLabel(const std::string& label) { label_ = label; }

  void DisableAudioSharing() {
    SetAudioType(MediaStreamType::NO_SERVICE);
    stream_controls_.audio.stream_type = MediaStreamType::NO_SERVICE;
    stream_controls_.hotword_enabled = false;
    stream_controls_.disable_local_echo = false;
    stream_controls_.suppress_local_audio_playback = false;
    stream_controls_.exclude_system_audio = false;
  }

  // TODO(crbug.com/1386165): Remove this method from DeviceRequest when
  // GenerateStreamRequest::FinalizeRequest and
  // GetOpenDeviceRequest::FinalizeRequest have been implemented (this should be
  // an internal callback in those subclasses).
  virtual void PanTiltZoomPermissionChecked(const std::string& label,
                                            bool pan_tilt_zoom_allowed) {
    NOTREACHED();
  }

  // TODO(crbug.com/1386165): Combine FinalizeRequest and
  // FinalizeMediaAccessRequest, implement it for the remaining subclasses and
  // make it into on pure virtual function.
  virtual void FinalizeRequest(const std::string& label) { NOTREACHED(); }

  virtual void FinalizeMediaAccessRequest(
      const std::string& label,
      const blink::mojom::StreamDevicesSet&) {
    NOTREACHED();
  }

  virtual void FinalizeRequestFailed(MediaStreamRequestResult result) = 0;

  virtual void FinalizeChangeDevice(const std::string& label) { NOTREACHED(); }

  virtual void OnRequestStateChangeFromBrowser(
      const std::string& label,
      const DesktopMediaID& media_id,
      blink::mojom::MediaStreamStateChange new_state) {}

  virtual void OnCaptureConfigurationChanged(
      const std::string& label,
      const blink::MediaStreamDevice& device) {}

  base::RepeatingCallback<void(const std::string&,
                               blink::mojom::MediaStreamType type,
                               media::mojom::CaptureHandlePtr)>
  OnCaptureHandleChangeCb() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    return base::BindRepeating(&DeviceRequest::OnCaptureHandleChange,
                               GetWeakPtr());
  }

  // Receives a new capture-handle from the CaptureHandleManager.
  virtual void OnCaptureHandleChange(
      const std::string& label,
      blink::mojom::MediaStreamType type,
      media::mojom::CaptureHandlePtr capture_handle) {}

  // The render process id that requested this stream to be generated and that
  // will receive a handle to the MediaStream. This may be different from
  // MediaStreamRequest::render_process_id which in the tab capture case
  // specifies the target renderer from which audio and video is captured.
  const int requesting_process_id;

  // The render frame id that requested this stream to be generated and that
  // will receive a handle to the MediaStream. This may be different from
  // MediaStreamRequest::render_frame_id which in the tab capture case
  // specifies the target renderer from which audio and video is captured.
  const int requesting_frame_id;

  // The id of the object that requested this stream to be generated and that
  // will receive a handle to the MediaStream. This may be different from
  // MediaStreamRequest::requester_id which in the tab capture case
  // specifies the target renderer from which audio and video is captured.
  const int requester_id;

  // An ID the render frame provided to identify this request.
  const int page_request_id;

  const bool user_gesture;

  // Information as of how to select a stream for an audio device provided by
  // the caller.
  // NB: This information is invalid after the request has been processed.
  StreamSelectionInfoPtr audio_stream_selection_info_ptr;

  const MediaDeviceSaltAndOrigin salt_and_origin;

  blink::mojom::StreamDevicesSet stream_devices_set;
  blink::mojom::StreamDevicesSet old_stream_devices_set;

  DeviceStoppedCallback device_stopped_cb;

  std::unique_ptr<MediaStreamUIProxy> ui_proxy;

  std::string tab_capture_device_id;

  PermissionController::SubscriptionId audio_subscription_id;

  PermissionController::SubscriptionId video_subscription_id;

 protected:
  virtual base::WeakPtr<DeviceRequest> GetWeakPtr() = 0;

 private:
#if BUILDFLAG(IS_CHROMEOS)
  static void NotifyMultiCaptureStarted(const std::string& label,
                                        const url::Origin& origin) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    auto* const service = chromeos::LacrosService::Get();
    if (!service->IsRegistered<crosapi::mojom::MultiCaptureService>() ||
        !service->IsAvailable<crosapi::mojom::MultiCaptureService>()) {
      LOG(ERROR) << "chrome.MultiCaptureService is not available in Lacros.";
      return;
    }
    crosapi::mojom::MultiCaptureService* const multi_capture_service =
        service->GetRemote<crosapi::mojom::MultiCaptureService>().get();
    multi_capture_service->MultiCaptureStarted(label, origin.host());
#elif BUILDFLAG(IS_CHROMEOS_ASH)
    content::GetMultiCaptureService().NotifyMultiCaptureStarted(label, origin);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void NotifyMultiCaptureStateChanged(MediaRequestState new_state) {
    if (!IsGetDisplayMediaSet()) {
      return;
    }
    switch (new_state) {
      case MediaRequestState::MEDIA_REQUEST_STATE_OPENING:
        GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(NotifyMultiCaptureStarted, label_,
                                      salt_and_origin.origin));
        break;
      case MediaRequestState::MEDIA_REQUEST_STATE_ERROR:
        GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(NotifyMultiCaptureStopped, label_));
        break;
      case MediaRequestState::MEDIA_REQUEST_STATE_CLOSING:
      case MediaRequestState::MEDIA_REQUEST_STATE_NOT_REQUESTED:
      case MediaRequestState::MEDIA_REQUEST_STATE_REQUESTED:
      case MediaRequestState::MEDIA_REQUEST_STATE_PENDING_APPROVAL:
      case MediaRequestState::MEDIA_REQUEST_STATE_DONE:
        // Nothing to do as usage indicators only need to shown while the
        // capture is active.
        break;
    }
  }

#endif  // BUILDFLAG(IS_CHROMEOS)

  // Mark true if the MediaStreamDevice of |MediaStreamType| type should be
  // stopped but can't at the moment because of ongoing transfers.
  std::vector<bool> should_stop_in_future_;
  std::vector<MediaRequestState> state_;
  // This vector keeps track of how many devices of a specific |MediaStreamType|
  // were already opened for this request.
  std::vector<size_t> devices_opened_count_;
  std::unique_ptr<MediaStreamRequest> ui_request_;
  // This vector of map tracks all the ongoing transfers of MediaStreamDevice of
  // |MediaStreamType| type.
  std::vector<TransferMap> transfer_status_map_;
  MediaStreamRequestType request_type_;
  StreamControls stream_controls_;
  MediaStreamType audio_type_;
  MediaStreamType video_type_;
  int target_process_id_;
  int target_frame_id_;
  std::string label_;
};

class MediaStreamManager::MediaAccessRequest
    : public MediaStreamManager::DeviceRequest {
 public:
  MediaAccessRequest(int requesting_process_id,
                     int requesting_frame_id,
                     int requester_id,
                     int page_request_id,
                     StreamSelectionInfoPtr audio_stream_selection_info_ptr,
                     const StreamControls& controls,
                     MediaDeviceSaltAndOrigin salt_and_origin,
                     MediaAccessRequestCallback media_access_request_cb)
      : DeviceRequest(requesting_process_id,
                      requesting_frame_id,
                      requester_id,
                      page_request_id,
                      /*user_gesture=*/false,
                      std::move(audio_stream_selection_info_ptr),
                      blink::MEDIA_DEVICE_ACCESS,
                      controls,
                      std::move(salt_and_origin)),
        media_access_request_cb_(std::move(media_access_request_cb)) {}

  ~MediaAccessRequest() override { DCHECK_CURRENTLY_ON(BrowserThread::IO); }

  void FinalizeMediaAccessRequest(
      const std::string& label,
      const blink::mojom::StreamDevicesSet& stream_devices_set) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(media_access_request_cb_);
    SendLogMessage(base::StringPrintf(
        "FinalizeMediaAccessRequest({label=%s}, {requester_id="
        "%d}, {request_type=%s})",
        label.c_str(), requester_id, RequestTypeToString(request_type())));
    std::move(media_access_request_cb_)
        .Run(stream_devices_set, std::move(ui_proxy));
  }

  void FinalizeRequestFailed(MediaStreamRequestResult) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(media_access_request_cb_);
    std::move(media_access_request_cb_)
        .Run(/*stream_devices_set=*/blink::mojom::StreamDevicesSet(),
             std::move(ui_proxy));
  }

 private:
  base::WeakPtr<DeviceRequest> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  // Callback to the requester which audio/video devices have been selected.
  // It can be null if the requester has no interest to know the result.
  MediaAccessRequestCallback media_access_request_cb_;
  base::WeakPtrFactory<DeviceRequest> weak_factory_{this};
};

class MediaStreamManager::CreateDeviceRequest
    : public MediaStreamManager::DeviceRequest {
 public:
  CreateDeviceRequest(
      int requesting_process_id,
      int requesting_frame_id,
      int requester_id,
      int page_request_id,
      bool user_gesture,
      StreamSelectionInfoPtr audio_stream_selection_info_ptr,
      MediaStreamRequestType request_type,
      const StreamControls& controls,
      MediaDeviceSaltAndOrigin salt_and_origin,
      DeviceStoppedCallback device_stopped_cb,
      DeviceChangedCallback device_changed_cb,
      DeviceRequestStateChangeCallback device_request_state_change_cb,
      DeviceCaptureConfigurationChangeCallback
          device_capture_configuration_change_cb,
      DeviceCaptureHandleChangeCallback device_capture_handle_change_cb)
      : DeviceRequest(requesting_process_id,
                      requesting_frame_id,
                      requester_id,
                      page_request_id,
                      user_gesture,
                      std::move(audio_stream_selection_info_ptr),
                      request_type,
                      controls,
                      std::move(salt_and_origin),
                      std::move(device_stopped_cb)),
        device_changed_cb_(std::move(device_changed_cb)),
        device_request_state_change_cb_(
            std::move(device_request_state_change_cb)),
        device_capture_configuration_change_cb_(
            std::move(device_capture_configuration_change_cb)),
        device_capture_handle_change_cb_(
            std::move(device_capture_handle_change_cb)) {}

  void FinalizeChangeDevice(const std::string& label) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(device_changed_cb_);
    DCHECK_EQ(1u, old_stream_devices_set.stream_devices.size());
    DCHECK_EQ(1u, stream_devices_set.stream_devices.size());

    const blink::mojom::StreamDevices& old_devices =
        *old_stream_devices_set.stream_devices[0];
    const blink::mojom::StreamDevices& new_devices =
        *stream_devices_set.stream_devices[0];

    SendLogMessage(base::StringPrintf(
        "FinalizeChangeDevice({label=%s}, {requester_id="
        "%d}, {request_type=%s})",
        label.c_str(), requester_id, RequestTypeToString(request_type())));

    std::vector<std::vector<MediaStreamDevice>> old_devices_by_type(
        static_cast<size_t>(MediaStreamType::NUM_MEDIA_TYPES));
    for (const absl::optional<blink::MediaStreamDevice>* old_device_ptr :
         {&old_devices.audio_device, &old_devices.video_device}) {
      if (!old_device_ptr->has_value()) {
        continue;
      }
      const blink::MediaStreamDevice& old_device = old_device_ptr->value();
      old_devices_by_type[static_cast<size_t>(old_device.type)].push_back(
          old_device);
    }

    for (const absl::optional<blink::MediaStreamDevice>* new_device_ptr :
         {&new_devices.audio_device, &new_devices.video_device}) {
      if (!new_device_ptr->has_value()) {
        continue;
      }
      const blink::MediaStreamDevice& new_device = new_device_ptr->value();
      MediaStreamDevice old_device;
      auto& old_devices_of_new_device_type =
          old_devices_by_type[static_cast<int>(new_device.type)];
      if (!old_devices_of_new_device_type.empty()) {
        old_device = old_devices_of_new_device_type.back();
        old_devices_of_new_device_type.pop_back();
      }

      device_changed_cb_.Run(label, old_device, new_device);
    }

    for (const auto& old_media_stream_devices : old_devices_by_type) {
      for (const auto& old_device : old_media_stream_devices) {
        device_changed_cb_.Run(label, old_device, MediaStreamDevice());
      }
    }
  }

  void OnRequestStateChangeFromBrowser(
      const std::string& label,
      const DesktopMediaID& media_id,
      blink::mojom::MediaStreamStateChange new_state) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    if (!device_request_state_change_cb_) {
      return;
    }

    for (const blink::mojom::StreamDevicesPtr& stream_devices_ptr :
         stream_devices_set.stream_devices) {
      const blink::mojom::StreamDevices& stream_devices = *stream_devices_ptr;
      for (const absl::optional<blink::MediaStreamDevice>* device_ptr : {
               &stream_devices.audio_device,
               &stream_devices.video_device,
           }) {
        if (!device_ptr->has_value()) {
          continue;
        }
        const blink::MediaStreamDevice& device = device_ptr->value();
        if (DesktopMediaID::Parse(device.id) == media_id) {
          device_request_state_change_cb_.Run(label, device, new_state);
        }
      }
    }
  }

  void OnCaptureConfigurationChanged(
      const std::string& label,
      const blink::MediaStreamDevice& device) override {
    if (device_capture_configuration_change_cb_) {
      device_capture_configuration_change_cb_.Run(label, device);
    }
  }

  // Receive a new capture-handle from the CaptureHandleManager.
  void OnCaptureHandleChange(
      const std::string& label,
      blink::mojom::MediaStreamType type,
      media::mojom::CaptureHandlePtr capture_handle) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK_EQ(1u, stream_devices_set.stream_devices.size());
    const blink::mojom::StreamDevices& devices =
        *stream_devices_set.stream_devices[0];

    const MediaStreamDevice* device = nullptr;
    if (blink::IsAudioInputMediaType(type) &&
        devices.audio_device.has_value()) {
      device = &devices.audio_device.value();
    } else if (blink::IsVideoInputMediaType(type) &&
               devices.video_device.has_value()) {
      device = &devices.video_device.value();
    }

    if (!device) {
      return;
    }

    if (!device->display_media_info) {
      DVLOG(1) << "Tab capture without a DisplayMediaInformation (" << label
               << ", " << type << ").";
      return;
    }

    device->display_media_info->capture_handle = capture_handle.Clone();

    if (device_capture_handle_change_cb_) {
      device_capture_handle_change_cb_.Run(label, *device);
    }
  }

 private:
  DeviceChangedCallback device_changed_cb_;
  DeviceRequestStateChangeCallback device_request_state_change_cb_;
  DeviceCaptureConfigurationChangeCallback
      device_capture_configuration_change_cb_;
  DeviceCaptureHandleChangeCallback device_capture_handle_change_cb_;
};

class MediaStreamManager::GenerateStreamsRequest
    : public MediaStreamManager::CreateDeviceRequest {
 public:
  GenerateStreamsRequest(
      int requesting_process_id,
      int requesting_frame_id,
      int requester_id,
      int page_request_id,
      bool user_gesture,
      StreamSelectionInfoPtr audio_stream_selection_info_ptr,
      const StreamControls& controls,
      MediaDeviceSaltAndOrigin salt_and_origin,
      DeviceStoppedCallback device_stopped_cb,
      DeviceChangedCallback device_changed_cb,
      DeviceRequestStateChangeCallback device_request_state_change_cb,
      DeviceCaptureConfigurationChangeCallback
          device_capture_configuration_change_cb,
      DeviceCaptureHandleChangeCallback device_capture_handle_change_cb,
      GenerateStreamsCallback generate_streams_cb)
      : CreateDeviceRequest(requesting_process_id,
                            requesting_frame_id,
                            requester_id,
                            page_request_id,
                            user_gesture,
                            std::move(audio_stream_selection_info_ptr),
                            blink::MEDIA_GENERATE_STREAM,
                            controls,
                            std::move(salt_and_origin),
                            std::move(device_stopped_cb),
                            std::move(device_changed_cb),
                            std::move(device_request_state_change_cb),
                            std::move(device_capture_configuration_change_cb),
                            std::move(device_capture_handle_change_cb)),
        generate_streams_cb_(std::move(generate_streams_cb)) {
    DCHECK(generate_streams_cb_);
  }

  ~GenerateStreamsRequest() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (generate_streams_cb_) {
      std::move(generate_streams_cb_)
          .Run(MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN,
               /*label=*/std::string(),
               /*stream_devices_set=*/nullptr,
               /*pan_tilt_zoom_allowed=*/false);
    }
  }

  void PanTiltZoomPermissionChecked(const std::string& label,
                                    bool pan_tilt_zoom_allowed) override {
    DCHECK(generate_streams_cb_);
    std::move(generate_streams_cb_)
        .Run(MediaStreamRequestResult::OK, label, stream_devices_set.Clone(),
             pan_tilt_zoom_allowed);
  }

  void FinalizeRequestFailed(MediaStreamRequestResult result) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(generate_streams_cb_);
    std::move(generate_streams_cb_)
        .Run(result, /*label=*/std::string(),
             /*stream_devices_set=*/nullptr,
             /*pan_tilt_zoom_allowed=*/false);
  }

 private:
  base::WeakPtr<DeviceRequest> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  GenerateStreamsCallback generate_streams_cb_;
  base::WeakPtrFactory<DeviceRequest> weak_factory_{this};
};

class MediaStreamManager::GetOpenDeviceRequest
    : public MediaStreamManager::CreateDeviceRequest {
 public:
  GetOpenDeviceRequest(
      int requesting_process_id,
      int requesting_frame_id,
      int requester_id,
      int page_request_id,
      MediaDeviceSaltAndOrigin salt_and_origin,
      DeviceStoppedCallback device_stopped_cb,
      DeviceChangedCallback device_changed_cb,
      DeviceRequestStateChangeCallback device_request_state_change_cb,
      DeviceCaptureConfigurationChangeCallback
          device_capture_configuration_change_cb,
      DeviceCaptureHandleChangeCallback device_capture_handle_change_cb,
      GetOpenDeviceCallback get_open_device_cb)
      : CreateDeviceRequest(requesting_process_id,
                            requesting_frame_id,
                            requester_id,
                            page_request_id,
                            /*user_gesture=*/false,
                            /*audio_stream_selection_info_ptr=*/nullptr,
                            blink::MEDIA_GET_OPEN_DEVICE,
                            StreamControls(),
                            std::move(salt_and_origin),
                            std::move(device_stopped_cb),
                            std::move(device_changed_cb),
                            std::move(device_request_state_change_cb),
                            std::move(device_capture_configuration_change_cb),
                            std::move(device_capture_handle_change_cb)),
        get_open_device_cb_(std::move(get_open_device_cb)) {
    DCHECK(get_open_device_cb_);
  }

  ~GetOpenDeviceRequest() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (get_open_device_cb_) {
      std::move(get_open_device_cb_)
          .Run(MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN, nullptr);
    }
  }

  void PanTiltZoomPermissionChecked(const std::string& label,
                                    bool pan_tilt_zoom_allowed) override {
    DCHECK(get_open_device_cb_);
    // GetOpenDevice is only available with exactly one stream.
    DCHECK_EQ(stream_devices_set.stream_devices.size(), 1u);
    const blink::mojom::StreamDevices& stream_devices =
        *stream_devices_set.stream_devices[0];
    // GetOpenDevice should return exactly one device, which can be of either
    // audio or video type.
    DCHECK_NE(stream_devices.audio_device.has_value(),
              stream_devices.video_device.has_value());
    MediaStreamDevice device = blink::IsVideoInputMediaType(video_type())
                                   ? stream_devices.video_device.value()
                                   : stream_devices.audio_device.value();

    std::move(get_open_device_cb_)
        .Run(MediaStreamRequestResult::OK,
             GetOpenDeviceResponse::New(label, device, pan_tilt_zoom_allowed));
  }

  void FinalizeRequestFailed(MediaStreamRequestResult result) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(get_open_device_cb_);
    std::move(get_open_device_cb_).Run(result, /*response=*/nullptr);
  }

 private:
  base::WeakPtr<DeviceRequest> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  // This callback is used by transferred MediaStreamTracks to access and clone
  // an existing open MediaStreamDevice (identified by its session_id). If the
  // device is found, it is returned to this callback along with a
  // MediaStreamRequestResult::OK. Otherwise, returns
  // MediaStreamRequestResult::INVALID_STATE along with absl::nullopt instead of
  // a MediaStreamDevice.
  GetOpenDeviceCallback get_open_device_cb_;
  base::WeakPtrFactory<DeviceRequest> weak_factory_{this};
};

class MediaStreamManager::OpenDeviceRequest
    : public MediaStreamManager::DeviceRequest {
 public:
  OpenDeviceRequest(int requesting_process_id,
                    int requesting_frame_id,
                    int requester_id,
                    int page_request_id,
                    StreamSelectionInfoPtr audio_stream_selection_info_ptr,
                    const StreamControls& controls,
                    MediaDeviceSaltAndOrigin salt_and_origin,
                    DeviceStoppedCallback device_stopped_cb,
                    OpenDeviceCallback open_device_cb)
      : DeviceRequest(requesting_process_id,
                      requesting_frame_id,
                      requester_id,
                      page_request_id,
                      /*user gesture=*/false,
                      std::move(audio_stream_selection_info_ptr),
                      blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY,
                      controls,
                      std::move(salt_and_origin),
                      std::move(device_stopped_cb)),
        open_device_cb_(std::move(open_device_cb)) {
    DCHECK(open_device_cb_);
  }

  ~OpenDeviceRequest() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (open_device_cb_) {
      std::move(open_device_cb_)
          .Run(/*success=*/false, std::string(), MediaStreamDevice());
    }
  }

  void FinalizeRequest(const std::string& label) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(open_device_cb_);
    SendLogMessage(base::StringPrintf(
        "FinalizeOpenDevice({label=%s}, {requester_id="
        "%d}, {request_type=%s})",
        label.c_str(), requester_id, RequestTypeToString(request_type())));
    std::move(open_device_cb_)
        .Run(/*success=*/true, label,
             blink::ToMediaStreamDevicesList(stream_devices_set).front());
  }

  void FinalizeRequestFailed(MediaStreamRequestResult result) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(open_device_cb_);
    std::move(open_device_cb_)
        .Run(/*success=*/false, /*label=*/std::string(), MediaStreamDevice());
  }

 private:
  base::WeakPtr<DeviceRequest> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  // This callback is only used by pepper and tries to open the device
  // identified by device_id. If it is opened successfully, it returns this
  // device. Otherwise, returns an empty device.
  OpenDeviceCallback open_device_cb_;
  base::WeakPtrFactory<DeviceRequest> weak_factory_{this};
};

// static
void MediaStreamManager::SendMessageToNativeLog(const std::string& message) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaStreamManager::SendMessageToNativeLog, message));
    return;
  }
  VLOG(1) << message;

  if (!media_stream_manager) {
    // MediaStreamManager hasn't been initialized. This is allowed in tests.
    return;
  }

  media_stream_manager->AddLogMessageOnIOThread(message);
}

MediaStreamManager::MediaStreamManager(media::AudioSystem* audio_system)
    : MediaStreamManager(audio_system, nullptr) {
  SendLogMessage(base::StringPrintf("MediaStreamManager([this=%p]))", this));
}

MediaStreamManager::MediaStreamManager(
    media::AudioSystem* audio_system,
    std::unique_ptr<VideoCaptureProvider> video_capture_provider)
    :
#if !BUILDFLAG(IS_ANDROID)
      conditional_focus_window_(GetConditionalFocusWindow()),
#endif
      audio_system_(audio_system) {
  bool use_fake_ui_factory = false;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForMediaStream)) {
    use_fake_ui_factory = true;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAutoAcceptCameraAndMicrophoneCapture)) {
    // Crashing the browser process is actually the user-friendly thing to do,
    // as it informs the user of their mistake early.
    CHECK(!use_fake_ui_factory) << "Mutually exclusive command line flags.";
    use_fake_ui_factory = true;
    use_fake_ui_only_for_camera_and_microphone_ = true;
  }

  if (use_fake_ui_factory) {
    fake_ui_factory_ = base::BindRepeating([] {
      return std::make_unique<FakeMediaStreamUIProxy>(
          /*tests_use_fake_render_frame_hosts=*/false);
    });
  }

  if (base::FeatureList::IsEnabled(media::kUseFakeDeviceForMediaStream)) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
  }

  DCHECK(audio_system_);

  if (!video_capture_provider) {
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner;

#if BUILDFLAG(IS_MAC)
    // On MacOS the main thread must be used to run VideoCaptureDevice.
    device_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
#else  // !BUILDFLAG(IS_MAC)
    // For all platforms other than MacOS start a new thread.
    video_capture_thread_.emplace("VideoCaptureThread");
    base::Thread::Options thread_options;
#if BUILDFLAG(IS_WIN)
    // Use an STA Video Capture Thread to try to avoid crashes on enumeration
    // of buggy third party Direct Show modules, http://crbug.com/428958.
    video_capture_thread_->init_com_with_mta(false);
    thread_options.message_pump_type = base::MessagePumpType::UI;
#elif BUILDFLAG(IS_FUCHSIA)
    // On Fuchsia IO thread is required for FIDL connections.
    thread_options.message_pump_type = base::MessagePumpType::IO;
#endif
    CHECK(video_capture_thread_->StartWithOptions(std::move(thread_options)));
    device_task_runner = video_capture_thread_->task_runner();
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (media::ShouldUseCrosCameraService()) {
      media::VideoCaptureDeviceFactoryChromeOS::SetGpuBufferManager(
          GpuMemoryBufferManagerSingleton::GetInstance());
      media::CameraHalDispatcherImpl::GetInstance()->Start(
          base::BindRepeating(
              &VideoCaptureDependencies::CreateJpegDecodeAccelerator),
          base::BindRepeating(
              &VideoCaptureDependencies::CreateJpegEncodeAccelerator));
    }
#endif

    if (base::FeatureList::IsEnabled(features::kMojoVideoCapture)) {
      video_capture_provider = std::make_unique<VideoCaptureProviderSwitcher>(
          std::make_unique<ServiceVideoCaptureProvider>(
              base::BindRepeating(&SendVideoCaptureLogMessage)),
          InProcessVideoCaptureProvider::CreateInstanceForNonDeviceCapture(
              std::move(device_task_runner),
              base::BindRepeating(&SendVideoCaptureLogMessage)));
    } else {
      video_capture_provider = InProcessVideoCaptureProvider::CreateInstance(
          std::make_unique<media::VideoCaptureSystemImpl>(
              media::CreateVideoCaptureDeviceFactory(
                  GetUIThreadTaskRunner({}))),
          std::move(device_task_runner),
          base::BindRepeating(&SendVideoCaptureLogMessage));
    }
  }
  InitializeMaybeAsync(std::move(video_capture_provider));

  audio_service_listener_ = std::make_unique<AudioServiceListener>();
}

MediaStreamManager::~MediaStreamManager() {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO));
  DCHECK(requests_.empty());
}

VideoCaptureManager* MediaStreamManager::video_capture_manager() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(video_capture_manager_.get());
  return video_capture_manager_.get();
}

AudioInputDeviceManager* MediaStreamManager::audio_input_device_manager()
    const {
  // May be called on any thread, provided that we are not in shutdown.
  DCHECK(audio_input_device_manager_.get());
  return audio_input_device_manager_.get();
}

AudioServiceListener* MediaStreamManager::audio_service_listener() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return audio_service_listener_.get();
}

MediaDevicesManager* MediaStreamManager::media_devices_manager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // nullptr might be returned during shutdown.
  return media_devices_manager_.get();
}

media::AudioSystem* MediaStreamManager::audio_system() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return audio_system_;
}

void MediaStreamManager::AddVideoCaptureObserver(
    media::VideoCaptureObserver* capture_observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (video_capture_manager_) {
    video_capture_manager_->AddVideoCaptureObserver(capture_observer);
  }
}

void MediaStreamManager::RemoveAllVideoCaptureObservers() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (video_capture_manager_) {
    video_capture_manager_->RemoveAllVideoCaptureObservers();
  }
}

std::string MediaStreamManager::MakeMediaAccessRequest(
    int render_process_id,
    int render_frame_id,
    int requester_id,
    int page_request_id,
    const StreamControls& controls,
    const url::Origin& security_origin,
    MediaAccessRequestCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  StreamSelectionInfoPtr audio_stream_selection_info_ptr =
      StreamSelectionInfo::New(
          blink::mojom::StreamSelectionStrategy::SEARCH_BY_DEVICE_ID,
          absl::nullopt);
  auto request = std::make_unique<MediaAccessRequest>(
      render_process_id, render_frame_id, requester_id, page_request_id,
      std::move(audio_stream_selection_info_ptr), controls,
      MediaDeviceSaltAndOrigin{
          std::string() /* salt */, std::string() /* group_id_salt */,
          security_origin, true /* has_focus */, false /* is_background */},
      std::move(callback));
  const std::string& label = AddRequest(std::move(request))->first;

  // Post a task and handle the request asynchronously. The reason is that the
  // requester won't have a label for the request until this function returns
  // and thus can not handle a response. Using base::Unretained is safe since
  // MediaStreamManager is deleted on the UI thread, after the IO thread has
  // been stopped.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MediaStreamManager::SetUpRequest,
                                base::Unretained(this), label));
  return label;
}

void MediaStreamManager::GenerateStreams(
    int render_process_id,
    int render_frame_id,
    int requester_id,
    int page_request_id,
    const StreamControls& controls,
    MediaDeviceSaltAndOrigin salt_and_origin,
    bool user_gesture,
    StreamSelectionInfoPtr audio_stream_selection_info_ptr,
    GenerateStreamsCallback generate_streams_cb,
    DeviceStoppedCallback device_stopped_cb,
    DeviceChangedCallback device_changed_cb,
    DeviceRequestStateChangeCallback device_request_state_change_cb,
    DeviceCaptureConfigurationChangeCallback
        device_capture_configuration_change_cb,
    DeviceCaptureHandleChangeCallback device_capture_handle_change_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(GetGenerateStreamsLogString(render_process_id, render_frame_id,
                                             requester_id, page_request_id));
  std::unique_ptr<DeviceRequest> request =
      std::make_unique<GenerateStreamsRequest>(
          render_process_id, render_frame_id, requester_id, page_request_id,
          user_gesture, std::move(audio_stream_selection_info_ptr), controls,
          std::move(salt_and_origin), std::move(device_stopped_cb),
          std::move(device_changed_cb),
          std::move(device_request_state_change_cb),
          std::move(device_capture_configuration_change_cb),
          std::move(device_capture_handle_change_cb),
          std::move(generate_streams_cb));
  DeviceRequests::const_iterator request_it = AddRequest(std::move(request));
  const std::string& label = request_it->first;

  if (generate_stream_test_callback_) {
    // The test callback is responsible to verify whether the |controls| is
    // as expected. Then we need to finish getUserMedia and let Javascript
    // access the result.
    if (std::move(generate_stream_test_callback_).Run(controls)) {
      FinalizeGenerateStreams(label, request_it->second.get());
    } else {
      // This will invalidate both |label| and |request_it|.
      FinalizeRequestFailed(request_it,
                            MediaStreamRequestResult::INVALID_STATE);
    }
    return;
  }

  // Post a task and handle the request asynchronously. The reason is that the
  // requester won't have a label for the request until this function returns
  // and thus can not handle a response. Using base::Unretained is safe since
  // MediaStreamManager is deleted on the UI thread, after the IO thread has
  // been stopped.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MediaStreamManager::SetUpRequest,
                                base::Unretained(this), label));
}

void MediaStreamManager::GetOpenDevice(
    const base::UnguessableToken& device_session_id,
    const base::UnguessableToken& transfer_id,
    int render_process_id,
    int render_frame_id,
    int requester_id,
    int page_request_id,
    MediaDeviceSaltAndOrigin salt_and_origin,
    GetOpenDeviceCallback get_open_device_cb,
    DeviceStoppedCallback device_stopped_cb,
    DeviceChangedCallback device_changed_cb,
    DeviceRequestStateChangeCallback device_request_state_change_cb,
    DeviceCaptureConfigurationChangeCallback
        device_capture_configuration_change_cb,
    DeviceCaptureHandleChangeCallback device_capture_handle_change_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(base::FeatureList::IsEnabled(features::kMediaStreamTrackTransfer));

  std::unique_ptr<DeviceRequest> request =
      std::make_unique<GetOpenDeviceRequest>(
          render_process_id, render_frame_id, requester_id, page_request_id,
          std::move(salt_and_origin), std::move(device_stopped_cb),
          std::move(device_changed_cb),
          std::move(device_request_state_change_cb),
          std::move(device_capture_configuration_change_cb),
          std::move(device_capture_handle_change_cb),
          std::move(get_open_device_cb));

  DeviceRequests::const_iterator request_it = AddRequest(std::move(request));
  const std::string& new_label = request_it->first;
  DeviceRequest* const request_ptr = request_it->second.get();

  const absl::optional<MediaStreamDevice> new_device =
      CloneExistingOpenDevice(device_session_id, transfer_id, new_label);
  if (!new_device.has_value()) {
    // No existing device with matching session id is found.
    // This invalidates |request_it|.
    FinalizeRequestFailed(request_it, MediaStreamRequestResult::INVALID_STATE);
    return;
  }

  request_ptr->stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  if (blink::IsAudioInputMediaType(new_device->type)) {
    request_ptr->stream_devices_set.stream_devices[0]->audio_device =
        *new_device;
    request_ptr->SetAudioType(new_device->type);
  } else if (blink::IsVideoInputMediaType(new_device->type)) {
    request_ptr->stream_devices_set.stream_devices[0]->video_device =
        *new_device;
    request_ptr->SetVideoType(new_device->type);
  }

  // Device cloned in CloneExistingOpenDevice is ensured to have the state
  // MEDIA_REQUEST_STATE_DONE.
  // Set state to MEDIA_REQUEST_STATE_DONE as all processing specific to
  // new_device has been done.
  request_ptr->SetState(new_device->type, MEDIA_REQUEST_STATE_DONE);
  HandleRequestDone(new_label, request_ptr);
}

void MediaStreamManager::CancelRequest(int render_process_id,
                                       int render_frame_id,
                                       int requester_id,
                                       int page_request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (auto request_it = requests_.begin(); request_it != requests_.end();
       ++request_it) {
    const DeviceRequest* const request = request_it->second.get();
    if (request->requesting_process_id == render_process_id &&
        request->requesting_frame_id == render_frame_id &&
        request->requester_id == requester_id &&
        request->page_request_id == page_request_id) {
      CancelRequest(request_it);
      return;
    }
  }
}

void MediaStreamManager::CancelRequest(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const DeviceRequests::const_iterator request_it = FindRequestIterator(label);
  if (request_it == requests_.end()) {
    SendLogMessage(
        base::StringPrintf("CancelRequest({label=%s})", label.c_str()));
    LOG(ERROR) << "The request with label = " << label << " does not exist.";
    return;
  }

  CancelRequest(request_it);
}

void MediaStreamManager::CancelAllRequests(int render_process_id,
                                           int render_frame_id,
                                           int requester_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DeviceRequests::const_iterator request_it = requests_.begin();
  while (request_it != requests_.end()) {
    if (request_it->second->requesting_process_id != render_process_id ||
        request_it->second->requesting_frame_id != render_frame_id ||
        request_it->second->requester_id != requester_id) {
      ++request_it;
      continue;
    }

    const DeviceRequests::const_iterator next = std::next(request_it);
    CancelRequest(request_it);
    request_it = next;
  }
}

void MediaStreamManager::StopStreamDevice(
    int render_process_id,
    int render_frame_id,
    int requester_id,
    const std::string& device_id,
    const base::UnguessableToken& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(GetStopStreamDeviceLogString(
      render_process_id, render_frame_id, requester_id, device_id, session_id));

  // Find the first request for this |render_process_id| and |render_frame_id|
  // of type MEDIA_GENERATE_STREAM, MEDIA_DEVICE_UPDATE or MEDIA_GET_OPEN_DEVICE
  // that had requested to use device with Id |device_id| and sessionId
  // |session_id| and is now requesting to stop it.
  for (const LabeledDeviceRequest& device_request : requests_) {
    DeviceRequest* const request = device_request.second.get();
    if (request->requesting_process_id != render_process_id ||
        request->requesting_frame_id != render_frame_id ||
        request->requester_id != requester_id) {
      continue;
    }
    switch (request->request_type()) {
      case blink::MEDIA_DEVICE_ACCESS:
      case blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY:
        break;
      case blink::MEDIA_DEVICE_UPDATE:
      case blink::MEDIA_GENERATE_STREAM:
      case blink::MEDIA_GET_OPEN_DEVICE:
        for (const auto& stream_devices_ptr :
             request->stream_devices_set.stream_devices) {
          const blink::MediaStreamDevice* const device =
              GetStreamDevice(*stream_devices_ptr, session_id);
          if (!device || device->id != device_id) {
            continue;
          }

          if (request->IsTransferMapEmpty(device->type)) {
            // There are no ongoing transfers for this device.
            StopDevice(device->type, device->session_id());
          } else {
            request->SetShouldStopInFuture(device->type,
                                           /*should_be_stopped=*/true);
          }
          return;
        }
        break;
    }
  }
}

bool MediaStreamManager::KeepDeviceAliveForTransfer(
    int render_process_id,
    int render_frame_id,
    int requester_id,
    const base::UnguessableToken& session_id,
    const base::UnguessableToken& transfer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(base::FeatureList::IsEnabled(features::kMediaStreamTrackTransfer));

  for (const LabeledDeviceRequest& device_request : requests_) {
    DeviceRequest* const request = device_request.second.get();
    switch (request->request_type()) {
      case blink::MEDIA_DEVICE_ACCESS:
      case blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY:
        break;
      case blink::MEDIA_DEVICE_UPDATE:
      case blink::MEDIA_GENERATE_STREAM:
      case blink::MEDIA_GET_OPEN_DEVICE:
        for (const auto& stream_devices_ptr :
             request->stream_devices_set.stream_devices) {
          const blink::MediaStreamDevice* const device =
              GetStreamDevice(*stream_devices_ptr, session_id);
          if (!device) {
            continue;
          }

          UpdateDeviceTransferStatus(request, device, transfer_id,
                                     TransferState::KEPT_ALIVE);
          return true;
        }
        break;
    }
  }

  return false;
}

base::UnguessableToken MediaStreamManager::VideoDeviceIdToSessionId(
    const std::string& device_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const LabeledDeviceRequest& device_request : requests_) {
    for (const blink::mojom::StreamDevicesPtr& stream_devices_ptr :
         device_request.second->stream_devices_set.stream_devices) {
      const blink::mojom::StreamDevices& devices = *stream_devices_ptr;
      if (devices.video_device.has_value() &&
          devices.video_device->id == device_id &&
          devices.video_device->type == MediaStreamType::DEVICE_VIDEO_CAPTURE) {
        return devices.video_device->session_id();
      }
    }
  }
  return base::UnguessableToken();
}

void MediaStreamManager::StopDevice(MediaStreamType type,
                                    const base::UnguessableToken& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(base::StringPrintf("StopDevice({type=%s}, {session_id=%s})",
                                    StreamTypeToString(type),
                                    session_id.ToString().c_str()));
  DeviceRequests::const_iterator request_it = requests_.begin();
  while (request_it != requests_.end()) {
    DeviceRequest* request = request_it->second.get();

    if (request->stream_devices_set.stream_devices.empty()) {
      // There is no device in use yet by this request.
      ++request_it;
      continue;
    }

    auto stream_devices_set_iterator =
        request->stream_devices_set.stream_devices.begin();
    while (stream_devices_set_iterator !=
           request->stream_devices_set.stream_devices.end()) {
      blink::mojom::StreamDevicesPtr& stream_devices_ptr =
          *stream_devices_set_iterator;
      blink::mojom::StreamDevices& devices = *stream_devices_ptr;
      if (devices.audio_device.has_value() &&
          devices.audio_device->type == type &&
          devices.audio_device->session_id() == session_id) {
        if (request->state(type) == MEDIA_REQUEST_STATE_DONE) {
          CloseDevice(type, session_id);
        }
        devices.audio_device = absl::nullopt;
      }
      if (devices.video_device.has_value() &&
          devices.video_device->type == type &&
          devices.video_device->session_id() == session_id) {
        if (request->state(type) == MEDIA_REQUEST_STATE_DONE) {
          CloseDevice(type, session_id);
        }
        devices.video_device = absl::nullopt;
      }

      if (!devices.audio_device.has_value() &&
          !devices.video_device.has_value()) {
        stream_devices_set_iterator =
            request->stream_devices_set.stream_devices.erase(
                stream_devices_set_iterator);
      } else {
        ++stream_devices_set_iterator;
      }
    }

    // If this request doesn't have any active devices after a device
    // has been stopped above, remove the request. Note that the request is
    // only deleted if a device has been removed from |devices|.
    if (request->stream_devices_set.stream_devices.empty()) {
      const DeviceRequests::const_iterator next = std::next(request_it);
      DeleteRequest(request_it);
      request_it = next;
    } else {
      ++request_it;
    }
  }
}

void MediaStreamManager::CloseDevice(MediaStreamType type,
                                     const base::UnguessableToken& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(base::StringPrintf("CloseDevice({type=%s}, {session_id=%s})",
                                    StreamTypeToString(type),
                                    session_id.ToString().c_str()));
  GetDeviceManager(type)->Close(session_id);

  for (const LabeledDeviceRequest& labeled_request : requests_) {
    DeviceRequest* const request = labeled_request.second.get();
    for (const blink::mojom::StreamDevicesPtr& stream_devices_ptr :
         request->stream_devices_set.stream_devices) {
      const blink::mojom::StreamDevices& stream_devices = *stream_devices_ptr;
      for (const absl::optional<blink::MediaStreamDevice>* device_ptr : {
               &stream_devices.audio_device,
               &stream_devices.video_device,
           }) {
        if (!device_ptr->has_value()) {
          continue;
        }
        const blink::MediaStreamDevice& device = device_ptr->value();
        if (device.session_id() != session_id || device.type != type) {
          continue;
        }

        MaybeStopTrackingCaptureHandleConfig(labeled_request.first, device);
        // Notify observers that this device is being closed.
        // Note that only one device per type can be opened.
        request->SetState(type, MEDIA_REQUEST_STATE_CLOSING);
        // AudioInputDeviceManager does not have a mechanism to stop the audio
        // stream when the session is closed, while VideoCaptureManager does.
        // To ensure consistent behavior when sessions are closed, use the
        // stop callback to stop audio streams.
        if (blink::IsAudioInputMediaType(device.type) &&
            request->device_stopped_cb) {
          request->device_stopped_cb.Run(labeled_request.first, device);
        }
        if (request->ui_proxy) {
          const DesktopMediaID media_id = DesktopMediaID::Parse(device.id);
          if (!media_id.is_null()) {
            request->ui_proxy->OnDeviceStopped(labeled_request.first, media_id);
          }
        }
      }
    }
  }
}

void MediaStreamManager::OpenDevice(int render_process_id,
                                    int render_frame_id,
                                    int requester_id,
                                    int page_request_id,
                                    const std::string& device_id,
                                    MediaStreamType type,
                                    MediaDeviceSaltAndOrigin salt_and_origin,
                                    OpenDeviceCallback open_device_cb,
                                    DeviceStoppedCallback device_stopped_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(type == MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         type == MediaStreamType::DEVICE_VIDEO_CAPTURE);
  SendLogMessage(GetOpenDeviceLogString(render_process_id, render_frame_id,
                                        requester_id, page_request_id,
                                        device_id, type));
  StreamControls controls;
  if (blink::IsAudioInputMediaType(type)) {
    controls.audio.stream_type = type;
    controls.audio.device_id = device_id;
  } else if (blink::IsVideoInputMediaType(type)) {
    controls.video.stream_type = type;
    controls.video.device_id = device_id;
  } else {
    NOTREACHED();
  }
  // For pepper, we default to searching for a device always based on device ID,
  // independently of whether the request is for an audio or a video device.
  StreamSelectionInfoPtr audio_stream_selection_info_ptr =
      StreamSelectionInfo::New(
          blink::mojom::StreamSelectionStrategy::SEARCH_BY_DEVICE_ID,
          absl::nullopt);
  auto request = std::make_unique<OpenDeviceRequest>(
      render_process_id, render_frame_id, requester_id, page_request_id,
      std::move(audio_stream_selection_info_ptr), controls,
      std::move(salt_and_origin), std::move(device_stopped_cb),
      std::move(open_device_cb));
  const std::string& label = AddRequest(std::move(request))->first;

  // Post a task and handle the request asynchronously. The reason is that the
  // requester won't have a label for the request until this function returns
  // and thus can not handle a response. Using base::Unretained is safe since
  // MediaStreamManager is deleted on the UI thread, after the IO thread has
  // been stopped.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MediaStreamManager::SetUpRequest,
                                base::Unretained(this), label));
}

bool MediaStreamManager::TranslateSourceIdToDeviceIdAndGroupId(
    MediaStreamType stream_type,
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& source_id,
    std::string* device_id,
    absl::optional<std::string>* group_id) const {
  DCHECK(stream_type == MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         stream_type == MediaStreamType::DEVICE_VIDEO_CAPTURE);
  // The source_id can be empty if the constraint is set but empty.
  if (source_id.empty()) {
    return false;
  }

  // TODO(guidou): Change to use MediaDevicesManager::EnumerateDevices.
  // See http://crbug.com/648155.
  blink::WebMediaDeviceInfoArray cached_devices =
      media_devices_manager_->GetCachedDeviceInfo(
          ConvertToMediaDeviceType(stream_type));
  return GetDeviceIDAndGroupIDFromHMAC(salt, security_origin, source_id,
                                       cached_devices, device_id, group_id);
}

void MediaStreamManager::EnsureDeviceMonitorStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media_devices_manager_->StartMonitoring();
}

void MediaStreamManager::StopRemovedDevice(
    MediaDeviceType type,
    const blink::WebMediaDeviceInfo& media_device_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(type == MediaDeviceType::MEDIA_AUDIO_INPUT ||
         type == MediaDeviceType::MEDIA_VIDEO_INPUT);
  SendLogMessage(base::StringPrintf(
                     "StopRemovedDevice({type=%s}, {device=[id: %s, name: %s]}",
                     DeviceTypeToString(type),
                     media_device_info.device_id.c_str(),
                     media_device_info.label.c_str())
                     .c_str());

  MediaStreamType stream_type = ConvertToMediaStreamType(type);
  std::vector<base::UnguessableToken> session_ids;
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    const DeviceRequest* request = labeled_request.second.get();
    for (const blink::mojom::StreamDevicesPtr& stream_devices_ptr :
         request->stream_devices_set.stream_devices) {
      const blink::mojom::StreamDevices& stream_devices = *stream_devices_ptr;
      for (const absl::optional<blink::MediaStreamDevice>* device_ptr : {
               &stream_devices.audio_device,
               &stream_devices.video_device,
           }) {
        if (!device_ptr->has_value()) {
          continue;
        }
        const blink::MediaStreamDevice& device = device_ptr->value();
        const std::string source_id = GetHMACForMediaDeviceID(
            request->salt_and_origin.device_id_salt,
            request->salt_and_origin.origin, media_device_info.device_id);
        if (device.id == source_id && device.type == stream_type) {
          session_ids.push_back(device.session_id());
          if (request->device_stopped_cb) {
            request->device_stopped_cb.Run(labeled_request.first, device);
          }
        }
      }
    }
  }
  for (const auto& session_id : session_ids) {
    StopDevice(stream_type, session_id);
  }
}

bool MediaStreamManager::PickDeviceId(
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const TrackControls& controls,
    const blink::WebMediaDeviceInfoArray& devices,
    std::string* device_id) const {
  if (controls.device_id.empty()) {
    return true;
  }
  if (!GetDeviceIDAndGroupIDFromHMAC(
          salt_and_origin.device_id_salt, salt_and_origin.origin,
          controls.device_id, devices, device_id, /*group_id=*/nullptr)) {
    LOG(WARNING) << "Invalid device ID = " << controls.device_id;
    return false;
  }
  return true;
}

bool MediaStreamManager::GetRequestedDeviceCaptureId(
    const DeviceRequest* request,
    MediaStreamType type,
    const blink::WebMediaDeviceInfoArray& devices,
    std::string* device_id) const {
  if (type == MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    return PickDeviceId(request->salt_and_origin,
                        request->stream_controls().audio, devices, device_id);
  } else if (type == MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    return PickDeviceId(request->salt_and_origin,
                        request->stream_controls().video, devices, device_id);
  } else {
    NOTREACHED();
  }
  return false;
}

void MediaStreamManager::TranslateDeviceIdToSourceId(
    const DeviceRequest* request,
    MediaStreamDevice* device) const {
  if (request->audio_type() == MediaStreamType::DEVICE_AUDIO_CAPTURE ||
      request->video_type() == MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    device->id =
        GetHMACForMediaDeviceID(request->salt_and_origin.device_id_salt,
                                request->salt_and_origin.origin, device->id);
    if (device->group_id) {
      device->group_id = GetHMACForMediaDeviceID(
          request->salt_and_origin.group_id_salt,
          request->salt_and_origin.origin, *device->group_id);
    }
  }
}

void MediaStreamManager::StartEnumeration(DeviceRequest* request,
                                          const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(
      base::StringPrintf("StartEnumeration({requester_id=%d}, {label=%s})",
                         request->requester_id, label.c_str()));

  // Start monitoring the devices when doing the first enumeration.
  media_devices_manager_->StartMonitoring();

  // Start enumeration for devices of all requested device types.
  bool request_audio_input =
      request->audio_type() != MediaStreamType::NO_SERVICE;
  if (request_audio_input) {
    request->SetState(request->audio_type(), MEDIA_REQUEST_STATE_REQUESTED);
  }

  bool request_video_input =
      request->video_type() != MediaStreamType::NO_SERVICE;
  if (request_video_input) {
    request->SetState(request->video_type(), MEDIA_REQUEST_STATE_REQUESTED);
  }

  // base::Unretained is safe here because MediaStreamManager is deleted on the
  // UI thread, after the IO thread has been stopped.
  DCHECK(request_audio_input || request_video_input);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(
      MediaDeviceType::MEDIA_AUDIO_INPUT)] = request_audio_input;
  devices_to_enumerate[static_cast<size_t>(
      MediaDeviceType::MEDIA_VIDEO_INPUT)] = request_video_input;
  media_devices_manager_->EnumerateDevices(
      devices_to_enumerate,
      base::BindOnce(&MediaStreamManager::DevicesEnumerated,
                     base::Unretained(this), request_audio_input,
                     request_video_input, label));
}

MediaStreamManager::DeviceRequests::const_iterator
MediaStreamManager::AddRequest(std::unique_ptr<DeviceRequest> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Create a label for this request and verify it is unique.
  std::string unique_label;
  do {
    unique_label = base::Uuid::GenerateRandomV4().AsLowercaseString();
  } while (FindRequest(unique_label) != nullptr);

  SendLogMessage(
      base::StringPrintf("AddRequest([requester_id=%d]) => (label=%s)",
                         request->requester_id, unique_label.c_str()));
  request->SetLabel(unique_label);
  requests_.push_back(std::make_pair(unique_label, std::move(request)));

  return std::prev(requests_.end());
}

MediaStreamManager::DeviceRequests::const_iterator
MediaStreamManager::FindRequestIterator(const std::string& label) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return base::ranges::find(requests_, label, &LabeledDeviceRequest::first);
}

MediaStreamManager::DeviceRequest* MediaStreamManager::FindRequest(
    const std::string& label) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  MediaStreamManager::DeviceRequests::const_iterator it =
      FindRequestIterator(label);
  return (it != requests_.end()) ? it->second.get() : nullptr;
}

absl::optional<MediaStreamDevice> MediaStreamManager::CloneExistingOpenDevice(
    const base::UnguessableToken& existing_device_session_id,
    const base::UnguessableToken& transfer_id,
    const std::string& new_label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DeviceRequest* const new_request = FindRequest(new_label);
  DCHECK(new_request);
  // TODO(crbug.com/1334583): Generalize to multiple streams.
  DCHECK(new_request->stream_devices_set.stream_devices.empty());
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    DeviceRequest* const existing_request = labeled_request.second.get();
    // Skipping requests that contain multiple streams.
    // TODO(crbug.com/1334583): Generalize to multiple streams.
    if (existing_request->stream_devices_set.stream_devices.size() > 1u) {
      continue;
    }
    for (const auto& stream_devices_ptr :
         existing_request->stream_devices_set.stream_devices) {
      const blink::MediaStreamDevice* const existing_device =
          GetStreamDevice(*stream_devices_ptr, existing_device_session_id);

      if (!existing_device) {
        continue;
      }
      if (existing_request->state(existing_device->type) !=
          MEDIA_REQUEST_STATE_DONE) {
        // TODO(https://crbug.com/1288839): Ensure state of MediaStreamDevice
        // doesn't change while MediaStreamTrack is being transferred.
        // Skip devices not in state MEDIA_REQUEST_STATE_DONE.
        continue;
      }

      MediaStreamDevice new_device = *existing_device;
      if (!blink::IsMediaStreamDeviceTransferrable(*existing_device)) {
        // TODO(https://crbug.com/1288839): Remove bad message after transfer
        // is supported for these stream types.
        // TODO(https://crbug.com/1288839): Hash device id and group_id for
        // MediaStreamType DEVICE_AUDIO_CAPTURE and DEVICE_VIDEO_CAPTURE.
        ReceivedBadMessage(
            new_request->requesting_process_id,
            bad_message::MSM_GET_OPEN_DEVICE_FOR_UNSUPPORTED_STREAM_TYPE);
        return absl::nullopt;
      }

      new_device.set_session_id(
          GetDeviceManager(new_device.type)->Open(new_device));
      UpdateDeviceTransferStatus(existing_request, existing_device, transfer_id,
                                 TransferState::GOT_OPEN_DEVICE);
      return new_device;
    }
  }
  return absl::nullopt;
}

void MediaStreamManager::UpdateDeviceTransferStatus(
    DeviceRequest* request,
    const blink::MediaStreamDevice* const device,
    const base::UnguessableToken& transfer_id,
    TransferState transfer_state) {
  // TODO(https://crbug.com/1288839): Use |start_time| to enforce a timeout to
  // stop device in case a transfer never completes.
  MediaStreamType stream_type = device->type;
  absl::optional<TransferState> existing_state =
      request->GetTransferState(stream_type, transfer_id);
  if (!existing_state) {
    request->SetTransferState(stream_type, transfer_id, transfer_state);
    return;
  }

  if (existing_state.value() != transfer_state) {
    // If the new |transfer_state| is different from the existing state in
    // |transfer_map|, this entry can be removed. This is because reaching here
    // implies both states, KEPT_ALIVE and GOT_OPEN_DEVICE, have been achieved,
    // which in turn means both the original and transferred renderer have
    // finished their execution with regards to transferring of this |device|.
    request->RemoveEntryInTransferMap(stream_type, transfer_id);
  }

  if (request->IsTransferMapEmpty(stream_type) &&
      request->ShouldStopInFuture(stream_type)) {
    StopDevice(stream_type, device->session_id());
  }
}

void MediaStreamManager::CancelRequest(
    DeviceRequests::const_iterator request_it) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (request_it == requests_.end()) {
    return;
  }
  const std::string& label = request_it->first;
  DeviceRequest* const request = request_it->second.get();

  SendLogMessage(
      base::StringPrintf("CancelRequest({label=%s})", label.c_str()));

  // This is a request for closing one or more devices.
  for (const blink::mojom::StreamDevicesPtr& stream_devices_ptr :
       request->stream_devices_set.stream_devices) {
    const blink::mojom::StreamDevices& stream_devices = *stream_devices_ptr;
    for (const absl::optional<blink::MediaStreamDevice>* device_ptr : {
             &stream_devices.audio_device,
             &stream_devices.video_device,
         }) {
      if (!device_ptr->has_value()) {
        continue;
      }
      const blink::MediaStreamDevice& device = device_ptr->value();
      const MediaRequestState state = request->state(device.type);
      // If we have not yet requested the device to be opened - just ignore it.
      if (state != MEDIA_REQUEST_STATE_OPENING &&
          state != MEDIA_REQUEST_STATE_DONE) {
        continue;
      }
      // Stop the opening/opened devices of the requests.
      CloseDevice(device.type, device.session_id());
    }
  }

  // Cancel the request if still pending at UI side.
  request->SetState(MediaStreamType::NUM_MEDIA_TYPES,
                    MEDIA_REQUEST_STATE_CLOSING);
  DeleteRequest(request_it);  // Invalidates |label| and |request|.
}

void MediaStreamManager::DeleteRequest(
    DeviceRequests::const_iterator request_it) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request_it != requests_.end());

  SendLogMessage(base::StringPrintf("DeleteRequest([label=%s])",
                                    request_it->first.c_str()));
#if BUILDFLAG(IS_CHROMEOS)
  if (request_it->second->IsGetDisplayMediaSet()) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(NotifyMultiCaptureStopped, request_it->first));
  }
#endif

  // Clean up permission controller subscription.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaStreamManager::UnsubscribeFromPermissionControllerOnUIThread,
          request_it->second->requesting_process_id,
          request_it->second->requesting_frame_id,
          request_it->second->audio_subscription_id,
          request_it->second->video_subscription_id));

  requests_.erase(request_it);
}

void MediaStreamManager::ReadOutputParamsAndPostRequestToUI(
    const std::string& label,
    DeviceRequest* request,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Actual audio parameters are required only for
  // MEDIA_GUM_TAB_AUDIO_CAPTURE.
  if (request->audio_type() == MediaStreamType::GUM_TAB_AUDIO_CAPTURE) {
    // Using base::Unretained is safe: |audio_system_| will post
    // PostRequestToUI() to IO thread, and MediaStreamManager is deleted on the
    // UI thread, after the IO thread has been stopped.
    audio_system_->GetOutputStreamParameters(
        media::AudioDeviceDescription::kDefaultDeviceId,
        base::BindOnce(&MediaStreamManager::PostRequestToUI,
                       base::Unretained(this), label, enumeration));
  } else {
    PostRequestToUI(label, enumeration,
                    absl::optional<media::AudioParameters>());
  }
}

void MediaStreamManager::PostRequestToUI(
    const std::string& label,
    const MediaDeviceEnumeration& enumeration,
    const absl::optional<media::AudioParameters>& output_parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!output_parameters || output_parameters->IsValid());
  DeviceRequest* request = FindRequest(label);
  if (!request) {
    return;
  }
  DCHECK(request->HasUIRequest());
  SendLogMessage(
      base::StringPrintf("PostRequestToUI({label=%s}, ", label.c_str()));

  const MediaStreamType audio_type = request->audio_type();
  const MediaStreamType video_type = request->video_type();

  // Post the request to UI and set the state.
  if (blink::IsAudioInputMediaType(audio_type)) {
    request->SetState(audio_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);
  }
  if (blink::IsVideoInputMediaType(video_type)) {
    request->SetState(video_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);
  }

  if (ShouldUseFakeUIProxy(*request)) {
    request->ui_proxy = MakeFakeUIProxy(label, enumeration, request);
  } else if (!request->ui_proxy) {
    request->ui_proxy = MediaStreamUIProxy::Create();
  }

  request->ui_proxy->RequestAccess(
      request->DetachUIRequest(),
      base::BindOnce(&MediaStreamManager::HandleAccessRequestResponse,
                     base::Unretained(this), label,
                     output_parameters.value_or(
                         media::AudioParameters::UnavailableDeviceParams())));
}

void MediaStreamManager::SetUpRequest(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const DeviceRequests::const_iterator request_it = FindRequestIterator(label);
  if (request_it == requests_.end()) {
    DVLOG(1) << "SetUpRequest label " << label << " doesn't exist!!";
    return;  // This can happen if the request has been canceled.
  }
  DeviceRequest* const request = request_it->second.get();

  SendLogMessage(
      base::StringPrintf("SetUpRequest([requester_id=%d] {label=%s})",
                         request->requester_id, label.c_str()));

  request->SetAudioType(request->stream_controls().audio.stream_type);
  request->SetVideoType(request->stream_controls().video.stream_type);

  const bool is_display_capture =
      request->video_type() == MediaStreamType::DISPLAY_VIDEO_CAPTURE ||
      request->video_type() ==
          MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB ||
      request->video_type() == MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET;
  if (is_display_capture && !SetUpDisplayCaptureRequest(request)) {
    FinalizeRequestFailed(request_it,
                          MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE);
    return;
  }

  const bool is_tab_capture =
      request->audio_type() == MediaStreamType::GUM_TAB_AUDIO_CAPTURE ||
      request->video_type() == MediaStreamType::GUM_TAB_VIDEO_CAPTURE;
  if (is_tab_capture) {
    if (!SetUpTabCaptureRequest(request, label)) {
      FinalizeRequestFailed(request_it,
                            MediaStreamRequestResult::TAB_CAPTURE_FAILURE);
    }
    return;
  }

  const bool is_screen_capture =
      request->video_type() == MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE;
  if (is_screen_capture && !SetUpScreenCaptureRequest(request)) {
    FinalizeRequestFailed(request_it,
                          MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE);
    return;
  }

  if (!is_tab_capture && !is_screen_capture && !is_display_capture) {
    if (blink::IsDeviceMediaType(request->audio_type()) ||
        blink::IsDeviceMediaType(request->video_type())) {
      StartEnumeration(request, label);
      return;
    }
    // If no actual device capture is requested, set up the request with an
    // empty device list.
    if (!SetUpDeviceCaptureRequest(request, MediaDeviceEnumeration())) {
      FinalizeRequestFailed(request_it, MediaStreamRequestResult::NO_HARDWARE);
      return;
    }
  }

  if (request->stream_controls().request_all_screens) {
    std::unique_ptr<media::ScreenEnumerator> screen_enumerator =
        GetContentClient()->browser()->CreateScreenEnumerator();
    if (!screen_enumerator) {
      HandleAccessRequestResponse(
          label, media::AudioParameters(), blink::mojom::StreamDevicesSet(),
          blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED);
      return;
    }

    // The screen enumerator lives on the IO thread.
    // It is safe to bind base::Unretained(this) because MediaStreamManager is
    // owned by BrowserMainLoop.
    screen_enumerator->EnumerateScreens(
        request->video_type(),
        base::BindOnce(&MediaStreamManager::HandleAccessRequestResponse,
                       base::Unretained(this), label,
                       media::AudioParameters()));
    return;
  }

  ReadOutputParamsAndPostRequestToUI(label, request, MediaDeviceEnumeration());
}

bool MediaStreamManager::SetUpDisplayCaptureRequest(DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request->video_type() == MediaStreamType::DISPLAY_VIDEO_CAPTURE ||
         request->video_type() ==
             MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB ||
         request->video_type() == MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET);

  // getDisplayMedia function does not permit the use of constraints for
  // selection of a source, see
  // https://w3c.github.io/mediacapture-screen-share/#constraints.
  if (!request->stream_controls().video.requested() ||
      !request->stream_controls().video.device_id.empty() ||
      !request->stream_controls().audio.device_id.empty()) {
    LOG(ERROR) << "Invalid display media request.";
    return false;
  }

  request->CreateUIRequest(std::string() /* requested_audio_device_id */,
                           std::string() /* requested_video_device_id */);
  DVLOG(3) << "Audio requested " << request->stream_controls().audio.requested()
           << " Video requested "
           << request->stream_controls().video.requested();
  return true;
}

bool MediaStreamManager::SetUpDeviceCaptureRequest(
    DeviceRequest* request,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK((request->audio_type() == MediaStreamType::DEVICE_AUDIO_CAPTURE ||
          request->audio_type() == MediaStreamType::NO_SERVICE) &&
         (request->video_type() == MediaStreamType::DEVICE_VIDEO_CAPTURE ||
          request->video_type() == MediaStreamType::NO_SERVICE));
  SendLogMessage(base::StringPrintf(
      "SetUpDeviceCaptureRequest([requester_id=%d])", request->requester_id));
  std::string audio_device_id;
  if (request->stream_controls().audio.requested() &&
      !GetRequestedDeviceCaptureId(
          request, request->audio_type(),
          enumeration[static_cast<size_t>(MediaDeviceType::MEDIA_AUDIO_INPUT)],
          &audio_device_id)) {
    return false;
  }

  std::string video_device_id;
  if (request->stream_controls().video.requested() &&
      !GetRequestedDeviceCaptureId(
          request, request->video_type(),
          enumeration[static_cast<size_t>(MediaDeviceType::MEDIA_VIDEO_INPUT)],
          &video_device_id)) {
    return false;
  }
  request->CreateUIRequest(audio_device_id, video_device_id);
  DVLOG(3) << "Audio requested " << request->stream_controls().audio.requested()
           << " device id = " << audio_device_id << "Video requested "
           << request->stream_controls().video.requested()
           << " device id = " << video_device_id;
  return true;
}

bool MediaStreamManager::SetUpTabCaptureRequest(DeviceRequest* request,
                                                const std::string& label) {
  DCHECK(request->audio_type() == MediaStreamType::GUM_TAB_AUDIO_CAPTURE ||
         request->video_type() == MediaStreamType::GUM_TAB_VIDEO_CAPTURE);

  std::string capture_device_id;
  if (!request->stream_controls().audio.device_id.empty()) {
    capture_device_id = request->stream_controls().audio.device_id;
  } else if (!request->stream_controls().video.device_id.empty()) {
    capture_device_id = request->stream_controls().video.device_id;
  } else {
    return false;
  }

  if ((request->audio_type() != MediaStreamType::GUM_TAB_AUDIO_CAPTURE &&
       request->audio_type() != MediaStreamType::NO_SERVICE) ||
      (request->video_type() != MediaStreamType::GUM_TAB_VIDEO_CAPTURE &&
       request->video_type() != MediaStreamType::NO_SERVICE)) {
    return false;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MediaStreamManager::ResolveTabCaptureDeviceIdOnUIThread,
                     base::Unretained(this), capture_device_id,
                     request->requesting_process_id,
                     request->requesting_frame_id,
                     request->salt_and_origin.origin.GetURL()),
      base::BindOnce(
          &MediaStreamManager::FinishTabCaptureRequestSetupWithDeviceId,
          base::Unretained(this), label));
  return true;
}

DesktopMediaID MediaStreamManager::ResolveTabCaptureDeviceIdOnUIThread(
    const std::string& capture_device_id,
    int requesting_process_id,
    int requesting_frame_id,
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Resolve DesktopMediaID for the specified device id.
  return DesktopStreamsRegistry::GetInstance()->RequestMediaForStreamId(
      capture_device_id, requesting_process_id, requesting_frame_id,
      url::Origin::Create(origin), nullptr, kRegistryStreamTypeTab);
}

void MediaStreamManager::FinishTabCaptureRequestSetupWithDeviceId(
    const std::string& label,
    const DesktopMediaID& device_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const DeviceRequests::const_iterator request_it = FindRequestIterator(label);
  if (request_it == requests_.end()) {
    DVLOG(1) << "SetUpRequest label " << label << " doesn't exist!!";
    return;  // This can happen if the request has been canceled.
  }
  DeviceRequest* const request = request_it->second.get();

  // Received invalid device id.
  if (device_id.type != content::DesktopMediaID::TYPE_WEB_CONTENTS) {
    FinalizeRequestFailed(request_it,
                          MediaStreamRequestResult::TAB_CAPTURE_FAILURE);
    return;
  }

  content::WebContentsMediaCaptureId web_id = device_id.web_contents_id;
  web_id.disable_local_echo = request->stream_controls().disable_local_echo;

  request->tab_capture_device_id = web_id.ToString();

  request->CreateTabCaptureUIRequest(web_id.render_process_id,
                                     web_id.main_render_frame_id);

  DVLOG(3) << "SetUpTabCaptureRequest "
           << ", {capture_device_id = " << web_id.ToString() << "}"
           << ", {target_render_process_id = " << web_id.render_process_id
           << "}"
           << ", {target_render_frame_id = " << web_id.main_render_frame_id
           << "}";

  ReadOutputParamsAndPostRequestToUI(label, request, MediaDeviceEnumeration());
}

bool MediaStreamManager::SetUpScreenCaptureRequest(DeviceRequest* request) {
  DCHECK(request->audio_type() == MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE ||
         request->video_type() == MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE);

  // For screen capture we only support two valid combinations:
  // (1) screen video capture only, or
  // (2) screen video capture with loopback audio capture.
  if (request->video_type() != MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE ||
      (request->audio_type() != MediaStreamType::NO_SERVICE &&
       request->audio_type() != MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE)) {
    LOG(ERROR) << "Invalid screen capture request.";
    return false;
  }

  std::string video_device_id;
  if (request->video_type() == MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE &&
      !request->stream_controls().video.device_id.empty()) {
    video_device_id = request->stream_controls().video.device_id;
  }

  const std::string audio_device_id =
      request->audio_type() == MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE
          ? video_device_id
          : "";

  request->CreateUIRequest(audio_device_id, video_device_id);
  return true;
}

void MediaStreamManager::SetUpDesktopCaptureChangeSourceRequest(
    DeviceRequest* request,
    const std::string& label,
    const DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(blink::IsDesktopCaptureMediaType(request->video_type()));
  DCHECK(request->request_type() == blink::MEDIA_GENERATE_STREAM ||
         request->request_type() == blink::MEDIA_DEVICE_UPDATE);

  // Set up request type to bring up the picker again within a session.
  request->set_request_type(blink::MEDIA_DEVICE_UPDATE);

  request->CreateUIRequest(
      std::string() /* requested_audio_device_id */,
      media_id.is_null() ? std::string()
                         : media_id.ToString() /* requested_video_device_id */);

  ReadOutputParamsAndPostRequestToUI(label, request, MediaDeviceEnumeration());
}

MediaStreamDevices MediaStreamManager::GetDevicesOpenedByRequest(
    const std::string& label) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeviceRequest* request = FindRequest(label);
  if (!request) {
    return MediaStreamDevices();
  }
  return blink::ToMediaStreamDevicesList(request->stream_devices_set);
}

bool MediaStreamManager::FindExistingRequestedDevice(
    const DeviceRequest& new_request,
    const MediaStreamDevice& new_device,
    MediaStreamDevice* existing_device,
    MediaRequestState* existing_request_state) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(existing_device);
  DCHECK(existing_request_state);

  std::string hashed_source_id = GetHMACForMediaDeviceID(
      new_request.salt_and_origin.device_id_salt,
      new_request.salt_and_origin.origin, new_device.id);

  bool is_audio_capture =
      new_device.type == MediaStreamType::DEVICE_AUDIO_CAPTURE &&
      new_request.audio_type() == MediaStreamType::DEVICE_AUDIO_CAPTURE;
  StreamSelectionStrategy strategy =
      new_request.audio_stream_selection_info_ptr->strategy;
  if (is_audio_capture &&
      strategy == blink::mojom::StreamSelectionStrategy::FORCE_NEW_STREAM) {
    return false;
  }

  absl::optional<base::UnguessableToken> requested_session_id =
      new_request.audio_stream_selection_info_ptr->session_id;
#if DCHECK_IS_ON()
  if (strategy == StreamSelectionStrategy::SEARCH_BY_SESSION_ID) {
    DCHECK(requested_session_id);
    DCHECK(!requested_session_id->is_empty());
  }
#endif

  for (const LabeledDeviceRequest& labeled_request : requests_) {
    const DeviceRequest* request = labeled_request.second.get();
    for (const blink::mojom::StreamDevicesPtr& stream_devices_ptr :
         request->stream_devices_set.stream_devices) {
      const blink::mojom::StreamDevices& stream_devices = *stream_devices_ptr;
      if (request->requesting_process_id == new_request.requesting_process_id &&
          request->requesting_frame_id == new_request.requesting_frame_id &&
          request->request_type() == new_request.request_type()) {
        for (const absl::optional<blink::MediaStreamDevice>* device_ptr : {
                 &stream_devices.audio_device,
                 &stream_devices.video_device,
             }) {
          if (!device_ptr->has_value()) {
            continue;
          }
          const blink::MediaStreamDevice& device = device_ptr->value();
          const bool is_same_device =
              device.id == hashed_source_id && device.type == new_device.type;
          // If |strategy| is equal to SEARCH_BY_DEVICE_ID, the
          // search is performed only based on the |device.id|. If, however,
          // |strategy| is equal to SEARCH_BY_SESSION_ID, the
          // search also includes the session ID provided in the request.
          // NB: this only applies to audio. In case of media stream types that
          // are not an audio capture, the session id is always ignored.
          const bool is_same_session =
              !is_audio_capture ||
              strategy == StreamSelectionStrategy::SEARCH_BY_DEVICE_ID ||
              (strategy == StreamSelectionStrategy::SEARCH_BY_SESSION_ID &&
               device.session_id() == *requested_session_id);

          if (is_same_device && is_same_session) {
            *existing_device = device;
            // Make sure that the audio |effects| reflect what the request
            // is set to and not what the capabilities are.
            int effects = existing_device->input.effects();
            FilterAudioEffects(request->stream_controls(), &effects);
            EnableHotwordEffect(request->stream_controls(), &effects);
            existing_device->input.set_effects(effects);
            *existing_request_state = request->state(device.type);
            return true;
          }
        }
      }
    }
  }
  return false;
}

void MediaStreamManager::FinalizeGenerateStreams(const std::string& label,
                                                 DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request);
  DCHECK_EQ(request->request_type(), blink::MEDIA_GENERATE_STREAM);
  SendLogMessage(
      base::StringPrintf("FinalizeGenerateStreams({label=%s}, {requester_id="
                         "%d}, {request_type=%s})",
                         label.c_str(), request->requester_id,
                         RequestTypeToString(request->request_type())));

  // Subscribe to follow permission changes in order to close streams when the
  // user denies mic/camera.
  SubscribeToPermissionController(label, request);

  blink::mojom::StreamDevicesSetPtr stream_devices_set =
      request->stream_devices_set.Clone();

  if (request->IsGetDisplayMediaSet()) {
    PanTiltZoomPermissionChecked(label, MediaStreamDevice(),
                                 /*pan_tilt_zoom_allowed=*/false);
    return;
  }

  // TODO(crbug.com/1300883): Generalize to multiple streams.
  DCHECK_EQ(1u, request->stream_devices_set.stream_devices.size());

  // It is safe to bind base::Unretained(this) because MediaStreamManager is
  // owned by BrowserMainLoop and so outlives the IO thread.
  // TODO(crbug.com/1314741): Avoid using PTZ permission checks for non-gUM
  // tracks.
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MediaDevicesPermissionChecker::
                         HasPanTiltZoomPermissionGrantedOnUIThread,
                     request->requesting_process_id,
                     request->requesting_frame_id),
      base::BindOnce(
          &MediaStreamManager::PanTiltZoomPermissionChecked,
          base::Unretained(this), label,
          request->stream_devices_set.stream_devices[0]->video_device));
}

void MediaStreamManager::FinalizeGetOpenDevice(const std::string& label,
                                               DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request);
  DCHECK_EQ(request->request_type(), blink::MEDIA_GET_OPEN_DEVICE);
  SendLogMessage(
      base::StringPrintf("FinalizeGetOpenDevice({label=%s}, {requester_id="
                         "%d}, {request_type=%s})",
                         label.c_str(), request->requester_id,
                         RequestTypeToString(request->request_type())));

  // Subscribe to follow permission changes in order to close streams when the
  // user denies mic/camera.
  SubscribeToPermissionController(label, request);

  // It is safe to bind base::Unretained(this) because MediaStreamManager is
  // owned by BrowserMainLoop and so outlives the IO thread.
  // TODO(crbug.com/1314743): Avoid this check once you have this permission
  // value from original context.
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MediaDevicesPermissionChecker::
                         HasPanTiltZoomPermissionGrantedOnUIThread,
                     request->requesting_process_id,
                     request->requesting_frame_id),
      base::BindOnce(
          &MediaStreamManager::PanTiltZoomPermissionChecked,
          base::Unretained(this), label,
          request->stream_devices_set.stream_devices[0]->video_device));
}

// TODO(https://crbug.com/1288839): Ensure CaptureHandle works for transferred
// MediaStreamTracks and add tests for the same.
// TODO(https://crbug.com/1314634): Ensure track transfer does not initiate
// focus-change with Conditional focus enabled.
void MediaStreamManager::PanTiltZoomPermissionChecked(
    const std::string& label,
    const absl::optional<MediaStreamDevice>& video_device,
    bool pan_tilt_zoom_allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeviceRequest* request = FindRequest(label);
  if (!request) {
    return;
  }

  SendLogMessage(base::StringPrintf(
      "PanTiltZoomPermissionChecked({label=%s}, {requester_id="
      "%d}, {request_type=%s}, {pan_tilt_zoom_allowed=%d})",
      label.c_str(), request->requester_id,
      RequestTypeToString(request->request_type()), pan_tilt_zoom_allowed));

  request->PanTiltZoomPermissionChecked(label, pan_tilt_zoom_allowed);

  if (request->IsGetDisplayMediaSet()) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  // 1. Only the first call to SetCapturedDisplaySurfaceFocus() has an
  //    effect, so a direct call to SetCapturedDisplaySurfaceFocus()
  //    before the scheduled task is executed would render the scheduled
  //    task ineffectual (by design).
  //    If conditional-focus is enabled in Blink, the application might
  //    suppress this focus-change by calling focus(false). Otherwise,
  //    either this following task changes focus in 1s, or the microtask
  //    that Blink schedules does so even sooner.
  // 2. Using base::Unretained is safe since MediaStreamManager is deleted on
  //    the UI thread, after the IO thread has been stopped.
  GetIOThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MediaStreamManager::SetCapturedDisplaySurfaceFocus,
                     base::Unretained(this), label, /*focus=*/true,
                     /*is_from_microtask=*/false,
                     /*is_from_timer=*/true),
      conditional_focus_window_);
#endif

  // We only start tracking once stream generation is truly complete.
  // If the CaptureHandle observable by this capturer has changed asynchronously
  // while the current task was hopping between threads/queues, an event will
  // be fired by the CaptureHandleManager.
  if (video_device.has_value()) {
    MaybeStartTrackingCaptureHandleConfig(label, video_device.value(),
                                          *request);
  }
}

void MediaStreamManager::FinalizeRequestFailed(
    DeviceRequests::const_iterator request_it,
    MediaStreamRequestResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request_it != requests_.end());

  DeviceRequest* const request = request_it->second.get();

  SendLogMessage(base::StringPrintf(
      "FinalizeRequestFailed({label=%s}, {requester_id=%d}, {result=%s})",
      request_it->first.c_str(), request->requester_id,
      RequestResultToString(result)));

  switch (request->request_type()) {
    case blink::MEDIA_DEVICE_ACCESS:
    case blink::MEDIA_GENERATE_STREAM:
    case blink::MEDIA_GET_OPEN_DEVICE:
    case blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY: {
      request->FinalizeRequestFailed(result);
      break;
    }
    case blink::MEDIA_DEVICE_UPDATE: {
      // Fail to change capture source, keep everything unchanged and
      // bring the previous shared tab to the front.
      DCHECK_EQ(1u, request->stream_devices_set.stream_devices.size());
      const blink::mojom::StreamDevices& devices =
          *request->stream_devices_set.stream_devices[0];
      if (devices.video_device.has_value()) {
        const blink::MediaStreamDevice& device = devices.video_device.value();
        DCHECK_NE(device.type, MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET);
        // TODO(crbug.com/1334332): Also consider
        // DISPLAY_VIDEO_CAPTURE_THIS_TAB.
        if (device.type == MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE ||
            device.type == MediaStreamType::DISPLAY_VIDEO_CAPTURE) {
          DesktopMediaID source = DesktopMediaID::Parse(device.id);
          DCHECK(source.type == DesktopMediaID::TYPE_WEB_CONTENTS);
          GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE,
              base::BindOnce(&MediaStreamManager::ActivateTabOnUIThread,
                             base::Unretained(this), source));
        }
      }
      return;
    }
    default:
      NOTREACHED();
      break;
  }

  DeleteRequest(request_it);
}

void MediaStreamManager::FinalizeChangeDevice(const std::string& label,
                                              DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request);

  request->FinalizeChangeDevice(label);

  MaybeUpdateTrackedCaptureHandleConfigs(label, request->stream_devices_set,
                                         *request);
}

void MediaStreamManager::FinalizeMediaAccessRequest(
    DeviceRequests::const_iterator request_it,
    const blink::mojom::StreamDevicesSet& stream_devices_set) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request_it != requests_.end());
  DeviceRequest* const request = request_it->second.get();

  request->FinalizeMediaAccessRequest(request_it->first, stream_devices_set);

  // Delete the request since it is done.
  DeleteRequest(request_it);
}

void MediaStreamManager::SetRequestDevice(
    blink::mojom::StreamDevices& target_devices,
    const blink::MediaStreamDevice& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (blink::IsAudioInputMediaType(device.type)) {
    target_devices.audio_device = device;
  } else {
    DCHECK(blink::IsVideoInputMediaType(device.type));
    target_devices.video_device = device;
  }
}

void MediaStreamManager::InitializeMaybeAsync(
    std::unique_ptr<VideoCaptureProvider> video_capture_provider) {
  // Some unit tests initialize the MSM in the IO thread and assume the
  // initialization is done synchronously. Other clients call this from a
  // different thread and expect initialization to run asynchronously.
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&MediaStreamManager::InitializeMaybeAsync,
                                  base::Unretained(this),
                                  std::move(video_capture_provider)));
    return;
  }
  SendLogMessage(base::StringPrintf("InitializeMaybeAsync([this=%p])", this));

  // Store a pointer to |this| on the IO thread to avoid having to jump to
  // the UI thread to fetch a pointer to the MSM. In particular on Android,
  // it can be problematic to post to a UI thread from arbitrary worker
  // threads since attaching to the VM is required and we may have to access
  // the MSM from callback threads that we don't own and don't want to
  // attach.
  media_stream_manager = this;

  audio_input_device_manager_ =
      base::MakeRefCounted<AudioInputDeviceManager>(audio_system_);
  audio_input_device_manager_->RegisterListener(this);

  // We want to be notified of IO message loop destruction to delete the thread
  // and the device managers.
  base::CurrentThread::Get()->AddDestructionObserver(this);

  video_capture_manager_ = base::MakeRefCounted<VideoCaptureManager>(
      std::move(video_capture_provider),
      base::BindRepeating(&SendVideoCaptureLogMessage));
  video_capture_manager_->RegisterListener(this);

  // Using base::Unretained(this) is safe because |this| owns and therefore
  // outlives |media_devices_manager_|.
  media_devices_manager_ = std::make_unique<MediaDevicesManager>(
      audio_system_, video_capture_manager_,
      base::BindRepeating(&MediaStreamManager::StopRemovedDevice,
                          base::Unretained(this)),
      base::BindRepeating(&MediaStreamManager::NotifyDevicesChanged,
                          base::Unretained(this)));
}

void MediaStreamManager::Opened(
    MediaStreamType stream_type,
    const base::UnguessableToken& capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(base::StringPrintf("Opened({stream_type=%s}, {session_id=%s})",
                                    StreamTypeToString(stream_type),
                                    capture_session_id.ToString().c_str()));

  // Find the request(s) containing this device and mark it as used.
  // It can be used in several requests since the same device can be
  // requested from the same web page.
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    const std::string& label = labeled_request.first;
    DeviceRequest* request = labeled_request.second.get();

    if (request->stream_devices_set.stream_devices.empty()) {
      continue;
    }

    // It can happen that a previous stream already failed and set an error,
    // in which case this streams request does not need to be handled further.
    if (request->state(stream_type) == MEDIA_REQUEST_STATE_ERROR) {
      continue;
    }

    for (blink::mojom::StreamDevicesPtr& stream_devices_ptr :
         request->stream_devices_set.stream_devices) {
      blink::mojom::StreamDevices& stream_devices = *stream_devices_ptr;
      for (absl::optional<blink::MediaStreamDevice>* device_ptr : {
               &stream_devices.audio_device,
               &stream_devices.video_device,
           }) {
        if (!device_ptr->has_value()) {
          continue;
        }

        blink::MediaStreamDevice& device = device_ptr->value();
        if (device.type == stream_type &&
            device.session_id() == capture_session_id) {
          if (request->state(device.type) == MEDIA_REQUEST_STATE_DONE) {
            continue;
          }

          // We've found a matching request.
          CHECK_EQ(request->state(device.type), MEDIA_REQUEST_STATE_OPENING);
          request->SetDeviceOpened(device.type);
          if (request->devices_opened_count(device.type) ==
              request->stream_devices_set.stream_devices.size()) {
            request->SetState(device.type, MEDIA_REQUEST_STATE_DONE);
            request->ResetDevicesOpened(device.type);
          }

          if (blink::IsAudioInputMediaType(device.type)) {
            // Store the native audio parameters in the device struct.
            // TODO(xians): Handle the tab capture sample rate/channel layout
            // in AudioInputDeviceManager::Open().
            if (device.type != MediaStreamType::GUM_TAB_AUDIO_CAPTURE) {
              const MediaStreamDevice* opened_device =
                  audio_input_device_manager_->GetOpenedDeviceById(
                      device.session_id());
              device.input = opened_device->input;

              // Since the audio input device manager will set the input
              // parameters to the default settings (including supported
              // effects), we need to adjust those settings here according to
              // what the request asks for.
              int effects = device.input.effects();
              FilterAudioEffects(request->stream_controls(), &effects);
              EnableHotwordEffect(request->stream_controls(), &effects);
              device.input.set_effects(effects);
            }
          }
          if (RequestDone(*request)) {
            HandleRequestDone(label, request);
          }
          break;
        }
      }
    }
  }
}

void MediaStreamManager::HandleRequestDone(const std::string& label,
                                           DeviceRequest* request) {
  DCHECK(RequestDone(*request));

  switch (request->request_type()) {
    case blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY:
      request->FinalizeRequest(label);
      OnStreamStarted(label);
      break;
    case blink::MEDIA_GENERATE_STREAM: {
      FinalizeGenerateStreams(label, request);
      if (base::FeatureList::IsEnabled(
              blink::features::kStartMediaStreamCaptureIndicatorInBrowser)) {
        OnStreamStarted(label);
      }
      break;
    }
    case blink::MEDIA_GET_OPEN_DEVICE: {
      FinalizeGetOpenDevice(label, request);
      break;
    }
    case blink::MEDIA_DEVICE_UPDATE:
      FinalizeChangeDevice(label, request);
      OnStreamStarted(label);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void MediaStreamManager::Closed(
    MediaStreamType stream_type,
    const base::UnguessableToken& capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(base::StringPrintf("Closed({stream_type=%s}, {session_id=%s})",
                                    StreamTypeToString(stream_type),
                                    capture_session_id.ToString().c_str()));
}

void MediaStreamManager::DevicesEnumerated(
    bool requested_audio_input,
    bool requested_video_input,
    const std::string& label,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const DeviceRequests::const_iterator request_it = FindRequestIterator(label);
  if (request_it == requests_.end()) {
    return;
  }
  DeviceRequest* const request = request_it->second.get();

  SendLogMessage(base::StringPrintf(
      "DevicesEnumerated({label=%s}, {requester_id=%d}, {request_type=%s})",
      label.c_str(), request->requester_id,
      RequestTypeToString(request->request_type())));

  bool requested[] = {requested_audio_input, requested_video_input};
  MediaStreamType stream_types[] = {MediaStreamType::DEVICE_AUDIO_CAPTURE,
                                    MediaStreamType::DEVICE_VIDEO_CAPTURE};
  for (size_t i = 0; i < std::size(requested); ++i) {
    if (!requested[i]) {
      continue;
    }

    DCHECK(request->audio_type() == stream_types[i] ||
           request->video_type() == stream_types[i]);
    if (request->state(stream_types[i]) == MEDIA_REQUEST_STATE_REQUESTED) {
      request->SetState(stream_types[i], MEDIA_REQUEST_STATE_PENDING_APPROVAL);
    }
  }

  if (!SetUpDeviceCaptureRequest(request, enumeration)) {
    FinalizeRequestFailed(request_it, MediaStreamRequestResult::NO_HARDWARE);
  } else {
    ReadOutputParamsAndPostRequestToUI(label, request, enumeration);
  }
}

void MediaStreamManager::Aborted(
    MediaStreamType stream_type,
    const base::UnguessableToken& capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(base::StringPrintf(
      "Aborted({stream_type=%s}, {session_id=%s})",
      StreamTypeToString(stream_type), capture_session_id.ToString().c_str()));
  StopDevice(stream_type, capture_session_id);
}

void MediaStreamManager::UseFakeUIFactoryForTests(
    base::RepeatingCallback<std::unique_ptr<FakeMediaStreamUIProxy>(void)>
        fake_ui_factory,
    bool use_for_gum_desktop_capture) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  fake_ui_factory_ = std::move(fake_ui_factory);
  use_fake_ui_for_gum_desktop_capture_ = use_for_gum_desktop_capture;
}

// static
void MediaStreamManager::RegisterNativeLogCallback(
    int renderer_host_id,
    base::RepeatingCallback<void(const std::string&)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!media_stream_manager) {
    DLOG(ERROR) << "No MediaStreamManager on the IO thread.";
    return;
  }

  media_stream_manager->DoNativeLogCallbackRegistration(renderer_host_id,
                                                        std::move(callback));
}

// static
void MediaStreamManager::UnregisterNativeLogCallback(int renderer_host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!media_stream_manager) {
    DLOG(ERROR) << "No MediaStreamManager on the IO thread.";
    return;
  }

  media_stream_manager->DoNativeLogCallbackUnregistration(renderer_host_id);
}

void MediaStreamManager::AddLogMessageOnIOThread(const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const auto& callback : log_callbacks_) {
    callback.second.Run(message);
  }
}

void MediaStreamManager::HandleAccessRequestResponse(
    const std::string& label,
    const media::AudioParameters& output_parameters,
    const blink::mojom::StreamDevicesSet& stream_devices_set,
    MediaStreamRequestResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK((result == MediaStreamRequestResult::OK &&
          !stream_devices_set.stream_devices.empty()) ||
         (result != MediaStreamRequestResult::OK &&
          stream_devices_set.stream_devices.empty()));

  const DeviceRequests::const_iterator request_it = FindRequestIterator(label);
  if (request_it == requests_.end()) {
    // The request has been canceled before the UI returned.
    return;
  }
  DeviceRequest* const request = request_it->second.get();

  SendLogMessage(base::StringPrintf(
      "HandleAccessRequestResponse({label=%s}, {request=%s}, {result=%s})",
      label.c_str(), RequestTypeToString(request->request_type()),
      RequestResultToString(result)));

  RecordMediaStreamRequestResponseMetric(request->video_type(),
                                         request->request_type(), result);

  if (request->request_type() == blink::MEDIA_DEVICE_ACCESS) {
    FinalizeMediaAccessRequest(request_it, stream_devices_set);
    return;
  }

  // Handle the case when the request was denied.
  if (result != MediaStreamRequestResult::OK) {
    FinalizeRequestFailed(request_it, result);
    return;
  }

  DCHECK(base::ranges::all_of(
      stream_devices_set.stream_devices,
      [](const blink::mojom::StreamDevicesPtr& stream_devices) {
        return stream_devices->audio_device.has_value() ||
               stream_devices->video_device.has_value();
      }));

  if (request->request_type() == blink::MEDIA_DEVICE_UPDATE) {
    HandleChangeSourceRequestResponse(label, request, stream_devices_set);
    return;
  }

  // Process all newly-accepted devices for this request.
  bool found_audio = false;
  bool found_video = false;
  for (size_t stream_index = request->stream_devices_set.stream_devices.size();
       stream_index < stream_devices_set.stream_devices.size();
       ++stream_index) {
    request->stream_devices_set.stream_devices.push_back(
        blink::mojom::StreamDevices::New());
  }
  for (size_t stream_index = 0;
       stream_index < stream_devices_set.stream_devices.size();
       ++stream_index) {
    const blink::mojom::StreamDevicesPtr& stream_devices_ptr =
        stream_devices_set.stream_devices[stream_index];
    const blink::mojom::StreamDevices& devices = *stream_devices_ptr;
    for (const absl::optional<blink::MediaStreamDevice>* device_ptr :
         {&devices.audio_device, &devices.video_device}) {
      if (!device_ptr->has_value()) {
        continue;
      }
      MediaStreamDevice device = device_ptr->value();

      if (device.type == MediaStreamType::GUM_TAB_VIDEO_CAPTURE ||
          device.type == MediaStreamType::GUM_TAB_AUDIO_CAPTURE) {
        device.id = request->tab_capture_device_id;
      }

      // Initialize the sample_rate and channel_layout here since for audio
      // mirroring, we don't go through EnumerateDevices where these are usually
      // initialized.
      if (device.type == MediaStreamType::GUM_TAB_AUDIO_CAPTURE ||
          device.type == MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE) {
        int sample_rate = output_parameters.sample_rate();
        // If we weren't able to get the native sampling rate or the sample_rate
        // is outside the valid range for input devices set reasonable defaults.
        if (sample_rate <= 0 || sample_rate > 96000) {
          sample_rate = 44100;
        }

        media::AudioParameters params(
            device.input.format(), media::ChannelLayoutConfig::Stereo(),
            sample_rate, device.input.frames_per_buffer());
        params.set_effects(device.input.effects());
        params.set_mic_positions(device.input.mic_positions());
        DCHECK(params.IsValid());
        device.input = params;
      }

      if (device.type == request->audio_type()) {
        found_audio = true;
      } else if (device.type == request->video_type()) {
        found_video = true;
      }

      // If this is request for a new MediaStream, a device is only opened once
      // per render frame. This is so that the permission to use a device can be
      // revoked by a single call to StopStreamDevice regardless of how many
      // MediaStreams it is being used in.
      if (request->request_type() == blink::MEDIA_GENERATE_STREAM) {
        MediaRequestState state;
        if (FindExistingRequestedDevice(*request, device, &device, &state)) {
          SetRequestDevice(
              *request->stream_devices_set.stream_devices[stream_index],
              device);
          request->SetState(device.type, state);
          SendLogMessage(base::StringPrintf(
              "HandleAccessRequestResponse([label=%s]) => "
              "(already opened device: [id: %s, session_id: %s])",
              label.c_str(), device.id.c_str(),
              device.session_id().ToString().c_str()));
          continue;
        }
      }
      device.set_session_id(GetDeviceManager(device.type)->Open(device));
      TranslateDeviceIdToSourceId(request, &device);
      SetRequestDevice(
          *request->stream_devices_set.stream_devices[stream_index], device);
      const MediaRequestState current_state = request->state(device.type);
      if (current_state != MEDIA_REQUEST_STATE_OPENING &&
          current_state != MEDIA_REQUEST_STATE_ERROR) {
        request->SetState(device.type, MEDIA_REQUEST_STATE_OPENING);
      }
      SendLogMessage(
          base::StringPrintf("HandleAccessRequestResponse([label=%s]) => "
                             "(opening device: [id: %s, session_id: %s])",
                             label.c_str(), device.id.c_str(),
                             device.session_id().ToString().c_str()));
    }
  }

  // If the user does not choose to share audio, the audio device is not
  // added. In this case the audio type needs to be set to NO_SERVICE so that no
  // audio device is added if a change-source is requested (i.e., if the
  // share-this-tab-instead button is clicked). (Resolves crbug.com/1378910)
  if (!found_audio) {
    request->DisableAudioSharing();
  }

  // Check whether we've received all stream types requested.
  if (!found_audio && blink::IsAudioInputMediaType(request->audio_type())) {
    request->SetState(request->audio_type(), MEDIA_REQUEST_STATE_ERROR);
    DVLOG(1) << "Set no audio found label " << label;
  }

  if (!found_video && blink::IsVideoInputMediaType(request->video_type())) {
    request->SetState(request->video_type(), MEDIA_REQUEST_STATE_ERROR);
  }

  if (RequestDone(*request)) {
    HandleRequestDone(label, request);
  }
}

void MediaStreamManager::HandleChangeSourceRequestResponse(
    const std::string& label,
    DeviceRequest* request,
    const blink::mojom::StreamDevicesSet& stream_devices_set) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(request->stream_devices_set.stream_devices.size(), 1u);
  DCHECK_LE(request->old_stream_devices_set.stream_devices.size(), 1u);
  DCHECK_EQ(stream_devices_set.stream_devices.size(), 1u);

  DVLOG(1) << "HandleChangeSourceRequestResponse("
           << ", {label = " << label << "})";

  if (request->old_stream_devices_set.stream_devices.empty()) {
    request->old_stream_devices_set.stream_devices.emplace_back(
        blink::mojom::StreamDevices::New());
  }
  std::swap(request->old_stream_devices_set.stream_devices,
            request->stream_devices_set.stream_devices);

  const blink::mojom::StreamDevices& devices =
      *stream_devices_set.stream_devices[0];
  for (const absl::optional<blink::MediaStreamDevice>* device :
       {&devices.audio_device, &devices.video_device}) {
    if (!device->has_value()) {
      continue;
    }
    blink::MediaStreamDevice new_device = device->value();
    new_device.set_session_id(
        GetDeviceManager(new_device.type)->Open(new_device));
    request->SetState(new_device.type, MEDIA_REQUEST_STATE_OPENING);
    SetRequestDevice(*request->stream_devices_set.stream_devices[0],
                     new_device);
  }

  request->SetAudioType(devices.audio_device.has_value()
                            ? request->stream_controls().audio.stream_type
                            : MediaStreamType::NO_SERVICE);
}

void MediaStreamManager::StopMediaStreamFromBrowser(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const DeviceRequests::const_iterator request_it = FindRequestIterator(label);
  if (request_it == requests_.end()) {
    return;
  }
  DeviceRequest* const request = request_it->second.get();

  SendLogMessage(base::StringPrintf("StopMediaStreamFromBrowser({label=%s})",
                                    label.c_str()));

  // Notify renderers that the devices in the stream will be stopped.
  if (request->device_stopped_cb) {
    for (const blink::mojom::StreamDevicesPtr& stream_devices_ptr :
         request->stream_devices_set.stream_devices) {
      const blink::mojom::StreamDevices& stream_devices = *stream_devices_ptr;
      for (const absl::optional<blink::MediaStreamDevice>* device_ptr : {
               &stream_devices.audio_device,
               &stream_devices.video_device,
           }) {
        if (!device_ptr->has_value()) {
          continue;
        }
        const blink::MediaStreamDevice& device = device_ptr->value();
        request->device_stopped_cb.Run(label, device);
      }
    }
  }

  CancelRequest(request_it);
  IncrementDesktopCaptureCounter(DESKTOP_CAPTURE_NOTIFICATION_STOP);
}

void MediaStreamManager::ChangeMediaStreamSourceFromBrowser(
    const std::string& label,
    const DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DeviceRequest* request = FindRequest(label);
  if (!request) {
    return;
  }

  DCHECK_EQ(1u, request->stream_devices_set.stream_devices.size());
  const blink::mojom::StreamDevices& devices =
      *request->stream_devices_set.stream_devices[0];

  if (request->ui_proxy) {
    for (const absl::optional<blink::MediaStreamDevice>* device_ptr :
         {&devices.audio_device, &devices.video_device}) {
      if (!device_ptr->has_value()) {
        continue;
      }
      const blink::MediaStreamDevice& device = device_ptr->value();
      const DesktopMediaID old_media_id = DesktopMediaID::Parse(device.id);
      if (!old_media_id.is_null()) {
        request->ui_proxy->OnDeviceStoppedForSourceChange(label, old_media_id,
                                                          media_id);
      }
    }
  }

  SendLogMessage(base::StringPrintf(
      "ChangeMediaStreamSourceFromBrowser({label=%s})", label.c_str()));

  SetUpDesktopCaptureChangeSourceRequest(request, label, media_id);
  IncrementDesktopCaptureCounter(DESKTOP_CAPTURE_NOTIFICATION_CHANGE_SOURCE);
}

void MediaStreamManager::OnRequestStateChangeFromBrowser(
    const std::string& label,
    const DesktopMediaID& media_id,
    blink::mojom::MediaStreamStateChange new_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DeviceRequest* request = FindRequest(label);
  if (!request) {
    return;
  }

  SendLogMessage(base::StringPrintf("RequestStateChangeFromBrowser({label=%s})",
                                    label.c_str()));

  request->OnRequestStateChangeFromBrowser(label, media_id, new_state);
}

void MediaStreamManager::WillDestroyCurrentMessageLoop() {
  DVLOG(3) << "MediaStreamManager::WillDestroyCurrentMessageLoop()";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::IO));
  if (media_devices_manager_) {
    media_devices_manager_->StopMonitoring();
  }
  if (video_capture_manager_) {
    video_capture_manager_->UnregisterListener(this);
  }
  if (audio_input_device_manager_) {
    audio_input_device_manager_->UnregisterListener(this);
  }

  audio_input_device_manager_ = nullptr;
  video_capture_manager_ = nullptr;
  media_devices_manager_ = nullptr;
  media_stream_manager = nullptr;
  requests_.clear();
  dispatcher_hosts_.Clear();
  video_capture_hosts_.Clear();
}

void MediaStreamManager::NotifyDevicesChanged(
    MediaDeviceType device_type,
    const blink::WebMediaDeviceInfoArray& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(base::StringPrintf("NotifyDevicesChanged({device_type=%s})",
                                    DeviceTypeToString(device_type)));

  MediaObserver* media_observer =
      GetContentClient()->browser()->GetMediaObserver();

  MediaStreamType stream_type = ConvertToMediaStreamType(device_type);
  MediaStreamDevices new_devices =
      ConvertToMediaStreamDevices(stream_type, devices);

  if (blink::IsAudioInputMediaType(stream_type)) {
    MediaCaptureDevicesImpl::GetInstance()->OnAudioCaptureDevicesChanged(
        new_devices);
    if (media_observer) {
      media_observer->OnAudioCaptureDevicesChanged();
    }
  } else if (blink::IsVideoInputMediaType(stream_type)) {
    MediaCaptureDevicesImpl::GetInstance()->OnVideoCaptureDevicesChanged(
        new_devices);
    if (media_observer) {
      media_observer->OnVideoCaptureDevicesChanged();
    }
  } else {
    NOTREACHED();
  }
}

bool MediaStreamManager::RequestDone(const DeviceRequest& request) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(base::StringPrintf(
      "RequestDone({requester_id=%d}, {request_type=%s})", request.requester_id,
      RequestTypeToString(request.request_type())));

  const bool requested_audio =
      blink::IsAudioInputMediaType(request.audio_type());
  const bool requested_video =
      blink::IsVideoInputMediaType(request.video_type());

  const bool audio_done =
      !requested_audio ||
      request.state(request.audio_type()) == MEDIA_REQUEST_STATE_DONE ||
      request.state(request.audio_type()) == MEDIA_REQUEST_STATE_ERROR;
  if (!audio_done) {
    return false;
  }

  const bool video_done =
      !requested_video ||
      request.state(request.video_type()) == MEDIA_REQUEST_STATE_DONE ||
      request.state(request.video_type()) == MEDIA_REQUEST_STATE_ERROR;
  if (!video_done) {
    return false;
  }

  return true;
}

MediaStreamProvider* MediaStreamManager::GetDeviceManager(
    MediaStreamType stream_type) const {
  if (blink::IsVideoInputMediaType(stream_type)) {
    return video_capture_manager();
  } else if (blink::IsAudioInputMediaType(stream_type)) {
    return audio_input_device_manager();
  }
  NOTREACHED();
  return nullptr;
}

void MediaStreamManager::OnMediaStreamUIWindowId(
    MediaStreamType video_type,
    blink::mojom::StreamDevicesSetPtr stream_devices_set,
    gfx::NativeViewId window_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!window_id) {
    return;
  }

  if (!blink::IsVideoDesktopCaptureMediaType(video_type)) {
    return;
  }

  // Pass along for desktop screen and window capturing when
  // DesktopCaptureDevice is used.
  for (const blink::mojom::StreamDevicesPtr& stream_devices_ptr :
       stream_devices_set->stream_devices) {
    const blink::mojom::StreamDevices& stream_devices = *stream_devices_ptr;
    for (const absl::optional<blink::MediaStreamDevice>* device_ptr : {
             &stream_devices.audio_device,
             &stream_devices.video_device,
         }) {
      if (!device_ptr->has_value()) {
        continue;
      }
      const blink::MediaStreamDevice& device = device_ptr->value();
      if (!blink::IsVideoDesktopCaptureMediaType(device.type)) {
        continue;
      }

      DesktopMediaID media_id = DesktopMediaID::Parse(device.id);
      // WebContentsVideoCaptureDevice is used for tab/webcontents.
      if (media_id.type == DesktopMediaID::TYPE_WEB_CONTENTS) {
        continue;
      }
#if defined(USE_AURA)
      // DesktopCaptureDeviceAura is used when aura_id is valid.
      if (media_id.window_id > DesktopMediaID::kNullId) {
        continue;
      }
#endif
      video_capture_manager_->SetDesktopCaptureWindowId(device.session_id(),
                                                        window_id);
      break;
    }
  }
}

void MediaStreamManager::DoNativeLogCallbackRegistration(
    int renderer_host_id,
    base::RepeatingCallback<void(const std::string&)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Re-registering (overwriting) is allowed and happens in some tests.
  log_callbacks_[renderer_host_id] = std::move(callback);
}

void MediaStreamManager::DoNativeLogCallbackUnregistration(
    int renderer_host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  log_callbacks_.erase(renderer_host_id);
}

// static
std::string MediaStreamManager::GetHMACForMediaDeviceID(
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& raw_unique_id) {
  // TODO(crbug.com/1215532): DCHECKs are disabled during automated testing on
  // CrOS and this check failed when tested on an experimental builder. Revert
  // https://crrev.com/c/2932244 to enable it. See go/chrome-dcheck-on-cros
  // or http://crbug.com/1113456 for more details.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(!raw_unique_id.empty());
#endif
  if (raw_unique_id == media::AudioDeviceDescription::kDefaultDeviceId ||
      raw_unique_id == media::AudioDeviceDescription::kCommunicationsDeviceId) {
    return raw_unique_id;
  }

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  const size_t digest_length = hmac.DigestLength();
  std::vector<uint8_t> digest(digest_length);
  bool result = hmac.Init(security_origin.Serialize()) &&
                hmac.Sign(raw_unique_id + salt, &digest[0], digest.size());
  DCHECK(result);
  return base::ToLowerASCII(base::HexEncode(&digest[0], digest.size()));
}

// static
bool MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& device_guid,
    const std::string& raw_unique_id) {
  DCHECK(!raw_unique_id.empty());
  std::string guid_from_raw_device_id =
      GetHMACForMediaDeviceID(salt, security_origin, raw_unique_id);
  return guid_from_raw_device_id == device_guid;
}

// static
void MediaStreamManager::GetMediaDeviceIDForHMAC(
    MediaStreamType stream_type,
    std::string salt,
    url::Origin security_origin,
    std::string hmac_device_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceCallback<void(const absl::optional<std::string>&)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(stream_type == MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         stream_type == MediaStreamType::DEVICE_VIDEO_CAPTURE);
  MediaDeviceType device_type = ConvertToMediaDeviceType(stream_type);
  MediaStreamManager::GetMediaDeviceIDForHMAC(
      device_type, std::move(salt), std::move(security_origin),
      std::move(hmac_device_id), std::move(task_runner), std::move(callback));
}

void MediaStreamManager::GetMediaDeviceIDForHMAC(
    MediaDeviceType device_type,
    std::string salt,
    url::Origin security_origin,
    std::string hmac_device_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceCallback<void(const absl::optional<std::string>&)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaDevicesManager::BoolDeviceTypes requested_types;
  requested_types[static_cast<size_t>(device_type)] = true;
  media_stream_manager->media_devices_manager()->EnumerateDevices(
      requested_types,
      base::BindOnce(&FinalizeGetMediaDeviceIDForHMAC, device_type,
                     std::move(salt), std::move(security_origin),
                     std::move(hmac_device_id), std::move(task_runner),
                     std::move(callback)));
}

// static
bool MediaStreamManager::IsOriginAllowed(int render_process_id,
                                         const url::Origin& origin) {
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          render_process_id, origin.GetURL())) {
    LOG(ERROR) << "MSM: Renderer requested a URL it's not allowed to use: "
               << origin.Serialize();
    return false;
  }

  return true;
}

void MediaStreamManager::SetCapturingLinkSecured(
    int render_process_id,
    const base::UnguessableToken& session_id,
    MediaStreamType type,
    bool is_secure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (LabeledDeviceRequest& labeled_request : requests_) {
    DeviceRequest* request = labeled_request.second.get();
    if (request->requesting_process_id != render_process_id) {
      continue;
    }

    for (const blink::mojom::StreamDevicesPtr& stream_devices_ptr :
         request->stream_devices_set.stream_devices) {
      const blink::mojom::StreamDevices& stream_devices = *stream_devices_ptr;
      for (const absl::optional<blink::MediaStreamDevice>* device_ptr : {
               &stream_devices.audio_device,
               &stream_devices.video_device,
           }) {
        if (!device_ptr->has_value()) {
          continue;
        }
        const blink::MediaStreamDevice& device = device_ptr->value();
        if (device.session_id() == session_id && device.type == type) {
          request->SetCapturingLinkSecured(is_secure);
          return;
        }
      }
    }
  }
}

void MediaStreamManager::SetStateForTesting(
    size_t request_index,
    blink::mojom::MediaStreamType stream_type,
    MediaRequestState new_state) {
  DCHECK_LT(request_index, requests_.size());
  auto requests_iterator = requests_.begin();
  std::advance(requests_iterator, request_index);
  requests_iterator->second->SetState(stream_type, new_state);
}

void MediaStreamManager::SetGenerateStreamsCallbackForTesting(
    GenerateStreamTestCallback test_callback) {
  generate_stream_test_callback_ = std::move(test_callback);
}

MediaStreamDevices MediaStreamManager::ConvertToMediaStreamDevices(
    MediaStreamType stream_type,
    const blink::WebMediaDeviceInfoArray& device_infos) {
  MediaStreamDevices devices;
  for (const auto& info : device_infos) {
    devices.emplace_back(
        stream_type, info.device_id, info.label, info.video_control_support,
        static_cast<media::VideoFacingMode>(info.video_facing), info.group_id);
  }

  return devices;
}

void MediaStreamManager::ActivateTabOnUIThread(const DesktopMediaID source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(source.web_contents_id.render_process_id,
                                  source.web_contents_id.main_render_frame_id);
  if (rfh) {
    rfh->render_view_host()->GetDelegate()->Activate();
  }
}

void MediaStreamManager::OnStreamStarted(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeviceRequest* const request = FindRequest(label);
  if (!request) {
    return;
  }
  SendLogMessage(base::StringPrintf(
      "OnStreamStarted({label=%s}, {requester_id=%d}, {request_type=%s})",
      label.c_str(), request->requester_id,
      RequestTypeToString(request->request_type())));

  MediaStreamUI::SourceCallback device_changed_cb;
  if (request->stream_controls().dynamic_surface_switching_requested &&
      ChangeSourceSupported(
          blink::ToMediaStreamDevicesList(request->stream_devices_set)) &&
      base::FeatureList::IsEnabled(features::kDesktopCaptureChangeSource)) {
    device_changed_cb = base::BindRepeating(
        &MediaStreamManager::ChangeMediaStreamSourceFromBrowser,
        base::Unretained(this), label);
  }

  std::vector<DesktopMediaID> screen_share_ids;
  for (const blink::mojom::StreamDevicesPtr& stream_devices_ptr :
       request->stream_devices_set.stream_devices) {
    const blink::mojom::StreamDevices& stream_devices = *stream_devices_ptr;
    for (const absl::optional<blink::MediaStreamDevice>* device_ptr : {
             &stream_devices.audio_device,
             &stream_devices.video_device,
         }) {
      if (!device_ptr->has_value()) {
        continue;
      }
      const blink::MediaStreamDevice& device = device_ptr->value();
      if (blink::IsVideoScreenCaptureMediaType(device.type)) {
        screen_share_ids.push_back(DesktopMediaID::Parse(device.id));
      }
    }
  }

  // base::Unretained is safe here because MediaStreamManager is deleted on the
  // UI thread, after the IO thread has been stopped.
  if (request->ui_proxy) {
    request->ui_proxy->OnStarted(
        base::BindOnce(&MediaStreamManager::StopMediaStreamFromBrowser,
                       base::Unretained(this), label),
        device_changed_cb,
        base::BindOnce(&MediaStreamManager::OnMediaStreamUIWindowId,
                       base::Unretained(this), request->video_type(),
                       request->stream_devices_set.Clone()),
        label, screen_share_ids,
        base::BindRepeating(
            &MediaStreamManager::OnRequestStateChangeFromBrowser,
            base::Unretained(this), label));
  }
}

void MediaStreamManager::OnCaptureConfigurationChanged(
    const base::UnguessableToken& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (LabeledDeviceRequest& labeled_request : requests_) {
    const std::string& label = labeled_request.first;
    DeviceRequest* request = labeled_request.second.get();
    for (const auto& stream_devices_ptr :
         request->stream_devices_set.stream_devices) {
      const blink::MediaStreamDevice* const device_ptr =
          GetStreamDevice(*stream_devices_ptr, session_id);
      if (!device_ptr) {
        continue;
      }
      request->OnCaptureConfigurationChanged(label, *device_ptr);
    }
  }
}

void MediaStreamManager::OnRegionCaptureRectChanged(
    const base::UnguessableToken& session_id,
    const absl::optional<gfx::Rect>& region_capture_rect) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const LabeledDeviceRequest& labeled_device_request : requests_) {
    DeviceRequest* const device_request = labeled_device_request.second.get();
    if (!device_request || !device_request->ui_proxy) {
      continue;
    }

    for (const blink::mojom::StreamDevicesPtr& stream_devices_ptr :
         labeled_device_request.second->stream_devices_set.stream_devices) {
      const blink::mojom::StreamDevices& stream_devices = *stream_devices_ptr;

      for (const absl::optional<blink::MediaStreamDevice>* device_ptr : {
               &stream_devices.audio_device,
               &stream_devices.video_device,
           }) {
        if (!device_ptr->has_value()) {
          continue;
        }
        const blink::MediaStreamDevice& device = device_ptr->value();
        if (blink::IsVideoInputMediaType(device.type) &&
            session_id == device.session_id()) {
          // Note: |device_request->ui_proxy != nullptr| tested in external
          // loop.
          device_request->ui_proxy->OnRegionCaptureRectChanged(
              region_capture_rect);
        }
      }
    }
  }
}

#if !BUILDFLAG(IS_ANDROID)
void MediaStreamManager::SetCapturedDisplaySurfaceFocus(
    const std::string& label,
    bool focus,
    bool is_from_microtask,
    bool is_from_timer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DeviceRequest* const request = FindRequest(label);
  if (!request) {
    return;
  }

  if (!request->ui_proxy) {
    return;
  }

  DCHECK_EQ(1u, request->stream_devices_set.stream_devices.size());
  const blink::mojom::StreamDevices& devices =
      *request->stream_devices_set.stream_devices[0];

  DesktopMediaID media_id;
  for (const absl::optional<blink::MediaStreamDevice>* device_ptr :
       {&devices.audio_device, &devices.video_device}) {
    if (!device_ptr->has_value()) {
      continue;
    }
    const blink::MediaStreamDevice& device = device_ptr->value();
    if (blink::IsVideoInputMediaType(device.type)) {
      media_id = DesktopMediaID::Parse(device.id);
      break;
    }
  }

  if (media_id.is_null()) {
    return;
  }

  if (media_id.type != DesktopMediaID::Type::TYPE_WEB_CONTENTS &&
      media_id.type != DesktopMediaID::Type::TYPE_WINDOW) {
    return;  // Video device not focus-able.
  }

  request->ui_proxy->SetFocus(media_id, focus, is_from_microtask,
                              is_from_timer);
}
#endif

void MediaStreamManager::RegisterDispatcherHost(
    std::unique_ptr<blink::mojom::MediaStreamDispatcherHost> host,
    mojo::PendingReceiver<blink::mojom::MediaStreamDispatcherHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  dispatcher_hosts_.Add(std::move(host), std::move(receiver));
}

void MediaStreamManager::RegisterVideoCaptureHost(
    std::unique_ptr<media::mojom::VideoCaptureHost> host,
    mojo::PendingReceiver<media::mojom::VideoCaptureHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  video_capture_hosts_.Add(std::move(host), std::move(receiver));
}

// static
PermissionControllerImpl* MediaStreamManager::GetPermissionController(
    int requesting_process_id,
    int requesting_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHost* rfh =
      RenderFrameHost::FromID(requesting_process_id, requesting_frame_id);
  if (!rfh) {
    return nullptr;
  }

  return PermissionControllerImpl::FromBrowserContext(rfh->GetBrowserContext());
}

void MediaStreamManager::SubscribeToPermissionController(
    const std::string& label,
    const DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request);

  // It is safe to bind base::Unretained(this) because MediaStreamManager is
  // owned by BrowserMainLoop.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaStreamManager::SubscribeToPermissionControllerOnUIThread,
          base::Unretained(this), label, request->requesting_process_id,
          request->requesting_frame_id, request->requester_id,
          request->page_request_id,
          blink::IsAudioInputMediaType(request->audio_type()),
          blink::IsVideoInputMediaType(request->video_type()),
          request->salt_and_origin.origin.GetURL()));
}

void MediaStreamManager::SubscribeToPermissionControllerOnUIThread(
    const std::string& label,
    int requesting_process_id,
    int requesting_frame_id,
    int requester_id,
    int page_request_id,
    bool is_audio_request,
    bool is_video_request,
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PermissionControllerImpl* controller =
      GetPermissionController(requesting_process_id, requesting_frame_id);
  if (!controller) {
    return;
  }

  PermissionController::SubscriptionId audio_subscription_id;
  PermissionController::SubscriptionId video_subscription_id;

  if (is_audio_request) {
    // It is safe to bind base::Unretained(this) because MediaStreamManager is
    // owned by BrowserMainLoop.
    audio_subscription_id = controller->SubscribePermissionStatusChange(
        blink::PermissionType::AUDIO_CAPTURE,
        /*render_process_host=*/nullptr,
        RenderFrameHost::FromID(requesting_process_id, requesting_frame_id),
        origin,
        base::BindRepeating(&MediaStreamManager::PermissionChangedCallback,
                            base::Unretained(this), requesting_process_id,
                            requesting_frame_id, requester_id,
                            page_request_id));
  }

  if (is_video_request) {
    // It is safe to bind base::Unretained(this) because MediaStreamManager is
    // owned by BrowserMainLoop.
    video_subscription_id = controller->SubscribePermissionStatusChange(
        blink::PermissionType::VIDEO_CAPTURE,
        /*render_process_host=*/nullptr,
        RenderFrameHost::FromID(requesting_process_id, requesting_frame_id),
        origin,
        base::BindRepeating(&MediaStreamManager::PermissionChangedCallback,
                            base::Unretained(this), requesting_process_id,
                            requesting_frame_id, requester_id,
                            page_request_id));
  }

  // It is safe to bind base::Unretained(this) because MediaStreamManager is
  // owned by BrowserMainLoop.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaStreamManager::SetPermissionSubscriptionIDs,
                     base::Unretained(this), label, requesting_process_id,
                     requesting_frame_id, audio_subscription_id,
                     video_subscription_id));
}

void MediaStreamManager::SetPermissionSubscriptionIDs(
    const std::string& label,
    int requesting_process_id,
    int requesting_frame_id,
    PermissionController::SubscriptionId audio_subscription_id,
    PermissionController::SubscriptionId video_subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DeviceRequest* const request = FindRequest(label);
  if (!request) {
    // Something happened with the request while the permission subscription was
    // created, unsubscribe to clean up.
    // It is safe to bind base::Unretained(this) because MediaStreamManager is
    // owned by BrowserMainLoop.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MediaStreamManager::UnsubscribeFromPermissionControllerOnUIThread,
            requesting_process_id, requesting_frame_id, audio_subscription_id,
            video_subscription_id));

    return;
  }

  request->audio_subscription_id = audio_subscription_id;
  request->video_subscription_id = video_subscription_id;
}

// static
void MediaStreamManager::UnsubscribeFromPermissionControllerOnUIThread(
    int requesting_process_id,
    int requesting_frame_id,
    PermissionController::SubscriptionId audio_subscription_id,
    PermissionController::SubscriptionId video_subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PermissionControllerImpl* controller =
      GetPermissionController(requesting_process_id, requesting_frame_id);
  if (!controller) {
    return;
  }

  controller->UnsubscribePermissionStatusChange(audio_subscription_id);
  controller->UnsubscribePermissionStatusChange(video_subscription_id);
}

void MediaStreamManager::PermissionChangedCallback(
    int requesting_process_id,
    int requesting_frame_id,
    int requester_id,
    int page_request_id,
    blink::mojom::PermissionStatus status) {
  if (status == blink::mojom::PermissionStatus::GRANTED) {
    return;
  }

  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    // It is safe to bind base::Unretained(this) because MediaStreamManager is
    // owned by BrowserMainLoop.
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaStreamManager::PermissionChangedCallback,
                       base::Unretained(this), requesting_process_id,
                       requesting_frame_id, requester_id, page_request_id,
                       status));

    return;
  }

  CancelRequest(requesting_process_id, requesting_frame_id, requester_id,
                page_request_id);
}

void MediaStreamManager::MaybeStartTrackingCaptureHandleConfig(
    const std::string& label,
    const MediaStreamDevice& captured_device,
    DeviceRequest& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!blink::IsVideoInputMediaType(captured_device.type) ||
      !WebContentsMediaCaptureId::Parse(captured_device.id, nullptr)) {
    return;
  }

  const GlobalRenderFrameHostId capturer(request.requesting_process_id,
                                         request.requesting_frame_id);

  // It is safe to bind base::Unretained(this) because MediaStreamManager is
  // owned by BrowserMainLoop.
  // Since |capture_handle_manager_| is owned by |this|, it is also safe to
  // bind base::Unretained(&capture_handle_manager_).
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CaptureHandleManager::OnTabCaptureStarted,
                     base::Unretained(&capture_handle_manager_), label,
                     captured_device, capturer,
                     base::BindPostTask(GetIOThreadTaskRunner({}),
                                        request.OnCaptureHandleChangeCb())));
}

void MediaStreamManager::MaybeStopTrackingCaptureHandleConfig(
    const std::string& label,
    const MediaStreamDevice& captured_device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!blink::IsVideoInputMediaType(captured_device.type) ||
      !WebContentsMediaCaptureId::Parse(captured_device.id, nullptr)) {
    return;
  }

  // It is safe to bind base::Unretained(&capture_handle_manager_) because
  // it is owned by MediaStreamManager, which is in turn owned by
  // BrowserMainLoop.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CaptureHandleManager::OnTabCaptureStopped,
                                base::Unretained(&capture_handle_manager_),
                                label, captured_device));
}

void MediaStreamManager::MaybeUpdateTrackedCaptureHandleConfigs(
    const std::string& label,
    const blink::mojom::StreamDevicesSet& new_devices_set,
    DeviceRequest& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(1u, new_devices_set.stream_devices.size());

  const GlobalRenderFrameHostId capturer(request.requesting_process_id,
                                         request.requesting_frame_id);

  const blink::mojom::StreamDevices& new_devices =
      *new_devices_set.stream_devices[0];
  blink::mojom::StreamDevicesSetPtr filtered_new_devices_set =
      blink::mojom::StreamDevicesSet::New();
  filtered_new_devices_set->stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& filtered_new_devices =
      *filtered_new_devices_set->stream_devices[0];
  if (new_devices.video_device.has_value() &&
      WebContentsMediaCaptureId::Parse(new_devices.video_device->id, nullptr)) {
    filtered_new_devices.video_device = new_devices.video_device.value();
  }

  // It is safe to bind base::Unretained(&capture_handle_manager_) because
  // it is owned by MediaStreamManager, which is in turn owned by
  // BrowserMainLoop.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CaptureHandleManager::OnTabCaptureDevicesUpdated,
                     base::Unretained(&capture_handle_manager_), label,
                     std::move(filtered_new_devices_set), capturer,
                     base::BindPostTask(GetIOThreadTaskRunner({}),
                                        request.OnCaptureHandleChangeCb())));
}

bool MediaStreamManager::ShouldUseFakeUIProxy(
    const DeviceRequest& request) const {
  if (!fake_ui_factory_) {
    return false;
  }

  if (use_fake_ui_only_for_camera_and_microphone_) {
    return request.audio_type() == MediaStreamType::DEVICE_AUDIO_CAPTURE ||
           request.video_type() == MediaStreamType::DEVICE_VIDEO_CAPTURE;
  }

  return request.video_type() != MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE ||
         use_fake_ui_for_gum_desktop_capture_;
}

std::unique_ptr<MediaStreamUIProxy> MediaStreamManager::MakeFakeUIProxy(
    const std::string& label,
    const MediaDeviceEnumeration& enumeration,
    DeviceRequest* request) {
  // Just auto-select from the available devices.
  MediaStreamDevices devices;
  if (request->video_type() == MediaStreamType::DISPLAY_VIDEO_CAPTURE ||
      request->video_type() ==
          MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB ||
      request->video_type() == MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET) {
    devices = DisplayMediaDevicesFromFakeDeviceConfig(
        request->video_type(),
        request->audio_type() == MediaStreamType::DISPLAY_AUDIO_CAPTURE,
        request->requesting_process_id, request->requesting_frame_id,
        request->stream_controls().preferred_display_surface);
  } else if (request->video_type() ==
             MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE) {
    // Cache the |label| in the device name field, for unit test purpose only.
    devices.emplace_back(
        MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
        DesktopMediaID(DesktopMediaID::TYPE_SCREEN, DesktopMediaID::kNullId)
            .ToString(),
        label);
  } else {
    MediaStreamDevices audio_devices = ConvertToMediaStreamDevices(
        request->audio_type(),
        enumeration[static_cast<size_t>(MediaDeviceType::MEDIA_AUDIO_INPUT)]);
    MediaStreamDevices video_devices = ConvertToMediaStreamDevices(
        request->video_type(),
        enumeration[static_cast<size_t>(MediaDeviceType::MEDIA_VIDEO_INPUT)]);
    devices.reserve(audio_devices.size() + video_devices.size());
    devices.insert(devices.end(), audio_devices.begin(), audio_devices.end());
    devices.insert(devices.end(), video_devices.begin(), video_devices.end());
  }

  std::unique_ptr<FakeMediaStreamUIProxy> fake_ui = fake_ui_factory_.Run();
  fake_ui->SetAvailableDevices(devices);

  return fake_ui;
}

}  // namespace content
