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

#ifndef CONTENT_TEST_COBALT_STUB_H5VCC_RUNTIME_IMPL_H_
#define CONTENT_TEST_COBALT_STUB_H5VCC_RUNTIME_IMPL_H_

#include "cobalt/browser/h5vcc_runtime/public/mojom/h5vcc_runtime.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

// Provides a stub implementation of the H5vccRuntime Mojo interface.
//
// This implementation does nothing useful but is used to provide a binder for
// the interface when an is_cobalt build of content_shell is run by
// run_web_tests.py. A binder is required for this interface because the
// constructor for blink::H5vccRuntime attempts to use the remote. The tests
// under third_party/blink/web_tests/wpt_internal/cobalt/h5vcc-runtime use
// MojoJS to intercept requests for the H5vccRuntime interface but cobalt's
// other web tests rely on
// WebTestContentBrowserClient::RegisterBrowserInterfaceBindersForFrame to
// provide the binding, using this stub.
class StubH5vccRuntimeImpl : public h5vcc_runtime::mojom::H5vccRuntime {
 public:
  StubH5vccRuntimeImpl();
  StubH5vccRuntimeImpl(const StubH5vccRuntimeImpl&) = delete;
  StubH5vccRuntimeImpl& operator=(const StubH5vccRuntimeImpl&) = delete;
  ~StubH5vccRuntimeImpl() override;

  void Bind(mojo::PendingReceiver<h5vcc_runtime::mojom::H5vccRuntime> receiver);
  void Reset();

  // h5vcc_runtime::mojom::H5vccRuntime impl.
  void GetAndClearInitialDeepLinkSync(
      GetAndClearInitialDeepLinkSyncCallback callback) override;
  void GetAndClearInitialDeepLink(GetAndClearInitialDeepLinkCallback callback)
      override;
  void AddListener(
      mojo::PendingRemote<h5vcc_runtime::mojom::DeepLinkListener> listener)
      override;

 private:
  mojo::ReceiverSet<h5vcc_runtime::mojom::H5vccRuntime> receivers_;
};

}  // namespace content

#endif  // CONTENT_TEST_COBALT_STUB_H5VCC_RUNTIME_IMPL_H_
