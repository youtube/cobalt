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

#ifndef COBALT_BROWSER_H5VCC_STORAGE_TESTING_H5VCC_STORAGE_FOR_TESTING_IMPL_H_
#define COBALT_BROWSER_H5VCC_STORAGE_TESTING_H5VCC_STORAGE_FOR_TESTING_IMPL_H_

#include <string>

#include "base/files/file_path.h"
#include "cobalt/browser/h5vcc_storage/testing/public/mojom/h5vcc_storage_for_testing.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_storage_for_testing {

// Implements the H5vccStorage Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class H5vccStorageForTestingImpl
    : public content::DocumentService<mojom::H5vccStorageForTesting> {
 public:
  // Creates a H5vccStorageForTestingImpl. The H5vccStorageForTestingImpl is
  // bound to the receiver and its lifetime is scoped to the render_frame_host.
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::H5vccStorageForTesting> receiver);

  H5vccStorageForTestingImpl(const H5vccStorageForTestingImpl&) = delete;
  H5vccStorageForTestingImpl& operator=(const H5vccStorageForTestingImpl&) =
      delete;

  void WriteTest(uint32_t test_size,
                 const std::string& test_string,
                 WriteTestCallback) override;
  void VerifyTest(uint32_t test_size,
                  const std::string& test_string,
                  VerifyTestCallback) override;

 private:
  H5vccStorageForTestingImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::H5vccStorageForTesting> receiver);
  base::FilePath user_data_path_;
};

}  // namespace h5vcc_storage_for_testing

#endif  // COBALT_BROWSER_H5VCC_STORAGE_TESTING_H5VCC_STORAGE_FOR_TESTING_IMPL_H_
