// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_BACKEND_DISPLAY_H_
#define COBALT_RENDERER_BACKEND_DISPLAY_H_

#include "base/memory/ref_counted.h"

namespace cobalt {
namespace renderer {
namespace backend {

class RenderTarget;

// A Display object represents a visual output device for a user.  Typically
// there will be only one Display object instantiated in a system and it will
// correspond to a TV or monitor.  Creating a display object should initialize
// the device and destructing it may shutdown display libraries.  A render
// target can be obtained from a display and associated with a graphics context
// to allow output to the display.
class Display {
 public:
  virtual ~Display() {}

  // Obtain a render target associated with the display that can be passed into
  // a GraphicsContext object allowing for rendering to the display.
  virtual scoped_refptr<RenderTarget> GetRenderTarget() = 0;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_DISPLAY_H_
