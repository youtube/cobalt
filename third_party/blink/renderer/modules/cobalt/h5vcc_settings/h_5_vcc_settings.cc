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

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "cobalt/browser/h5vcc_settings/public/mojom/h5vcc_settings.mojom-blink.h"
#include "media/base/decoder_buffer.h"
#include "media/base/stream_parser.h"
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
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

constexpr char kMediaAppendFirstSegmentSynchronously[] =
    "Media.AppendFirstSegmentSynchronously";
constexpr char kMediaIncrementalParseLookAhead[] =
    "Media.IncrementalParseLookAhead";
constexpr char kDecoderBufferSettingPrefix[] = "DecoderBuffer.";

constexpr char kEnableMediaBufferPoolAllocatorStrategy[] =
    "DecoderBuffer.EnableMediaBufferPoolAllocatorStrategy";
static_assert(
    std::string_view(kEnableMediaBufferPoolAllocatorStrategy)
        .substr(0, std::string_view(kDecoderBufferSettingPrefix).size()) ==
    kDecoderBufferSettingPrefix);

constexpr char kEnableInPlaceReuseAllocatorBase[] =
    "DecoderBuffer.EnableInPlaceReuseAllocatorBase";
static_assert(
    std::string_view(kEnableInPlaceReuseAllocatorBase)
        .substr(0, std::string_view(kDecoderBufferSettingPrefix).size()) ==
    kDecoderBufferSettingPrefix);

using EnableFunction = void (*)();
using SettingsMap = WTF::HashMap<WTF::String, EnableFunction>;

struct ScriptContext {
  ScriptState* script_state;
  const ExceptionContext& exception_context;
};

template <typename Callback>
ScriptPromise ProcessLongAsBool(const ScriptContext& context,
                                const V8UnionLongOrString* value,
                                const String& name,
                                Callback callback) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      context.script_state, context.exception_context);

  if (!value->IsLong()) {
    LOG(WARNING) << "The value for '" << name << "' must be a number.";
    resolver->Reject(V8ThrowException::CreateTypeError(
        context.script_state->GetIsolate(),
        "The value for '" + name + "' must be a number."));
    return resolver->Promise();
  }
  callback(resolver, value->GetAsLong() != 0);
  return resolver->Promise();
}

const SettingsMap& GetDecoderBufferSettings() {
  static const base::NoDestructor<SettingsMap> settings({
      {kEnableMediaBufferPoolAllocatorStrategy,
       &::media::DecoderBuffer::EnableMediaBufferPoolStrategy},
      {kEnableInPlaceReuseAllocatorBase,
       &::media::DecoderBuffer::EnableInPlaceReuseAllocatorBase},
  });
  return *settings;
}

// Ideally this function should be moved to decoder_buffer.h.  It's kept here as
// H5vccSettings will soon be deprecated and it's easier to remove from here.
ScriptPromise ProcessDecoderBufferSettings(const ScriptContext& context,
                                           const WTF::String& name,
                                           const V8UnionLongOrString* value) {
  DCHECK(name.StartsWith(kDecoderBufferSettingPrefix));

  return ProcessLongAsBool(
      context, value, name, [&](ScriptPromiseResolver* resolver, bool enable) {
        const auto& settings = GetDecoderBufferSettings();
        auto it = settings.find(name);

        if (it == settings.end()) {
          LOG(WARNING) << name << " isn't a supported setting.";
          // An unknown setting leads to TypeError.
          resolver->Reject(V8ThrowException::CreateTypeError(
              context.script_state->GetIsolate(),
              name + " isn't a supported setting."));
          return;
        }

        if (!enable) {
          LOG(WARNING) << name << " cannot be disabled.";
          resolver->Reject(V8ThrowException::CreateTypeError(
              context.script_state->GetIsolate(),
              name + " cannot be disabled."));
          return;
        }

        LOG(INFO) << "Enabling " << name << ".";
        it->value();
        resolver->Resolve();
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
  ScriptContext context{script_state, exception_state.GetContext()};

  if (name.StartsWith(kDecoderBufferSettingPrefix)) {
    return ProcessDecoderBufferSettings(context, name, value);
  }
  if (name == kMediaAppendFirstSegmentSynchronously) {
    return ProcessLongAsBool(
        context, value, name,
        [&](ScriptPromiseResolver* resolver, bool enable) {
          append_first_segment_synchronously_ = enable;
          if (enable) {
            LOG(INFO)
                << "Enable synchronous append of first media source segment.";
          } else {
            LOG(INFO) << "Disable synchronous append of first media source"
                      << " segment.";
          }
          resolver->Resolve();
        });
  }
  if (name == kMediaIncrementalParseLookAhead) {
    return ProcessLongAsBool(
        context, value, name,
        [&](ScriptPromiseResolver* resolver, bool enable) {
          if (enable) {
            LOG(INFO) << "Enable incremental parse look ahead.";
            ::media::StreamParser::SetEnableIncrementalParseLookAhead(true);
            resolver->Resolve();
          } else {
            LOG(WARNING) << kMediaIncrementalParseLookAhead
                         << " cannot be disabled.";
            resolver->Reject(V8ThrowException::CreateTypeError(
                context.script_state->GetIsolate(),
                kMediaIncrementalParseLookAhead +
                    String(" cannot be disabled.")));
          }
        });
  }

  EnsureReceiverIsBound();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  h5vcc_settings::mojom::blink::ValuePtr mojo_value;
  if (value->IsString()) {
    mojo_value = h5vcc_settings::mojom::blink::Value::NewStringValue(
        value->GetAsString());
  } else if (value->IsLong()) {
    mojo_value =
        h5vcc_settings::mojom::blink::Value::NewIntValue(value->GetAsLong());
  } else {
    NOTREACHED();
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "Unsupported type."));
    return resolver->Promise();
  }

  ongoing_requests_.insert(resolver);
  remote_h5vcc_settings_->SetValue(
      name, std::move(mojo_value),
      WTF::BindOnce(&H5vccSettings::OnSetValueFinished, WrapPersistent(this),
                    WrapPersistent(resolver)));
  return resolver->Promise();
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
