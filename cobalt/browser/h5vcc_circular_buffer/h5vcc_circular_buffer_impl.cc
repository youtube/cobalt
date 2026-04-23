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

#include "cobalt/browser/h5vcc_circular_buffer/h5vcc_circular_buffer_impl.h"

#include <vector>

#include "base/logging.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"

namespace h5vcc_circular_buffer {

namespace {
const int64_t kBufferSize = 1024 * 1024;  // 1MB
}  // namespace

std::map<std::string, std::shared_ptr<cobalt::storage::CircularBuffer>>
    H5vccCircularBufferImpl::buffers_;

H5vccCircularBufferImpl::H5vccCircularBufferImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccCircularBuffer> receiver)
    : content::DocumentService<mojom::H5vccCircularBuffer>(
          render_frame_host,
          std::move(receiver)) {
  DETACH_FROM_THREAD(thread_checker_);
}

H5vccCircularBufferImpl::~H5vccCircularBufferImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void H5vccCircularBufferImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccCircularBuffer> receiver) {
  new H5vccCircularBufferImpl(*render_frame_host, std::move(receiver));
}

void H5vccCircularBufferImpl::Init(const std::string& identifier,
                                   InitCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = buffers_.find(identifier);
  if (it != buffers_.end()) {
    circular_buffer_ = it->second;
    std::move(callback).Run(true);
    return;
  }

  std::vector<char> cache_dir(kSbFileMaxPath + 1, 0);
  if (SbSystemGetPath(kSbSystemPathCacheDirectory, cache_dir.data(),
                      kSbFileMaxPath)) {
    std::string path = std::string(cache_dir.data()) + "/circular_buffer_" +
                       identifier + ".dat";
    auto buffer =
        std::make_shared<cobalt::storage::CircularBuffer>(path, kBufferSize);
    if (buffer->Init()) {
      buffers_[identifier] = buffer;
      circular_buffer_ = buffer;
      std::move(callback).Run(true);
      return;
    } else {
      LOG(ERROR) << "Failed to initialize CircularBuffer at " << path;
    }
  } else {
    LOG(ERROR) << "Failed to get cache directory path.";
  }
  std::move(callback).Run(false);
}

void H5vccCircularBufferImpl::Append(const std::string& data,
                                     AppendCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (circular_buffer_) {
    circular_buffer_->Append(data);
  } else {
    LOG(ERROR) << "CircularBuffer not available.";
  }
  std::move(callback).Run();
}

void H5vccCircularBufferImpl::Peek(PeekCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::string data;
  bool success = false;
  if (circular_buffer_) {
    success = circular_buffer_->Peek(&data);
  }
  std::move(callback).Run(data, success);
}

void H5vccCircularBufferImpl::Pop(PopCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool success = false;
  if (circular_buffer_) {
    success = circular_buffer_->Pop();
  }
  std::move(callback).Run(success);
}

}  // namespace h5vcc_circular_buffer
