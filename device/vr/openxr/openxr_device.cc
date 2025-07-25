// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_device.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_render_loop.h"
#include "device/vr/public/cpp/features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

namespace {

const std::vector<mojom::XRSessionFeature>& GetSupportedFeatures() {
  static base::NoDestructor<std::vector<mojom::XRSessionFeature>>
      kSupportedFeatures{{mojom::XRSessionFeature::REF_SPACE_VIEWER,
                          mojom::XRSessionFeature::REF_SPACE_LOCAL,
                          mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
                          mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR,
                          mojom::XRSessionFeature::REF_SPACE_UNBOUNDED,
                          mojom::XRSessionFeature::ANCHORS,
                          mojom::XRSessionFeature::SECONDARY_VIEWS}};

  return *kSupportedFeatures;
}

bool AreAllRequiredFeaturesSupported(
    const std::vector<mojom::XRSessionFeature>& required_features,
    const OpenXrExtensionHelper& extension_helper) {
  auto* extension_enum = extension_helper.ExtensionEnumeration();
  return base::ranges::all_of(
      required_features,
      [extension_enum](const mojom::XRSessionFeature& feature) {
        switch (feature) {
          case device::mojom::XRSessionFeature::ANCHORS:
            return extension_enum->ExtensionSupported(
                XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME);
          case device::mojom::XRSessionFeature::HAND_INPUT:
            return extension_enum->ExtensionSupported(
                XR_MSFT_HAND_INTERACTION_EXTENSION_NAME);
          case device::mojom::XRSessionFeature::HIT_TEST:
            return extension_enum->ExtensionSupported(
                XR_MSFT_SCENE_UNDERSTANDING_EXTENSION_NAME);
          case device::mojom::XRSessionFeature::SECONDARY_VIEWS:
            return extension_enum->ExtensionSupported(
                XR_MSFT_SECONDARY_VIEW_CONFIGURATION_EXTENSION_NAME);
          default:
            // All features that don't require an extension are assumed to be
            // supported. We rely on the Browser process pre-filtering and not
            // passing us any features that we haven't already indicated that
            // we could support.
            return true;
        }
      });
}

}  // namespace

OpenXrDevice::OpenXrDevice(
    VizContextProviderFactoryAsync context_provider_factory_async,
    OpenXrPlatformHelper* platform_helper)
    : VRDeviceBase(device::mojom::XRDeviceId::OPENXR_DEVICE_ID),
      context_provider_factory_async_(
          std::move(context_provider_factory_async)),
      platform_helper_(platform_helper) {
  CHECK(platform_helper_);
  CHECK(platform_helper_->EnsureInitialized());

  device::mojom::XRDeviceData device_data = platform_helper_->GetXRDeviceData();

  device_data.supported_features = GetSupportedFeatures();

  // Only support hand input if the feature flag is enabled.
  if (base::FeatureList::IsEnabled(features::kWebXrHandInput))
    device_data.supported_features.emplace_back(
        mojom::XRSessionFeature::HAND_INPUT);

  // Only support layers if the feature flag is enabled.
  if (base::FeatureList::IsEnabled(features::kWebXrLayers))
    device_data.supported_features.emplace_back(
        mojom::XRSessionFeature::LAYERS);

  // Only support hit test if the feature flag is enabled.
  if (base::FeatureList::IsEnabled(features::kOpenXrExtendedFeatureSupport)) {
    device_data.supported_features.emplace_back(
        mojom::XRSessionFeature::HIT_TEST);
  }

  SetDeviceData(std::move(device_data));
}

OpenXrDevice::~OpenXrDevice() {
  // Wait for the render loop to stop before completing destruction. This will
  // ensure that the render loop doesn't get shutdown while it is processing
  // any requests.
  if (render_loop_ && render_loop_->IsRunning()) {
    render_loop_->Stop();
  }

  if (instance_ != XR_NULL_HANDLE) {
    platform_helper_->DestroyInstance(instance_);
  }

  // request_session_callback_ may still be active if we're tearing down the
  // OpenXrDevice while we're still making asynchronous calls to setup the GPU
  // process connection. Ensure the callback is run regardless.
  if (request_session_callback_) {
    std::move(request_session_callback_).Run(nullptr);
  }
}

mojo::PendingRemote<mojom::XRCompositorHost>
OpenXrDevice::BindCompositorHost() {
  return compositor_host_receiver_.BindNewPipeAndPassRemote();
}

void OpenXrDevice::EnsureRenderLoop() {
  if (!render_loop_) {
    render_loop_ = std::make_unique<OpenXrRenderLoop>(
        context_provider_factory_async_, instance_, *extension_helper_,
        platform_helper_);
  }
}

void OpenXrDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  CHECK(!request_session_callback_);
  CHECK(!HasExclusiveSession());

  request_session_callback_ = std::move(callback);

  OpenXrCreateInfo create_info;
  create_info.render_process_id = options->render_process_id;
  create_info.render_frame_id = options->render_frame_id;
  platform_helper_->CreateInstanceWithCreateInfo(
      create_info,
      base::BindOnce(&OpenXrDevice::OnCreateInstanceResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(options)));
}

void OpenXrDevice::OnCreateInstanceResult(
    mojom::XRRuntimeSessionOptionsPtr options,
    XrResult result,
    XrInstance instance) {
  if (XR_FAILED(result)) {
    DVLOG(1) << __func__ << " Failed to create an XrInstance";
    instance_ = XR_NULL_HANDLE;
    std::move(request_session_callback_).Run(nullptr);
    return;
  }

  instance_ = instance;

  extension_helper_ = std::make_unique<OpenXrExtensionHelper>(
      instance_, platform_helper_->GetExtensionEnumeration());

  if (!AreAllRequiredFeaturesSupported(options->required_features,
                                       *extension_helper_)) {
    DVLOG(1) << __func__ << " Missing a required feature";
    // Reject session request, and call ForceEndSession to ensure that we clean
    // up any objects that were already created.
    ForceEndSession(ExitXrPresentReason::kOpenXrStartFailed);
    std::move(request_session_callback_).Run(nullptr);
    return;
  }

  EnsureRenderLoop();

  if (!render_loop_->IsRunning()) {
    render_loop_->Start();

    if (!render_loop_->IsRunning()) {
      DVLOG(1) << __func__ << " Could not start RenderLoop";
      // Reject session request, and call ForceEndSession to ensure that we
      // clean up any objects that were already created.
      ForceEndSession(ExitXrPresentReason::kOpenXrStartFailed);
      std::move(request_session_callback_).Run(nullptr);
      return;
    }

    if (overlay_receiver_) {
      render_loop_->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestOverlay,
                                    base::Unretained(render_loop_.get()),
                                    std::move(overlay_receiver_)));
    }
  }

  auto my_callback = base::BindOnce(&OpenXrDevice::OnRequestSessionResult,
                                    weak_ptr_factory_.GetWeakPtr());

  auto on_visibility_state_changed = base::BindRepeating(
      &OpenXrDevice::OnVisibilityStateChanged, weak_ptr_factory_.GetWeakPtr());

  render_loop_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestSession,
                                base::Unretained(render_loop_.get()),
                                std::move(on_visibility_state_changed),
                                std::move(options), std::move(my_callback)));
}

void OpenXrDevice::OnRequestSessionResult(
    bool result,
    mojom::XRSessionPtr session) {
  DCHECK(request_session_callback_);

  if (!result) {
    // Reject session request, and call ForceEndSession to ensure that we clean
    // up any objects that were already created.
    ForceEndSession(ExitXrPresentReason::kOpenXrStartFailed);
    std::move(request_session_callback_).Run(nullptr);
    return;
  }

  OnStartPresenting();

  auto session_result = mojom::XRRuntimeSessionResult::New();
  session_result->session = std::move(session);
  session_result->controller =
      exclusive_controller_receiver_.BindNewPipeAndPassRemote();

  std::move(request_session_callback_).Run(std::move(session_result));

  // Use of Unretained is safe because the callback will only occur if the
  // binding is not destroyed.
  exclusive_controller_receiver_.set_disconnect_handler(
      base::BindOnce(&OpenXrDevice::OnPresentingControllerMojoConnectionError,
                     base::Unretained(this)));
}

void OpenXrDevice::ForceEndSession(ExitXrPresentReason reason) {
  // This method is called when the rendering process exit presents.
  if (render_loop_ && render_loop_->IsRunning()) {
    render_loop_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&XRCompositorCommon::ExitPresent,
                       base::Unretained(render_loop_.get()), reason));
    render_loop_->Stop();
    render_loop_.reset();
  }

  OnExitPresent();
  exclusive_controller_receiver_.reset();

  extension_helper_.reset();
  if (instance_ != XR_NULL_HANDLE) {
    platform_helper_->DestroyInstance(instance_);
  }
}

void OpenXrDevice::OnPresentingControllerMojoConnectionError() {
  ForceEndSession(ExitXrPresentReason::kMojoConnectionError);
}

void OpenXrDevice::ShutdownSession(
    mojom::XRRuntime::ShutdownSessionCallback callback) {
  ForceEndSession(ExitXrPresentReason::kBrowserShutdown);
  std::move(callback).Run();
}

void OpenXrDevice::SetFrameDataRestricted(bool restricted) {
  // Presentation sessions can not currently be restricted.
  NOTREACHED();
}

void OpenXrDevice::CreateImmersiveOverlay(
    mojo::PendingReceiver<mojom::ImmersiveOverlay> overlay_receiver) {
  // This should only be triggered if we have a session
  if (render_loop_ && render_loop_->IsRunning()) {
    render_loop_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestOverlay,
                                  base::Unretained(render_loop_.get()),
                                  std::move(overlay_receiver)));
  } else {
    overlay_receiver_ = std::move(overlay_receiver);
  }
}

}  // namespace device
