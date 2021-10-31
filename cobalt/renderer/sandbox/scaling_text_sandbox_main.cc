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

#include <memory>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/math/size.h"
#include "cobalt/renderer/pipeline.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/renderer/submission.h"
#include "cobalt/renderer/test/scenes/scaling_text_scene.h"
#include "cobalt/system_window/system_window.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"

using cobalt::render_tree::ResourceProvider;
using cobalt::renderer::test::scenes::AddBlankBackgroundToScene;
using cobalt::renderer::test::scenes::CreateScalingTextScene;
using cobalt::system_window::SystemWindow;

namespace {

const int kViewportWidth = 1920;
const int kViewportHeight = 1080;

int SandboxMain(int argc, char** argv) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);

  cobalt::trace_event::ScopedTraceToFile trace_to_file(
      base::FilePath(FILE_PATH_LITERAL("scaling_text_sandbox_trace.json")));

  base::EventDispatcher event_dispatcher;
  // Create a system window to use as a render target.
  cobalt::math::Size view_size(kViewportWidth, kViewportHeight);
  std::unique_ptr<SystemWindow> system_window(
      new cobalt::system_window::SystemWindow(&event_dispatcher, view_size));

  // Construct a renderer module using default options.
  cobalt::renderer::RendererModule::Options renderer_module_options;
  cobalt::renderer::RendererModule renderer_module(system_window.get(),
                                                   renderer_module_options);

  cobalt::math::SizeF output_dimensions(
      renderer_module.render_target()->GetSize());

  // Construct our render tree and associated animations to be passed into
  // the renderer pipeline for display.
  base::TimeDelta start_time = base::Time::Now() - base::Time::UnixEpoch();
  scoped_refptr<cobalt::render_tree::Node> scene = AddBlankBackgroundToScene(
      CreateScalingTextScene(renderer_module.pipeline()->GetResourceProvider(),
                             output_dimensions, start_time),
      output_dimensions);

  // Pass the render tree along with associated animations into the renderer
  // module to be displayed.
  renderer_module.pipeline()->Submit(
      cobalt::renderer::Submission(scene, start_time));

  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(30));

  return 0;
}

}  // namespace

COBALT_WRAP_SIMPLE_MAIN(SandboxMain);
