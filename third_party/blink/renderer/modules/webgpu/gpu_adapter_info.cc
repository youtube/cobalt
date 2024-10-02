// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_adapter_info.h"

namespace blink {

GPUAdapterInfo::GPUAdapterInfo(const String& vendor,
                               const String& architecture,
                               const String& device,
                               const String& description,
                               const String& driver)
    : vendor_(vendor),
      architecture_(architecture),
      device_(device),
      description_(description),
      driver_(driver) {}

const String& GPUAdapterInfo::vendor() const {
  return vendor_;
}

const String& GPUAdapterInfo::architecture() const {
  return architecture_;
}

const String& GPUAdapterInfo::device() const {
  return device_;
}

const String& GPUAdapterInfo::description() const {
  return description_;
}

const String& GPUAdapterInfo::driver() const {
  return driver_;
}

}  // namespace blink
