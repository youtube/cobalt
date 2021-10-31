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
#include "cobalt/renderer/test/scenes/all_scenes_combined_scene.h"
#include "cobalt/system_window/system_window.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"

using cobalt::render_tree::ResourceProvider;
using cobalt::renderer::test::scenes::AddBlankBackgroundToScene;
using cobalt::renderer::test::scenes::CreateAllScenesCombinedScene;
using cobalt::system_window::SystemWindow;

namespace {

const int kViewportWidth = 1920;
const int kViewportHeight = 1080;

class RendererSandbox {
 public:
  RendererSandbox();

 private:
  cobalt::trace_event::ScopedTraceToFile trace_to_file_;
  base::EventDispatcher event_dispatcher_;
  std::unique_ptr<SystemWindow> system_window_;
  std::unique_ptr<cobalt::renderer::RendererModule> renderer_module_;
};

RendererSandbox::RendererSandbox()
    : trace_to_file_(
          base::FilePath(FILE_PATH_LITERAL("renderer_sandbox_trace.json"))) {
  cobalt::math::Size view_size(kViewportWidth, kViewportHeight);
  // Create a system window to use as a render target.
  system_window_.reset(
      new cobalt::system_window::SystemWindow(&event_dispatcher_, view_size));

  // Construct a renderer module using default options.
  cobalt::renderer::RendererModule::Options renderer_module_options;
  renderer_module_.reset(new cobalt::renderer::RendererModule(
      system_window_.get(), renderer_module_options));

  cobalt::math::SizeF output_dimensions(
      renderer_module_->render_target()->GetSize());

  // Construct our render tree and associated animations to be passed into
  // the renderer pipeline for display.
  base::TimeDelta start_time = base::Time::Now() - base::Time::UnixEpoch();
  scoped_refptr<cobalt::render_tree::Node> scene = AddBlankBackgroundToScene(
      CreateAllScenesCombinedScene(
          renderer_module_->pipeline()->GetResourceProvider(),
          output_dimensions, start_time),
      output_dimensions);

  // Pass the render tree along with associated animations into the renderer
  // module to be displayed.
  renderer_module_->pipeline()->Submit(
      cobalt::renderer::Submission(scene, start_time));
}

RendererSandbox* g_renderer_sandbox = NULL;

void StartApplication(int argc, char** argv, const char* link,
                      const base::Closure& quit_closure,
                      SbTimeMonotonic timestamp) {
  DCHECK(!g_renderer_sandbox);
  g_renderer_sandbox = new RendererSandbox();
  DCHECK(g_renderer_sandbox);

  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE, quit_closure, base::TimeDelta::FromSeconds(30));
}

void StopApplication() {
  DCHECK(g_renderer_sandbox);
  delete g_renderer_sandbox;
  g_renderer_sandbox = NULL;
}

}  // namespace

COBALT_WRAP_BASE_MAIN(StartApplication, StopApplication);
