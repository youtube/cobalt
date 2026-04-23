// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_H5VCC_CIRCULAR_BUFFER_H5VCC_CIRCULAR_BUFFER_IMPL_H_
#define COBALT_BROWSER_H5VCC_CIRCULAR_BUFFER_H5VCC_CIRCULAR_BUFFER_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "cobalt/browser/h5vcc_circular_buffer/public/mojom/h5vcc_circular_buffer.mojom.h"
#include "cobalt/storage/circular_buffer.h"
#include "content/public/browser/document_service.h"

namespace h5vcc_circular_buffer {

class H5vccCircularBufferImpl final
    : public content::DocumentService<mojom::H5vccCircularBuffer> {
 public:
  H5vccCircularBufferImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::H5vccCircularBuffer> receiver);

  ~H5vccCircularBufferImpl() override;

  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::H5vccCircularBuffer> receiver);

  void Init(const std::string& identifier, InitCallback callback) override;
  void Append(const std::string& data, AppendCallback callback) override;
  void Peek(PeekCallback callback) override;
  void Pop(PopCallback callback) override;

 private:
  THREAD_CHECKER(thread_checker_);
  std::shared_ptr<cobalt::storage::CircularBuffer> circular_buffer_;

  static std::map<std::string, std::shared_ptr<cobalt::storage::CircularBuffer>>
      buffers_;
};

}  // namespace h5vcc_circular_buffer

#endif  // COBALT_BROWSER_H5VCC_CIRCULAR_BUFFER_H5VCC_CIRCULAR_BUFFER_IMPL_H_
