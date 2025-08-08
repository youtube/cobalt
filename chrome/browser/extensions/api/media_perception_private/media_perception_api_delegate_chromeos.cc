// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/media_perception_private/media_perception_api_delegate_chromeos.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/chromeos/delegate_to_browser_gpu_service_accelerator_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/video_capture_service.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace extensions {

namespace {

constexpr char kLightComponentName[] = "rtanalytics-light";
constexpr char kFullComponentName[] = "rtanalytics-full";

std::string GetComponentNameForComponentType(
    const extensions::api::media_perception_private::ComponentType& type) {
  switch (type) {
    case extensions::api::media_perception_private::ComponentType::kLight:
      return kLightComponentName;
    case extensions::api::media_perception_private::ComponentType::kFull:
      return kFullComponentName;
    case extensions::api::media_perception_private::ComponentType::kNone:
      LOG(ERROR) << "No component type requested.";
      return "";
  }
  NOTREACHED() << "Reached component type not in switch.";
  return "";
}

api::media_perception_private::ComponentInstallationError
GetComponentInstallationErrorForCrOSComponentManagerError(
    const component_updater::CrOSComponentManager::Error error) {
  switch (error) {
    case component_updater::CrOSComponentManager::Error::ERROR_MAX:
    case component_updater::CrOSComponentManager::Error::NONE:
      return api::media_perception_private::ComponentInstallationError::kNone;
    case component_updater::CrOSComponentManager::Error::UNKNOWN_COMPONENT:
      return api::media_perception_private::ComponentInstallationError::
          kUnknownComponent;
    case component_updater::CrOSComponentManager::Error::INSTALL_FAILURE:
    case component_updater::CrOSComponentManager::Error::UPDATE_IN_PROGRESS:
      return api::media_perception_private::ComponentInstallationError::
          kInstallFailure;
    case component_updater::CrOSComponentManager::Error::MOUNT_FAILURE:
      return api::media_perception_private::ComponentInstallationError::
          kMountFailure;
    case component_updater::CrOSComponentManager::Error::
        COMPATIBILITY_CHECK_FAILED:
      return api::media_perception_private::ComponentInstallationError::
          kCompatibilityCheckFailed;
    case component_updater::CrOSComponentManager::Error::NOT_FOUND:
      return api::media_perception_private::ComponentInstallationError::
          kNotFound;
  }
  NOTREACHED() << "Reached component error type not in switch.";
  return api::media_perception_private::ComponentInstallationError::kNone;
}

void OnLoadComponent(
    MediaPerceptionAPIDelegate::LoadCrOSComponentCallback load_callback,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& mount_point) {
  std::move(load_callback)
      .Run(GetComponentInstallationErrorForCrOSComponentManagerError(error),
           mount_point);
}

}  // namespace

MediaPerceptionAPIDelegateChromeOS::MediaPerceptionAPIDelegateChromeOS() =
    default;

MediaPerceptionAPIDelegateChromeOS::~MediaPerceptionAPIDelegateChromeOS() {}

void MediaPerceptionAPIDelegateChromeOS::LoadCrOSComponent(
    const extensions::api::media_perception_private::ComponentType& type,
    LoadCrOSComponentCallback load_callback) {
  g_browser_process->platform_part()->cros_component_manager()->Load(
      GetComponentNameForComponentType(type),
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(OnLoadComponent, std::move(load_callback)));
}

void MediaPerceptionAPIDelegateChromeOS::BindVideoSourceProvider(
    mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojo::PendingRemote<video_capture::mojom::AcceleratorFactory>
      accelerator_factory;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<
          content::DelegateToBrowserGpuServiceAcceleratorFactory>(),
      accelerator_factory.InitWithNewPipeAndPassReceiver());

  auto& service = content::GetVideoCaptureService();
  service.InjectGpuDependencies(std::move(accelerator_factory));
  service.ConnectToVideoSourceProvider(std::move(receiver));
}

void MediaPerceptionAPIDelegateChromeOS::SetMediaPerceptionRequestHandler(
    MediaPerceptionRequestHandler handler) {
  handler_ = std::move(handler);
}

void MediaPerceptionAPIDelegateChromeOS::ForwardMediaPerceptionReceiver(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<chromeos::media_perception::mojom::MediaPerception>
        receiver) {
  if (!handler_) {
    DLOG(ERROR) << "Got receiver but the handler is not set.";
    return;
  }
  handler_.Run(std::move(receiver));
}

}  // namespace extensions
