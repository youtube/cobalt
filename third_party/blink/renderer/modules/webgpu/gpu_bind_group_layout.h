// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BIND_GROUP_LAYOUT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BIND_GROUP_LAYOUT_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class ExceptionState;
class GPUBindGroupLayoutDescriptor;

class GPUBindGroupLayout : public DawnObject<WGPUBindGroupLayout> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUBindGroupLayout* Create(
      GPUDevice* device,
      const GPUBindGroupLayoutDescriptor* webgpu_desc,
      ExceptionState& exception_state);
  explicit GPUBindGroupLayout(GPUDevice* device,
                              WGPUBindGroupLayout bind_group_layout);

  GPUBindGroupLayout(const GPUBindGroupLayout&) = delete;
  GPUBindGroupLayout& operator=(const GPUBindGroupLayout&) = delete;

  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetProcs().bindGroupLayoutSetLabel(GetHandle(), utf8_label.c_str());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BIND_GROUP_LAYOUT_H_
