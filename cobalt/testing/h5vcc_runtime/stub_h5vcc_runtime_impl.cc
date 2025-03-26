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

#include "cobalt/testing/h5vcc_runtime/stub_h5vcc_runtime_impl.h"

#include <utility>

namespace content {

StubH5vccRuntimeImpl::StubH5vccRuntimeImpl() = default;

StubH5vccRuntimeImpl::~StubH5vccRuntimeImpl() = default;

void StubH5vccRuntimeImpl::Bind(
    mojo::PendingReceiver<h5vcc_runtime::mojom::H5vccRuntime> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void StubH5vccRuntimeImpl::GetAndClearInitialDeepLinkSync(
    GetAndClearInitialDeepLinkSyncCallback callback) {
  // Prevents a dropped callback error.
  std::move(callback).Run("");
}

void StubH5vccRuntimeImpl::GetAndClearInitialDeepLink(
    GetAndClearInitialDeepLinkCallback callback) {
  // Prevents a dropped callback error.
  std::move(callback).Run("");
}

void StubH5vccRuntimeImpl::AddListener(
    mojo::PendingRemote<h5vcc_runtime::mojom::DeepLinkListener> listener) {}

}  // namespace content
