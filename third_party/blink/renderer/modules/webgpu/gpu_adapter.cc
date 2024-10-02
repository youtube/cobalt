// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_device_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_request_adapter_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_enum_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter_info.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_limits.h"
#include "third_party/blink/renderer/modules/webgpu/string_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

absl::optional<V8GPUFeatureName::Enum> ToV8FeatureNameEnum(WGPUFeatureName f) {
  switch (f) {
    case WGPUFeatureName_Depth32FloatStencil8:
      return V8GPUFeatureName::Enum::kDepth32FloatStencil8;
    case WGPUFeatureName_TimestampQuery:
      return V8GPUFeatureName::Enum::kTimestampQuery;
    case WGPUFeatureName_TimestampQueryInsidePasses:
      return V8GPUFeatureName::Enum::kTimestampQueryInsidePasses;
    case WGPUFeatureName_PipelineStatisticsQuery:
      return V8GPUFeatureName::Enum::kPipelineStatisticsQuery;
    case WGPUFeatureName_TextureCompressionBC:
      return V8GPUFeatureName::Enum::kTextureCompressionBc;
    case WGPUFeatureName_TextureCompressionETC2:
      return V8GPUFeatureName::Enum::kTextureCompressionEtc2;
    case WGPUFeatureName_TextureCompressionASTC:
      return V8GPUFeatureName::Enum::kTextureCompressionAstc;
    case WGPUFeatureName_IndirectFirstInstance:
      return V8GPUFeatureName::Enum::kIndirectFirstInstance;
    case WGPUFeatureName_DepthClipControl:
      return V8GPUFeatureName::Enum::kDepthClipControl;
    case WGPUFeatureName_RG11B10UfloatRenderable:
      return V8GPUFeatureName::Enum::kRg11B10UfloatRenderable;
    case WGPUFeatureName_BGRA8UnormStorage:
      return V8GPUFeatureName::Enum::kBgra8UnormStorage;
    case WGPUFeatureName_ChromiumExperimentalDp4a:
      return V8GPUFeatureName::Enum::kChromiumExperimentalDp4A;
    case WGPUFeatureName_ShaderF16:
      return V8GPUFeatureName::Enum::kShaderF16;
    default:
      return absl::nullopt;
  }
}

}  // anonymous namespace

namespace {

GPUSupportedFeatures* MakeFeatureNameSet(const DawnProcTable& procs,
                                         WGPUAdapter adapter) {
  GPUSupportedFeatures* features = MakeGarbageCollected<GPUSupportedFeatures>();
  DCHECK(features->FeatureNameSet().empty());

  size_t feature_count = procs.adapterEnumerateFeatures(adapter, nullptr);
  DCHECK(feature_count <= std::numeric_limits<wtf_size_t>::max());

  Vector<WGPUFeatureName> feature_names(static_cast<wtf_size_t>(feature_count));
  procs.adapterEnumerateFeatures(adapter, feature_names.data());

  for (WGPUFeatureName f : feature_names) {
    auto feature_name_enum_optional = ToV8FeatureNameEnum(f);
    if (feature_name_enum_optional) {
      features->AddFeatureName(
          V8GPUFeatureName(feature_name_enum_optional.value()));
    }
  }
  return features;
}

}  // anonymous namespace

GPUAdapter::GPUAdapter(
    GPU* gpu,
    WGPUAdapter handle,
    scoped_refptr<DawnControlClientHolder> dawn_control_client)
    : DawnObjectBase(dawn_control_client), handle_(handle), gpu_(gpu) {
  WGPUAdapterProperties properties = {};
  GetProcs().adapterGetProperties(handle_, &properties);
  is_fallback_adapter_ = properties.adapterType == WGPUAdapterType_CPU;
  backend_type_ = properties.backendType;

  vendor_ = properties.vendorName;
  architecture_ = properties.architecture;
  if (properties.deviceID <= 0xffff) {
    device_ = String::Format("0x%04x", properties.deviceID);
  } else {
    device_ = String::Format("0x%08x", properties.deviceID);
  }
  description_ = properties.name;
  driver_ = properties.driverDescription;

  WGPUSupportedLimits limits = {};
  GetProcs().adapterGetLimits(handle_, &limits);
  limits_ = MakeGarbageCollected<GPUSupportedLimits>(limits);

  features_ = MakeFeatureNameSet(GetProcs(), handle_);
}

void GPUAdapter::AddConsoleWarning(ExecutionContext* execution_context,
                                   const char* message) {
  if (execution_context && allowed_console_warnings_remaining_ > 0) {
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        StringFromASCIIAndUTF8(message));
    execution_context->AddConsoleMessage(console_message);

    allowed_console_warnings_remaining_--;
    if (allowed_console_warnings_remaining_ == 0) {
      auto* final_message = MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "WebGPU: too many warnings, no more warnings will be reported to the "
          "console for this GPUAdapter.");
      execution_context->AddConsoleMessage(final_message);
    }
  }
}

GPUSupportedFeatures* GPUAdapter::features() const {
  return features_;
}

bool GPUAdapter::isFallbackAdapter() const {
  return is_fallback_adapter_;
}

WGPUBackendType GPUAdapter::backendType() const {
  return backend_type_;
}

bool GPUAdapter::SupportsMultiPlanarFormats() const {
  return GetProcs().adapterHasFeature(handle_,
                                      WGPUFeatureName_DawnMultiPlanarFormats);
}

void GPUAdapter::OnRequestDeviceCallback(ScriptState* script_state,
                                         ScriptPromiseResolver* resolver,
                                         const GPUDeviceDescriptor* descriptor,
                                         WGPURequestDeviceStatus status,
                                         WGPUDevice dawn_device,
                                         const char* error_message) {
  switch (status) {
    case WGPURequestDeviceStatus_Success: {
      DCHECK(dawn_device);

      if (is_consumed_) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidStateError,
            "The adapter is invalid because it has already been used to create "
            "a device. NOTE: The behavior in this error case may change in a "
            "future release."));
        break;
      }
      is_consumed_ = true;

      ExecutionContext* execution_context =
          ExecutionContext::From(script_state);
      auto* device = MakeGarbageCollected<GPUDevice>(
          execution_context, GetDawnControlClient(), this, dawn_device,
          descriptor);
      resolver->Resolve(device);

      ukm::builders::ClientRenderingAPI(execution_context->UkmSourceID())
          .SetGPUDevice(static_cast<int>(true))
          .Record(execution_context->UkmRecorder());
      break;
    }

    case WGPURequestDeviceStatus_Error:
    case WGPURequestDeviceStatus_Unknown:
      DCHECK_EQ(dawn_device, nullptr);
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError,
          StringFromASCIIAndUTF8(error_message)));
      break;
    default:
      NOTREACHED();
  }
}

ScriptPromise GPUAdapter::requestDevice(ScriptState* script_state,
                                        GPUDeviceDescriptor* descriptor) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state,
      ExceptionContext(ExceptionContext::Context::kOperationInvoke,
                       "GPUAdapter", "requestDevice"));
  ScriptPromise promise = resolver->Promise();

  WGPUDeviceDescriptor dawn_desc = {};

  WGPURequiredLimits required_limits = {};
  if (descriptor->hasRequiredLimits()) {
    dawn_desc.requiredLimits = &required_limits;
    GPUSupportedLimits::MakeUndefined(&required_limits);
    DOMException* exception = GPUSupportedLimits::Populate(
        &required_limits, descriptor->requiredLimits());
    if (exception) {
      resolver->Reject(exception);
      return promise;
    }
  }

  Vector<WGPUFeatureName> required_features;
  if (descriptor->hasRequiredFeatures()) {
    // Insert features into a set to dedup them.
    HashSet<WGPUFeatureName> required_features_set;
    for (const V8GPUFeatureName& f : descriptor->requiredFeatures()) {
      // If the feature is not a valid feature reject with a type error.
      if (!features_->has(f.AsEnum())) {
        resolver->RejectWithTypeError(
            String::Format("Unsupported feature: %s", f.AsCStr()));
        return promise;
      }
      required_features_set.insert(AsDawnEnum(f));
    }

    // Then, push the deduped features into a vector.
    required_features.AppendRange(required_features_set.begin(),
                                  required_features_set.end());
    dawn_desc.requiredFeatures = required_features.data();
    dawn_desc.requiredFeaturesCount = required_features.size();
  }

  auto* callback = BindWGPUOnceCallback(
      &GPUAdapter::OnRequestDeviceCallback, WrapPersistent(this),
      WrapPersistent(script_state), WrapPersistent(resolver),
      WrapPersistent(descriptor));

  GetProcs().adapterRequestDevice(
      handle_, &dawn_desc, callback->UnboundCallback(), callback->AsUserdata());
  EnsureFlush(ToEventLoop(script_state));

  return promise;
}

ScriptPromise GPUAdapter::requestAdapterInfo(
    ScriptState* script_state,
    const Vector<String>& unmask_hints) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // If any unmask hints have been given, the method must also have been called
  // during user activation. If not, reject the promise.
  if (unmask_hints.size()) {
    LocalDOMWindow* domWindow = gpu_->DomWindow();
    if (!domWindow ||
        !LocalFrame::HasTransientUserActivation(domWindow->GetFrame())) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "requestAdapterInfo requires user activation if any unmaskHints are "
          "given."));
      return promise;
    }

    // TODO(crbug.com/1405528): Handling unmask hints is not yet supported.
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "Passing unmaskHints to requestAdapterInfo is not yet implemented. In "
        "the future, doing so may trigger a permissions prompt."));

    return promise;
  }

  GPUAdapterInfo* adapter_info;
  if (RuntimeEnabledFeatures::WebGPUDeveloperFeaturesEnabled()) {
    // If WebGPU developer features have been enabled then provide unmasked
    // versions of all available adapter info values, including some that are
    // only available when the flag is enabled.
    adapter_info = MakeGarbageCollected<GPUAdapterInfo>(
        vendor_, architecture_, device_, description_, driver_);
  } else {
    // TODO(dawn:1427): If unmask_hints are given ask the user for consent to
    // expose more information and, if given, include device_ and description_
    // in the returned GPUAdapterInfo.
    adapter_info = MakeGarbageCollected<GPUAdapterInfo>(vendor_, architecture_);
  }

  resolver->Resolve(adapter_info);

  return promise;
}

void GPUAdapter::Trace(Visitor* visitor) const {
  visitor->Trace(gpu_);
  visitor->Trace(features_);
  visitor->Trace(limits_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
