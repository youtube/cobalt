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

#include "third_party/blink/renderer/modules/bluetooth/bluetooth.h"  // nogncheck
#include "third_party/blink/renderer/modules/hid/hid.h"  // nogncheck
#include "third_party/blink/renderer/modules/ml/navigator_ml.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_limits.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter_info.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/gpu_memory_heap_info.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/gpu_subgroup_matrix_config.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/gpu_device_lost_info.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/gpu_external_texture.h"  // nogncheck
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/dawn_enum_conversions.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/gpu.h"  // nogncheck
#include "third_party/blink/renderer/modules/webgpu/wgsl_language_features.h"  // nogncheck
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_2d_gpu_transfer_option.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_format.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/serial/serial.h"  // nogncheck
#include "third_party/blink/renderer/modules/webusb/usb.h"  // nogncheck
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
    {gin::kEmbedderBlink},
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

#define STUB_V8_WRAPPER(ClassName) \
  class ClassName { public: static const WrapperTypeInfo wrapper_type_info_; }; \
  const WrapperTypeInfo ClassName::wrapper_type_info_ = g_dummy_wrapper_type_info;

// Web Bluetooth V8 Wrappers
STUB_V8_WRAPPER(V8Bluetooth)
STUB_V8_WRAPPER(V8BluetoothAdvertisingEvent)
STUB_V8_WRAPPER(V8BluetoothCharacteristicProperties)
STUB_V8_WRAPPER(V8BluetoothDevice)
STUB_V8_WRAPPER(V8BluetoothLEScan)
STUB_V8_WRAPPER(V8BluetoothManufacturerDataMap)
STUB_V8_WRAPPER(V8BluetoothRemoteGATTCharacteristic)
STUB_V8_WRAPPER(V8BluetoothRemoteGATTDescriptor)
STUB_V8_WRAPPER(V8BluetoothRemoteGATTServer)
STUB_V8_WRAPPER(V8BluetoothRemoteGATTService)
STUB_V8_WRAPPER(V8BluetoothServiceDataMap)
STUB_V8_WRAPPER(V8BluetoothUUID)

// WebUSB V8 Wrappers
STUB_V8_WRAPPER(V8USB)
STUB_V8_WRAPPER(V8USBAlternateInterface)
STUB_V8_WRAPPER(V8USBConfiguration)
STUB_V8_WRAPPER(V8USBConnectionEvent)
STUB_V8_WRAPPER(V8USBDevice)
STUB_V8_WRAPPER(V8USBEndpoint)
STUB_V8_WRAPPER(V8USBInTransferResult)
STUB_V8_WRAPPER(V8USBInterface)
STUB_V8_WRAPPER(V8USBIsochronousInTransferPacket)
STUB_V8_WRAPPER(V8USBIsochronousInTransferResult)
STUB_V8_WRAPPER(V8USBIsochronousOutTransferPacket)
STUB_V8_WRAPPER(V8USBIsochronousOutTransferResult)
STUB_V8_WRAPPER(V8USBOutTransferResult)

// WebHID V8 Wrappers
STUB_V8_WRAPPER(V8HID)
STUB_V8_WRAPPER(V8HIDConnectionEvent)
STUB_V8_WRAPPER(V8HIDDevice)
STUB_V8_WRAPPER(V8HIDInputReportEvent)

// Web Serial V8 Wrappers
STUB_V8_WRAPPER(V8Serial)
STUB_V8_WRAPPER(V8SerialPort)

// Web NFC V8 Wrappers
STUB_V8_WRAPPER(V8NDEFMessage)
STUB_V8_WRAPPER(V8NDEFReader)
STUB_V8_WRAPPER(V8NDEFReadingEvent)
STUB_V8_WRAPPER(V8NDEFRecord)

// WebNN / ML V8 Wrappers
STUB_V8_WRAPPER(V8ML)
STUB_V8_WRAPPER(V8MLGraph)
STUB_V8_WRAPPER(V8MLGraphBuilder)
STUB_V8_WRAPPER(V8MLOperand)
STUB_V8_WRAPPER(V8MLTensor)
STUB_V8_WRAPPER(V8MLContext)

// AI V8 Wrappers
STUB_V8_WRAPPER(V8CreateMonitor)
STUB_V8_WRAPPER(V8LanguageModel)
STUB_V8_WRAPPER(V8LanguageModelParams)
STUB_V8_WRAPPER(V8LanguageDetector)
STUB_V8_WRAPPER(V8Proofreader)
STUB_V8_WRAPPER(V8Rewriter)
STUB_V8_WRAPPER(V8Summarizer)
STUB_V8_WRAPPER(V8Translator)
STUB_V8_WRAPPER(V8Writer)

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

// WebGPU V8 Wrappers
STUB_V8_WRAPPER(V8GPU)
STUB_V8_WRAPPER(V8GPUAdapter)
STUB_V8_WRAPPER(V8GPUAdapterInfo)
STUB_V8_WRAPPER(V8GPUBindGroup)
STUB_V8_WRAPPER(V8GPUBindGroupLayout)
STUB_V8_WRAPPER(V8GPUBuffer)
STUB_V8_WRAPPER(V8GPUBufferUsage)
STUB_V8_WRAPPER(V8GPUCanvasContext)
STUB_V8_WRAPPER(V8GPUColorWrite)
STUB_V8_WRAPPER(V8GPUCommandBuffer)
STUB_V8_WRAPPER(V8GPUCommandEncoder)
STUB_V8_WRAPPER(V8GPUCompilationInfo)
STUB_V8_WRAPPER(V8GPUCompilationMessage)
STUB_V8_WRAPPER(V8GPUComputePassEncoder)
STUB_V8_WRAPPER(V8GPUComputePipeline)
STUB_V8_WRAPPER(V8GPUDevice)
STUB_V8_WRAPPER(V8GPUDeviceLostInfo)
STUB_V8_WRAPPER(V8GPUError)
STUB_V8_WRAPPER(V8GPUExternalTexture)
STUB_V8_WRAPPER(V8GPUHeapProperty)
STUB_V8_WRAPPER(V8GPUInternalError)
STUB_V8_WRAPPER(V8GPUMapMode)
STUB_V8_WRAPPER(V8GPUMemoryHeapInfo)
STUB_V8_WRAPPER(V8GPUOutOfMemoryError)
STUB_V8_WRAPPER(V8GPUPipelineError)
STUB_V8_WRAPPER(V8GPUPipelineLayout)
STUB_V8_WRAPPER(V8GPUQuerySet)
STUB_V8_WRAPPER(V8GPUQueue)
STUB_V8_WRAPPER(V8GPURenderBundle)
STUB_V8_WRAPPER(V8GPURenderBundleEncoder)
STUB_V8_WRAPPER(V8GPURenderPassEncoder)
STUB_V8_WRAPPER(V8GPURenderPipeline)
STUB_V8_WRAPPER(V8GPUSampler)
STUB_V8_WRAPPER(V8GPUShaderModule)
STUB_V8_WRAPPER(V8GPUShaderStage)
STUB_V8_WRAPPER(V8GPUSubgroupMatrixConfig)
STUB_V8_WRAPPER(V8GPUSupportedFeatures)
STUB_V8_WRAPPER(V8GPUSupportedLimits)
STUB_V8_WRAPPER(V8GPUTexture)
STUB_V8_WRAPPER(V8GPUTextureUsage)
STUB_V8_WRAPPER(V8GPUTextureView)
STUB_V8_WRAPPER(V8GPUUncapturedErrorEvent)
STUB_V8_WRAPPER(V8GPUValidationError)


#undef STUB_V8_WRAPPER

// --- Web Bluetooth, WebUSB, WebHID, and Web Serial C++ Stubs ---

const WrapperTypeInfo& Bluetooth::wrapper_type_info_ = g_dummy_wrapper_type_info;
const WrapperTypeInfo& USB::wrapper_type_info_ = g_dummy_wrapper_type_info;
const WrapperTypeInfo& HID::wrapper_type_info_ = g_dummy_wrapper_type_info;
const WrapperTypeInfo& Serial::wrapper_type_info_ = g_dummy_wrapper_type_info;

Bluetooth* Bluetooth::bluetooth(Navigator&) {
  return nullptr;
}

USB* USB::usb(NavigatorBase&) {
  return nullptr;
}

HID* HID::hid(NavigatorBase&) {
  return nullptr;
}

Serial* Serial::serial(NavigatorBase&) {
  return nullptr;
}

// --- WebNN / ML C++ Stubs ---

const char NavigatorML::kSupplementName[] = "NavigatorML";

ML* NavigatorML::ml(NavigatorBase&) {
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

// --- WebGPU C++ Stubs ---

const WrapperTypeInfo& GPUTexture::wrapper_type_info_ = g_dummy_wrapper_type_info;
const WrapperTypeInfo& GPU::wrapper_type_info_ = g_dummy_wrapper_type_info;
const WrapperTypeInfo& GPUDevice::wrapper_type_info_ = g_dummy_wrapper_type_info;
const WrapperTypeInfo& GPUAdapter::wrapper_type_info_ = g_dummy_wrapper_type_info;
const WrapperTypeInfo& GPUAdapterInfo::wrapper_type_info_ = g_dummy_wrapper_type_info;
const WrapperTypeInfo& GPUBuffer::wrapper_type_info_ = g_dummy_wrapper_type_info;

DawnObjectBase::DawnObjectBase(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    const String& label)
    : dawn_control_client_(std::move(dawn_control_client)), label_(label) {}

const scoped_refptr<DawnControlClientHolder>&
DawnObjectBase::GetDawnControlClient() const {
  return dawn_control_client_;
}

void DawnObjectBase::setLabel(const String& value) {}
void DawnObjectBase::EnsureFlush(scheduler::EventLoop& event_loop) {}
void DawnObjectBase::FlushNow() {}

DawnObjectImpl::DawnObjectImpl(GPUDevice* device, const String& label)
    : DawnObjectBase(nullptr, label), device_(device) {}

DawnObjectImpl::~DawnObjectImpl() = default;

const wgpu::Device& DawnObjectImpl::GetDeviceHandle() const {
  return device_->GetHandle();
}

void DawnObjectImpl::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  ScriptWrappable::Trace(visitor);
}
void GPUAdapter::Trace(Visitor* visitor) const {
  visitor->Trace(gpu_);
  visitor->Trace(limits_);
  visitor->Trace(features_);
  visitor->Trace(info_);
  ScriptWrappable::Trace(visitor);
}

void GPUAdapterInfo::Trace(Visitor* visitor) const {
  visitor->Trace(memory_heaps_);
  visitor->Trace(subgroup_matrix_configs_);
  ScriptWrappable::Trace(visitor);
}

void GPU::Trace(Visitor* visitor) const {
  visitor->Trace(wgsl_language_features_);
  visitor->Trace(mappable_buffers_);
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void GPUDevice::Trace(Visitor* visitor) const {
  visitor->Trace(adapter_);
  visitor->Trace(features_);
  visitor->Trace(limits_);
  visitor->Trace(adapter_info_);
  visitor->Trace(queue_);
  visitor->Trace(lost_property_);
  visitor->Trace(textures_with_mailbox_);
  visitor->Trace(mappable_buffers_);
  visitor->Trace(external_texture_cache_);
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

GPUTexture::GPUTexture(GPUDevice* device,
                       wgpu::TextureFormat format,
                       wgpu::TextureUsage usage,
                       scoped_refptr<WebGPUMailboxTexture> mailbox_texture,
                       const String& label)
    : DawnObject<wgpu::Texture>(device, mailbox_texture ? mailbox_texture->GetTexture() : nullptr, label),
      dimension_(wgpu::TextureDimension::e2D),
      format_(format),
      usage_(usage),
      mailbox_texture_(std::move(mailbox_texture)) {}

GPUTexture::~GPUTexture() {}

void GPUTexture::destroy() {}

scoped_refptr<WebGPUMailboxTexture> GPUTexture::GetMailboxTexture() {
  return mailbox_texture_;
}

void GPUTexture::DissociateMailbox() {}

const char* const V8GPUTextureFormat::string_table_[] = { "r8unorm" };

V8GPUTextureFormat FromDawnEnum(wgpu::TextureFormat dawn_enum) {
  return V8GPUTextureFormat(V8GPUTextureFormat::Enum::kR8Unorm);
}

GPU* GPU::gpu(NavigatorBase& navigator) {
  return nullptr;
}

bool WGSLLanguageFeatures::hasForBinding(ScriptState* script_state, const String&, ExceptionState&) const {
  return false;
}

bool GPUSupportedFeatures::hasForBinding(ScriptState* script_state, const String&, ExceptionState&) const {
  return false;
}

// --- Canvas 2D GPU Transfer Stubs ---

Canvas2dGPUTransferOption::Canvas2dGPUTransferOption(v8::Isolate* isolate) {}

void Canvas2dGPUTransferOption::Trace(Visitor* visitor) const {
  visitor->Trace(member_device_);
  bindings::InputDictionaryBase::Trace(visitor);
}

Canvas2dGPUTransferOption* Canvas2dGPUTransferOption::Create(v8::Isolate* isolate, v8::Local<v8::Value> v8_value, ExceptionState& exception_state) {
  return MakeGarbageCollected<Canvas2dGPUTransferOption>(isolate);
}

String Canvas2dGPUTransferOption::getLabelOr(const String& fallback_value) const {
  if (!hasLabel()) {
    return fallback_value;
  }
  return label();
}

String Canvas2dGPUTransferOption::getLabelOr(String&& fallback_value) const {
  if (!hasLabel()) {
    return fallback_value;
  }
  return label();
}

// --- BaseRenderingContext2D WebGPU Stubs ---

GPUTexture* BaseRenderingContext2D::transferToGPUTexture(
    const Canvas2dGPUTransferOption* options,
    ExceptionState& exception_state) {
  return nullptr;
}

void BaseRenderingContext2D::transferBackFromGPUTexture(
    ExceptionState& exception_state) {
}

// --- GC Trace Stubs for incomplete types ---

class GPUMappedDOMArrayBuffer : public DOMArrayBuffer {
  // dummy
};

void GPUBuffer::Trace(Visitor* visitor) const {
  visitor->Trace(mapped_array_buffers_);
  DawnObjectImpl::Trace(visitor);
}

void ExternalTextureCache::Trace(Visitor* visitor) const {
  visitor->Trace(from_html_video_element_);
  visitor->Trace(from_video_frame_);
  visitor->Trace(expire_set_);
  visitor->Trace(device_);
}

V8GPUTextureFormat BaseRenderingContext2D::getTextureFormat() const {
  return V8GPUTextureFormat(V8GPUTextureFormat::Enum::kR8Unorm);
}

}  // namespace blink
