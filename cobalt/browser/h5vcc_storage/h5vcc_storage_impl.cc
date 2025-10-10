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

#include "cobalt/browser/h5vcc_storage/h5vcc_storage_impl.h"

// #include "build/build_config.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace h5vcc_storage {

H5vccStorageImpl::H5vccStorageImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccStorage> receiver)
    : content::DocumentService<mojom::H5vccStorage>(render_frame_host,
                                                    std::move(receiver)) {
  DETACH_FROM_THREAD(thread_checker_);
}

H5vccStorageImpl::~H5vccStorageImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void H5vccStorageImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccStorage> receiver) {
  new H5vccStorageImpl(*render_frame_host, std::move(receiver));
}

void H5vccStorageImpl::Flush(FlushCallback flush_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* storage_partition = render_frame_host().GetStoragePartition();
  CHECK(storage_partition);
  // Flushes localStorage.
  storage_partition->Flush();
  auto* cookie_manager = storage_partition->GetCookieManagerForBrowserProcess();
  CHECK(cookie_manager);
  cookie_manager->FlushCookieStore(std::move(flush_callback));
}

}  // namespace h5vcc_storage
