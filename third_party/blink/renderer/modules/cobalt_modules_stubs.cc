// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include <utility>

#include "third_party/blink/renderer/modules/ml/navigator_ml.h"  // nogncheck
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"  // nogncheck
#include "third_party/blink/renderer/modules/xr/xr_session.h"  // nogncheck
#include "third_party/blink/renderer/modules/xr/xr_system.h"  // nogncheck
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"

namespace blink {

static void DummyInstallInterfaceTemplateFunc(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Template> interface_template) {
  // Do nothing
}

static const WrapperTypeInfo g_dummy_wrapper_type_info = {
    gin::kEmbedderBlink,
    DummyInstallInterfaceTemplateFunc,
    nullptr,
    "Dummy",
    nullptr,
    v8::CppHeapPointerTag::kDefaultTag,
    v8::CppHeapPointerTag::kDefaultTag,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kIdlInterface,
    false,
};

// --- V8 WrapperTypeInfo Stubs ---

#define STUB_V8_WRAPPER(ClassName) \
  class ClassName { public: static const WrapperTypeInfo wrapper_type_info_; }; \
  const WrapperTypeInfo ClassName::wrapper_type_info_ = g_dummy_wrapper_type_info;

// WebNN V8 Wrappers
STUB_V8_WRAPPER(V8ML)
STUB_V8_WRAPPER(V8MLContext)
STUB_V8_WRAPPER(V8MLTensor)
STUB_V8_WRAPPER(V8MLGraph)
STUB_V8_WRAPPER(V8MLGraphBuilder)
STUB_V8_WRAPPER(V8MLOperand)

// WebXR V8 Wrappers
STUB_V8_WRAPPER(V8XRAnchor)
STUB_V8_WRAPPER(V8XRAnchorSet)
STUB_V8_WRAPPER(V8XRBoundedReferenceSpace)
STUB_V8_WRAPPER(V8XRCamera)
STUB_V8_WRAPPER(V8XRCompositionLayer)
STUB_V8_WRAPPER(V8XRCPUDepthInformation)
STUB_V8_WRAPPER(V8XRDepthInformation)
STUB_V8_WRAPPER(V8XRDOMOverlayState)
STUB_V8_WRAPPER(V8XRFrame)
STUB_V8_WRAPPER(V8XRGPUBinding)
STUB_V8_WRAPPER(V8XRGPUSubImage)
STUB_V8_WRAPPER(V8XRHand)
STUB_V8_WRAPPER(V8XRHitTestResult)
STUB_V8_WRAPPER(V8XRHitTestSource)
STUB_V8_WRAPPER(V8XRImageTrackingResult)
STUB_V8_WRAPPER(V8XRInputSource)
STUB_V8_WRAPPER(V8XRInputSourceArray)
STUB_V8_WRAPPER(V8XRInputSourceEvent)
STUB_V8_WRAPPER(V8XRInputSourcesChangeEvent)
STUB_V8_WRAPPER(V8XRJointPose)
STUB_V8_WRAPPER(V8XRJointSpace)
STUB_V8_WRAPPER(V8XRLayer)
STUB_V8_WRAPPER(V8XRLightEstimate)
STUB_V8_WRAPPER(V8XRLightProbe)
STUB_V8_WRAPPER(V8XRPlane)
STUB_V8_WRAPPER(V8XRPlaneSet)
STUB_V8_WRAPPER(V8XRPose)
STUB_V8_WRAPPER(V8XRProjectionLayer)
STUB_V8_WRAPPER(V8XRRay)
STUB_V8_WRAPPER(V8XRReferenceSpace)
STUB_V8_WRAPPER(V8XRReferenceSpaceEvent)
STUB_V8_WRAPPER(V8XRRenderState)
STUB_V8_WRAPPER(V8XRRigidTransform)
STUB_V8_WRAPPER(V8XRSession)
STUB_V8_WRAPPER(V8XRSessionEvent)
STUB_V8_WRAPPER(V8XRSpace)
STUB_V8_WRAPPER(V8XRSubImage)
STUB_V8_WRAPPER(V8XRSystem)
STUB_V8_WRAPPER(V8XRTransientInputHitTestResult)
STUB_V8_WRAPPER(V8XRTransientInputHitTestSource)
STUB_V8_WRAPPER(V8XRView)
STUB_V8_WRAPPER(V8XRViewerPose)
STUB_V8_WRAPPER(V8XRViewport)
STUB_V8_WRAPPER(V8XRWebGLBinding)
STUB_V8_WRAPPER(V8XRWebGLDepthInformation)
STUB_V8_WRAPPER(V8XRWebGLLayer)
STUB_V8_WRAPPER(V8XRWebGLSubImage)
STUB_V8_WRAPPER(V8XRWebGLContext)

#undef STUB_V8_WRAPPER

// --- WebNN C++ Stubs ---

ML* NavigatorML::ml(NavigatorBase& navigator) {
  return nullptr;
}

// --- WebXR C++ Stubs ---

XRSystem* XRSystem::From(Document& document) {
  return nullptr;
}

XRSystem* XRSystem::FromIfExists(Document& document) {
  return nullptr;
}

XRSystem* XRSystem::xr(Navigator& navigator) {
  return nullptr;
}

void XRSystem::MakeXrCompatibleAsync(device::mojom::blink::VRService::MakeXrCompatibleCallback callback) {
  std::move(callback).Run(device::mojom::blink::XrCompatibleResult::kNoDeviceAvailable);
}

void XRSystem::MakeXrCompatibleSync(device::mojom::XrCompatibleResult* xr_compatible_result) {
  if (xr_compatible_result) {
    *xr_compatible_result = device::mojom::XrCompatibleResult::kNoDeviceAvailable;
  }
}

XRFrameProvider* XRSystem::frameProvider() {
  return nullptr;
}

void XRFrameProvider::AddImmersiveSessionObserver(ImmersiveSessionObserver* observer) {}

void XRSession::ScheduleVideoFrameCallbacksExecution(ExecuteVfcCallback callback) {}

}  // namespace blink
