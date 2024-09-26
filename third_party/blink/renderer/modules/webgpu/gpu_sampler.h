// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SAMPLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SAMPLER_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUSamplerDescriptor;

class GPUSampler : public DawnObject<WGPUSampler> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUSampler* Create(GPUDevice* device,
                            const GPUSamplerDescriptor* webgpu_desc);
  explicit GPUSampler(GPUDevice* device, WGPUSampler sampler);

  GPUSampler(const GPUSampler&) = delete;
  GPUSampler& operator=(const GPUSampler&) = delete;

 private:
  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetProcs().samplerSetLabel(GetHandle(), utf8_label.c_str());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SAMPLER_H_
