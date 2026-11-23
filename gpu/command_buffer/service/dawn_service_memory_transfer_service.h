// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_SERVICE_MEMORY_TRANSFER_SERVICE_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_SERVICE_MEMORY_TRANSFER_SERVICE_H_

#include <dawn/wire/WireServer.h>

#include "base/memory/raw_ptr.h"

namespace gpu {

class CommonDecoder;

namespace webgpu {

class DawnServiceMemoryTransferService final
    : public dawn::wire::server::MemoryTransferService {
 public:
  DawnServiceMemoryTransferService(CommonDecoder* decoder);
  ~DawnServiceMemoryTransferService() override;

  // Creates a MemoryHandle from a serialized MemoryTransferHandle made of the
  // id, offset and size of a shared memory region. Returns nullptr when the
  // region is invalid, causing a context loss.
  std::unique_ptr<MemoryHandle> DeserializeMemoryHandle(
      std::span<const std::byte> creation_data) override;

 private:
  raw_ptr<CommonDecoder> decoder_;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_SERVICE_MEMORY_TRANSFER_SERVICE_H_
