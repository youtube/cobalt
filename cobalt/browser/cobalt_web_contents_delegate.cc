// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/cobalt_web_contents_delegate.h"

#include "content/public/browser/media_capture_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

#if BUILDFLAG(USE_STARBOARD_MEDIA) && BUILDFLAG(IS_ANDROID)
#include "starboard/android/shared/jni_env_ext.h"
#endif  // BUILDFLAG(IS_ANDROID) && BUILDFLAG(USE_STARBOARD_MEDIA)

#if BUILDFLAG(IS_ANDROID)
#include "starboard/android/shared/starboard_bridge.h"

using starboard::android::shared::StarboardBridge;
#endif  // BUILDFLAG(IS_ANDROID)

namespace cobalt {

namespace {

#if BUILDFLAG(USE_STARBOARD_MEDIA) && BUILDFLAG(IS_ANDROID)
bool RequestRecordAudioPermission() {
  auto* env = starboard::android::shared::JniEnvExt::Get();
  jobject j_audio_permission_requester =
      static_cast<jobject>(env->CallStarboardObjectMethodOrAbort(
          "getAudioPermissionRequester",
          "()Ldev/cobalt/coat/AudioPermissionRequester;"));
  jboolean j_permission = env->CallBooleanMethodOrAbort(
      j_audio_permission_requester, "requestRecordAudioPermission", "()Z");

  return j_permission == JNI_TRUE;
}
#else
bool RequestRecordAudioPermission() {
  // It is expected that all 3P will have system-level permissions.
  return true;
}
#endif  // BUILDFLAG(IS_ANDROID) && BUILDFLAG(USE_STARBOARD_MEDIA)

const blink::MediaStreamDevice* GetRequestedDeviceOrDefault(
    const blink::MediaStreamDevices& devices,
    const std::string& requested_device_id) {
  if (devices.empty()) {
    return nullptr;
  }
  if (requested_device_id.empty()) {
    return &devices[0];
  }
  auto it = base::ranges::find(devices, requested_device_id,
                               &blink::MediaStreamDevice::id);
  if (it != devices.end()) {
    return &(*it);
  }
  return &devices[0];
}

}  // namespace

void CobaltWebContentsDelegate::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  if (request.audio_type !=
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        /*ui=*/nullptr);
    return;
  }
  bool can_record_audio = RequestRecordAudioPermission();
  if (!can_record_audio) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        /*ui=*/nullptr);
    return;
  }
  auto audio_devices =
      content::MediaCaptureDevices::GetInstance()->GetAudioCaptureDevices();
  const blink::MediaStreamDevice* device = GetRequestedDeviceOrDefault(
      audio_devices, request.requested_audio_device_id);
  if (!device) {
    std::move(callback).Run(blink::mojom::StreamDevicesSet(),
                            blink::mojom::MediaStreamRequestResult::NO_HARDWARE,
                            /*ui=*/nullptr);
    return;
  }
  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  stream_devices_set.stream_devices[0]->audio_device = *device;
  std::move(callback).Run(stream_devices_set,
                          blink::mojom::MediaStreamRequestResult::OK,
                          std::unique_ptr<content::MediaStreamUI>());
}

bool CobaltWebContentsDelegate::CheckMediaAccessPermission(
    content::RenderFrameHost*,
    const GURL&,
    blink::mojom::MediaStreamType) {
  return true;
}

void CobaltWebContentsDelegate::CloseContents(content::WebContents* source) {
#if BUILDFLAG(IS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  StarboardBridge* starboard_bridge = StarboardBridge::GetInstance();
  starboard_bridge->CloseApp(env);
#endif
}

}  // namespace cobalt
