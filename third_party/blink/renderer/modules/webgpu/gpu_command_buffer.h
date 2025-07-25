// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_BUFFER_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUCommandBuffer : public DawnObject<WGPUCommandBuffer> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPUCommandBuffer(GPUDevice* device,
                            WGPUCommandBuffer command_buffer);

  GPUCommandBuffer(const GPUCommandBuffer&) = delete;
  GPUCommandBuffer& operator=(const GPUCommandBuffer&) = delete;

 private:
  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetProcs().commandBufferSetLabel(GetHandle(), utf8_label.c_str());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_BUFFER_H_
