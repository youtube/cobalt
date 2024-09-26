// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_LOST_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_LOST_INFO_H_

#include <dawn/webgpu.h>

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class GPUDeviceLostInfo : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPUDeviceLostInfo(const WGPUDeviceLostReason reason,
                             const String& message);

  GPUDeviceLostInfo(const GPUDeviceLostInfo&) = delete;
  GPUDeviceLostInfo& operator=(const GPUDeviceLostInfo&) = delete;

  // gpu_device_lost_info.idl
  const ScriptValue reason(ScriptState* script_state) const;
  const String& message() const;

 private:
  String reason_;
  String message_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_LOST_INFO_H_
