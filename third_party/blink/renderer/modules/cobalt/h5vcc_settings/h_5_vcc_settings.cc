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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_settings/h_5_vcc_settings.h"

#include "base/types/expected.h"
#include "cobalt/browser/h5vcc_settings/public/mojom/h5vcc_settings.mojom-blink.h"
#include "media/base/decoder_buffer.h"
#include "media/base/stream_parser.h"
#include "media/filters/source_buffer_state.h"
#include "media/starboard/decoder_buffer_allocator.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_long_string.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

struct SettingContext {
  ScriptState* script_state;
  const ExceptionContext& exception_context;
  const V8UnionLongOrString* value;
  const String& name;
};

using Result = base::expected<void, String>;

ScriptPromise Reject(const SettingContext& context, const String& error) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      context.script_state, context.exception_context);
  ScriptPromise promise = resolver->Promise();
  resolver->Reject(V8ThrowException::CreateTypeError(
      context.script_state->GetIsolate(), error));
  return promise;
}

ScriptPromise Resolve(const SettingContext& context) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      context.script_state, context.exception_context);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve();
  return promise;
}

template <typename T, typename Callback>
ScriptPromise ProcessSettingAs(const SettingContext& context,
                               Callback callback) {
  if (!context.value->IsLong()) {
    LOG(WARNING) << "The value for '" << context.name << "' must be a number.";
    return Reject(context,
                  "The value for '" + context.name + "' must be a number.");
  }
  const int32_t value = context.value->GetAsLong();

  Result result = callback(static_cast<T>(value));
  if (!result.has_value()) {
    LOG(WARNING) << "Failed to set " << context.name << " to " << value << ": "
                 << result.error();
    return Reject(context, result.error());
  }

  LOG(INFO) << context.name << " is set to " << value;
  return Resolve(context);
}

template <typename ActionCallback>
ScriptPromise ProcessSettingAsEnableOnly(const SettingContext& context,
                                         ActionCallback action) {
  return ProcessSettingAs<bool>(
      context,
      [name = context.name, action = std::move(action)](bool enable) -> Result {
        if (!enable) {
          return base::unexpected(name + " cannot be disabled.");
        }

        action();
        return base::ok();
      });
}

template <typename ActionCallback>
ScriptPromise ProcessSettingAsPositiveInt(const SettingContext& context,
                                          ActionCallback action) {
  return ProcessSettingAs<int>(
      context,
      [name = context.name, action = std::move(action)](int value) -> Result {
        if (value <= 0) {
          return base::unexpected(name + " must be a positive integer.");
        }

        action(value);
        return base::ok();
      });
}

}  // namespace

H5vccSettings::H5vccSettings(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_settings_(window.GetExecutionContext()) {}

void H5vccSettings::ContextDestroyed() {
  ongoing_requests_.clear();
}

ScriptPromise H5vccSettings::set(ScriptState* script_state,
                                 const WTF::String& name,
                                 const V8UnionLongOrString* value,
                                 ExceptionState& exception_state) {
  SettingContext context{script_state, exception_state.GetContext(), value,
                         name};

  if (name == "DecoderBuffer.EnableDecommitableAllocatorStrategy") {
    return ProcessSettingAsEnableOnly(context, [] {
      ::media::DecoderBufferAllocator::EnableDecommitableAllocatorStrategy();
    });
  }
  if (name == "DecoderBuffer.EnableMediaBufferPoolAllocatorStrategy") {
    return ProcessSettingAsEnableOnly(context, [] {
      ::media::DecoderBufferAllocator::EnableMediaBufferPoolStrategy();
    });
  }
  if (name == "DecoderBuffer.EnableInPlaceReuseAllocatorBase") {
    return ProcessSettingAsEnableOnly(context, [] {
      ::media::DecoderBufferAllocator::EnableInPlaceReuseAllocatorBase();
    });
  }
  // "DecoderBuffer." settings must be handled before this catch-all block.
  if (name.StartsWith("DecoderBuffer.")) {
    return Reject(context, name + " isn't a supported setting.");
  }

  if (name == "Media.AppendFirstSegmentSynchronously") {
    return ProcessSettingAs<bool>(context, [this](bool enable) -> Result {
      append_first_segment_synchronously_ = enable;
      return base::ok();
    });
  }
  if (name == "Media.ExperimentalMaxPendingBytesPerParse") {
    return ProcessSettingAsPositiveInt(context, [](int value) {
      ::media::SourceBufferState::SetMaxPendingBytesPerParseOverride(value);
    });
  }
  if (name == "Media.IncrementalParseLookAhead") {
    return ProcessSettingAsEnableOnly(context, [] {
      ::media::StreamParser::SetEnableIncrementalParseLookAhead(true);
    });
  }

  EnsureReceiverIsBound();

  h5vcc_settings::mojom::blink::ValuePtr mojo_value;
  if (value->IsString()) {
    mojo_value = h5vcc_settings::mojom::blink::Value::NewStringValue(
        value->GetAsString());
  } else if (value->IsLong()) {
    mojo_value =
        h5vcc_settings::mojom::blink::Value::NewIntValue(value->GetAsLong());
  } else {
    NOTREACHED();
    return Reject(context, "Unsupported type.");
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  ongoing_requests_.insert(resolver);
  remote_h5vcc_settings_->SetValue(
      name, std::move(mojo_value),
      WTF::BindOnce(&H5vccSettings::OnSetValueFinished, WrapPersistent(this),
                    WrapPersistent(resolver)));
  return promise;
}

void H5vccSettings::OnSetValueFinished(ScriptPromiseResolver* resolver) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve();
}

void H5vccSettings::OnConnectionError() {
  remote_h5vcc_settings_.reset();
  HeapHashSet<Member<ScriptPromiseResolver>> h5vcc_settings_promises;
  // Script may execute during a call to Resolve(). Swap these sets to prevent
  // concurrent modification.
  ongoing_requests_.swap(h5vcc_settings_promises);
  for (auto& resolver : h5vcc_settings_promises) {
    resolver->Reject("Mojo connection error.");
  }
}

void H5vccSettings::EnsureReceiverIsBound() {
  CHECK(GetExecutionContext());

  if (remote_h5vcc_settings_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_settings_.BindNewPipeAndPassReceiver(task_runner));
  remote_h5vcc_settings_.set_disconnect_handler(WTF::BindOnce(
      &H5vccSettings::OnConnectionError, WrapWeakPersistent(this)));
}

void H5vccSettings::Trace(Visitor* visitor) const {
  visitor->Trace(remote_h5vcc_settings_);
  visitor->Trace(ongoing_requests_);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
