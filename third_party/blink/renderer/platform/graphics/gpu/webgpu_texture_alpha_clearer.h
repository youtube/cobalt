// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_TEXTURE_ALPHA_CLEARER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_TEXTURE_ALPHA_CLEARER_H_

#include <dawn/webgpu.h>

#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class PLATFORM_EXPORT WebGPUTextureAlphaClearer final
    : public WTF::RefCounted<WebGPUTextureAlphaClearer> {
 public:
  WebGPUTextureAlphaClearer(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      WGPUDevice device,
      WGPUTextureFormat format);

  bool IsCompatible(WGPUDevice device, WGPUTextureFormat format) const;
  void ClearAlpha(WGPUTexture texture);

 private:
  friend class WTF::RefCounted<WebGPUTextureAlphaClearer>;
  ~WebGPUTextureAlphaClearer();

  const scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  const WGPUDevice device_;
  const WGPUTextureFormat format_;
  WGPURenderPipeline alpha_to_one_pipeline_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_TEXTURE_ALPHA_CLEARER_H_
