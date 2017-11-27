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

#ifndef COBALT_RENDERER_BACKEND_DISPLAY_STUB_H_
#define COBALT_RENDERER_BACKEND_DISPLAY_STUB_H_

#include "cobalt/renderer/backend/display.h"
#include "cobalt/renderer/backend/render_target_stub.h"

namespace cobalt {
namespace renderer {
namespace backend {

// The display object part of the stub graphics framework.  This display does
// not initialize any hardware, and will create and own a stub render target
// that it returns if queried for a render target.
class DisplayStub : public Display {
 public:
  DisplayStub()
      : render_target_(new RenderTargetStub(math::Size(1920, 1080))) {}

  // Return the stub render target.
  scoped_refptr<RenderTarget> GetRenderTarget() override {
    return render_target_;
  }

 private:
  // Own a stub render target that we can return on request to permit
  // pretend rendering to anyone who desires it.
  scoped_refptr<RenderTargetStub> render_target_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_DISPLAY_STUB_H_
