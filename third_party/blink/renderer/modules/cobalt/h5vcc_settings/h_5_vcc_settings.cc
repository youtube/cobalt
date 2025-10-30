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
#include "cobalt/browser/h5vcc_settings/public/mojom/h5vcc_settings.mojom-blink.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_long_string.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

H5vccSettings::H5vccSettings(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_settings_(window.GetExecutionContext()) {}

void H5vccSettings::ContextDestroyed() {}

bool H5vccSettings::set(const WTF::String& name,
                        const V8UnionLongOrString* value) {
  EnsureReceiverIsBound();

  h5vcc_settings::mojom::blink::ValuePtr mojo_value;
  if (value->IsString()) {
    mojo_value = h5vcc_settings::mojom::blink::Value::NewStringValue(
        value->GetAsString());
  } else if (value->IsLong()) {
    mojo_value =
        h5vcc_settings::mojom::blink::Value::NewIntValue(value->GetAsLong());
  } else {
    // Should not happen.
    return false;
  }

  remote_h5vcc_settings_->SetValue(name, std::move(mojo_value));
  return true;
}

void H5vccSettings::OnConnectionError() {
  remote_h5vcc_settings_.reset();
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
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
