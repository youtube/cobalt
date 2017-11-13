// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_BACKEND_RENDER_TARGET_STUB_H_
#define COBALT_RENDERER_BACKEND_RENDER_TARGET_STUB_H_

#include "cobalt/renderer/backend/render_target.h"

namespace cobalt {
namespace renderer {
namespace backend {

// A render target for the stub graphics system.
// The render target has no functionality and returns fake but valid metadata
// if queried.
class RenderTargetStub : public RenderTarget {
 public:
  explicit RenderTargetStub(const math::Size& size) : size_(size) {}

  const math::Size& GetSize() const OVERRIDE { return size_; }

  intptr_t GetPlatformHandle() const OVERRIDE { return 0; }

  bool CreationError() OVERRIDE { return false; }

 private:
  ~RenderTargetStub() {}

  math::Size size_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_RENDER_TARGET_STUB_H_
