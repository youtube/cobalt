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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_storage/testing/h_5_vcc_storage_for_testing.h"

#include "cobalt/browser/h5vcc_storage/testing/public/mojom/h5vcc_storage_for_testing.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

class H5vccStorageForTestingImpl final
    : public GarbageCollected<H5vccStorageForTestingImpl>,
      public ExecutionContextLifecycleObserver,
      public Supplement<H5vccStorage> {
 public:
  static constexpr char kSupplementName[] = "H5vccStorageForTestingImpl";

  static H5vccStorageForTestingImpl* From(H5vccStorage& supplementable) {
    H5vccStorageForTestingImpl* supplement =
        Supplement<H5vccStorage>::From<H5vccStorageForTestingImpl>(
            supplementable);
    if (!supplement) {
      supplement =
          MakeGarbageCollected<H5vccStorageForTestingImpl>(supplementable);
      Supplement<H5vccStorage>::ProvideTo(supplementable, supplement);
    }
    return supplement;
  }

  explicit H5vccStorageForTestingImpl(H5vccStorage& h5vcc_storage)
      : Supplement<H5vccStorage>(h5vcc_storage),
        ExecutionContextLifecycleObserver(h5vcc_storage.GetExecutionContext()),
        remote_h5vcc_storage_for_testing_(h5vcc_storage.GetExecutionContext()) {
  }

  void ContextDestroyed() override {}

  H5vccStorageWriteTestResponse* writeTest(uint32_t test_size,
                                           const String& test_string) {
    CHECK_GT(test_size, 0);
    CHECK(!test_string.empty());
    auto* response = H5vccStorageWriteTestResponse::Create();
    EnsureReceiverIsBound();
    absl::optional<int32_t> bytes_written;
    String error;
    remote_h5vcc_storage_for_testing_->WriteTest(test_size, test_string,
                                                 &bytes_written, &error);
    if (bytes_written) {
      response->setBytesWritten(*bytes_written);
    }
    response->setError(!error.empty() ? error : "");
    return response;
  }

  H5vccStorageVerifyTestResponse* verifyTest(uint32_t test_size,
                                             const String& test_string) {
    CHECK_GT(test_size, 0);
    CHECK(!test_string.empty());
    auto* response = H5vccStorageVerifyTestResponse::Create();
    EnsureReceiverIsBound();
    absl::optional<int32_t> bytes_read;
    String error;
    bool verified;
    remote_h5vcc_storage_for_testing_->VerifyTest(
        test_size, test_string, &bytes_read, &error, &verified);
    if (bytes_read) {
      response->setBytesRead(*bytes_read);
    }
    response->setError(!error.empty() ? error : "");
    response->setVerified(verified);
    return response;
  }

  void EnsureReceiverIsBound() {
    DCHECK(GetExecutionContext());

    if (remote_h5vcc_storage_for_testing_.is_bound()) {
      return;
    }

    auto task_runner =
        GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
    GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        remote_h5vcc_storage_for_testing_.BindNewPipeAndPassReceiver(
            task_runner));
  }

  void Trace(Visitor* visitor) const override {
    ExecutionContextLifecycleObserver::Trace(visitor);
    visitor->Trace(remote_h5vcc_storage_for_testing_);
    Supplement<H5vccStorage>::Trace(visitor);
  }

 private:
  HeapMojoRemote<
      h5vcc_storage_for_testing::mojom::blink::H5vccStorageForTesting>
      remote_h5vcc_storage_for_testing_;
};

}  // namespace

// static
H5vccStorageWriteTestResponse* H5vccStorageForTesting::writeTest(
    H5vccStorage& h5vcc_storage,
    uint32_t test_size,
    const String& test_string) {
  return H5vccStorageForTestingImpl::From(h5vcc_storage)
      ->writeTest(test_size, test_string);
}

// static
H5vccStorageVerifyTestResponse* H5vccStorageForTesting::verifyTest(
    H5vccStorage& h5vcc_storage,
    uint32_t test_size,
    const String& test_string) {
  return H5vccStorageForTestingImpl::From(h5vcc_storage)
      ->verifyTest(test_size, test_string);
}

}  // namespace blink
