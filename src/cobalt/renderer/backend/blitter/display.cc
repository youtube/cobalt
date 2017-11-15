// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/backend/blitter/display.h"

#include "base/debug/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/backend/blitter/render_target.h"
#include "cobalt/system_window/system_window.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace backend {

namespace {

class DisplayRenderTargetBlitter : public RenderTargetBlitter {
 public:
  // The DisplayRenderTargetBlitter is constructed with a handle to a native
  // window to use as the render target.
  DisplayRenderTargetBlitter(SbBlitterDevice device,
                             system_window::SystemWindow* system_window);

  const math::Size& GetSize() const OVERRIDE;

  SbBlitterRenderTarget GetSbRenderTarget() const OVERRIDE;

  void Flip() OVERRIDE;

  bool CreationError() OVERRIDE { return false; }

 private:
  ~DisplayRenderTargetBlitter() OVERRIDE;

  SbBlitterSwapChain swap_chain_;
  SbBlitterRenderTarget render_target_;

  math::Size size_;
};

DisplayRenderTargetBlitter::DisplayRenderTargetBlitter(
    SbBlitterDevice device, system_window::SystemWindow* system_window) {
  SbWindow starboard_window = system_window->GetSbWindow();

  swap_chain_ = SbBlitterCreateSwapChainFromWindow(device, starboard_window);
  CHECK(SbBlitterIsSwapChainValid(swap_chain_));

  render_target_ = SbBlitterGetRenderTargetFromSwapChain(swap_chain_);
  CHECK(SbBlitterIsRenderTargetValid(render_target_));

  SbWindowSize window_size;
  SbWindowGetSize(starboard_window, &window_size);
  size_.SetSize(window_size.width, window_size.height);
}

const math::Size& DisplayRenderTargetBlitter::GetSize() const { return size_; }

DisplayRenderTargetBlitter::~DisplayRenderTargetBlitter() {
  SbBlitterDestroySwapChain(swap_chain_);
}

SbBlitterRenderTarget DisplayRenderTargetBlitter::GetSbRenderTarget() const {
  return render_target_;
}

void DisplayRenderTargetBlitter::Flip() {
  TRACE_EVENT0("cobalt::renderer", "DisplayRenderTargetBlitter::Flip()");
  SbBlitterFlipSwapChain(swap_chain_);
}

}  // namespace

DisplayBlitter::DisplayBlitter(SbBlitterDevice device,
                               system_window::SystemWindow* system_window) {
  // A display effectively just hosts a DisplayRenderTargetBlitter.
  render_target_ = new DisplayRenderTargetBlitter(device, system_window);
}

scoped_refptr<RenderTarget> DisplayBlitter::GetRenderTarget() {
  return render_target_;
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
