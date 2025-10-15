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

#ifndef COBALT_BROWSER_H5VCC_STORAGE_H5VCC_STORAGE_IMPL_H_
#define COBALT_BROWSER_H5VCC_STORAGE_H5VCC_STORAGE_IMPL_H_

#include "base/threading/thread_checker.h"
#include "cobalt/browser/h5vcc_storage/public/mojom/h5vcc_storage.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_storage {

// Implements the H5vccStorage Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class H5vccStorageImpl : public content::DocumentService<mojom::H5vccStorage> {
 public:
  // Creates a H5vccStorageImpl. The H5vccStorageImpl is bound to the
  // receiver and its lifetime is scoped to the render_frame_host.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::H5vccStorage> receiver);

  H5vccStorageImpl(const H5vccStorageImpl&) = delete;
  H5vccStorageImpl& operator=(const H5vccStorageImpl&) = delete;

  void Flush(FlushCallback) override;

 private:
  H5vccStorageImpl(content::RenderFrameHost& render_frame_host,
                   mojo::PendingReceiver<mojom::H5vccStorage> receiver);
  ~H5vccStorageImpl();

  THREAD_CHECKER(thread_checker_);
};

}  // namespace h5vcc_storage

#endif  // COBALT_BROWSER_H5VCC_STORAGE_H5VCC_STORAGE_IMPL_H_
