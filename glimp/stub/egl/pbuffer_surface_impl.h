/*
 * Copyright 2023 The Cobalt Authors. All Rights Reserved.
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GLIMP_STUB_EGL_PBUFFER_SURFACE_IMPL_H_
#define GLIMP_STUB_EGL_PBUFFER_SURFACE_IMPL_H_

#include <KHR/khrplatform.h>

#include "glimp/ref_counted.h"
#include "glimp/stub/egl/surface_impl.h"

namespace glimp {
namespace egl {

class PbufferSurfaceImplStub : public SurfaceImplStub {
 public:
  PbufferSurfaceImplStub(int width, int height);
  virtual ~PbufferSurfaceImplStub();

  int GetWidth() const override { return width_; }
  int GetHeight() const override { return height_; }

 private:
  int width_;
  int height_;
};

}  // namespace egl
}  // namespace glimp

#endif  // GLIMP_STUB_EGL_PBUFFER_SURFACE_IMPL_H_
