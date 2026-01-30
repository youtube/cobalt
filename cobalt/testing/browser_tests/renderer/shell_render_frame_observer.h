// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_TESTING_BROWSER_TESTS_RENDERER_SHELL_RENDER_FRAME_OBSERVER_H_
#define COBALT_TESTING_BROWSER_TESTS_RENDERER_SHELL_RENDER_FRAME_OBSERVER_H_

#include "cobalt/renderer/cobalt_render_frame_observer.h"

namespace content {

class ShellRenderFrameObserver : public cobalt::CobaltRenderFrameObserver {
 public:
  explicit ShellRenderFrameObserver(RenderFrame* frame);
  ~ShellRenderFrameObserver() override;

  ShellRenderFrameObserver(const ShellRenderFrameObserver&) = delete;
  ShellRenderFrameObserver& operator=(const ShellRenderFrameObserver&) = delete;

 private:
  // RenderFrameObserver implementation.
  void OnDestruct() override;
  void DidClearWindowObject() override;
  void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_RENDERER_SHELL_RENDER_FRAME_OBSERVER_H_
