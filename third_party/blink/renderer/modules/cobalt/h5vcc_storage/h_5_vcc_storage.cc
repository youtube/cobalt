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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_storage/h_5_vcc_storage.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

H5vccStorage::H5vccStorage(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_storage_(window.GetExecutionContext()) {}

void H5vccStorage::ContextDestroyed() {}

H5vccStorageWriteTestResponse* H5vccStorage::writeTest(
    uint32_t test_size,
    const String& test_string) {
  auto* response = H5vccStorageWriteTestResponse::Create();
#if !defined(COBALT_IS_RELEASE_BUILD)
  EnsureReceiverIsBound();
  h5vcc_storage::mojom::blink::WriteTestResponsePtr mojom_response;
  remote_h5vcc_storage_->WriteTest(test_size, test_string, &mojom_response);
  if (!mojom_response.is_null()) {
    response->setBytesWritten(mojom_response->bytes_written);
    response->setError(mojom_response->error);
  }
#endif  // !defined(COBALT_IS_RELEASE_BUILD)
  return response;
}

H5vccStorageVerifyTestResponse* H5vccStorage::verifyTest(
    uint32_t test_size,
    const String& test_string) {
  auto* response = H5vccStorageVerifyTestResponse::Create();
#if !defined(COBALT_IS_RELEASE_BUILD)
  EnsureReceiverIsBound();
  h5vcc_storage::mojom::blink::VerifyTestResponsePtr mojom_response;
  remote_h5vcc_storage_->VerifyTest(test_size, test_string, &mojom_response);
  if (!mojom_response.is_null()) {
    response->setBytesRead(mojom_response->bytes_read);
    response->setError(mojom_response->error);
    response->setVerified(mojom_response->verified);
  }
#endif  // !defined(COBALT_IS_RELEASE_BUILD)
  return response;
}

void H5vccStorage::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_h5vcc_storage_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_storage_.BindNewPipeAndPassReceiver(task_runner));
}

void H5vccStorage::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(remote_h5vcc_storage_);
}

}  // namespace blink
