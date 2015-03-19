/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "base/at_exit.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/init_cobalt.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/renderer/test/scenes/all_scenes_combined_scene.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"

using cobalt::render_tree::ResourceProvider;
using cobalt::renderer::test::scenes::AddBlankBackgroundToScene;
using cobalt::renderer::test::scenes::CreateAllScenesCombinedScene;
using cobalt::renderer::test::scenes::RenderTreeWithAnimations;

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  cobalt::InitCobalt(argc, argv);

  cobalt::trace_event::ScopedTraceToFile trace_to_file(
      FilePath(FILE_PATH_LITERAL("renderer_sandbox_trace.json")));

  // Construct a renderer module using default options.
  cobalt::renderer::RendererModule::Options renderer_module_options;
  cobalt::renderer::RendererModule renderer_module(renderer_module_options);

  cobalt::math::SizeF output_dimensions(
      renderer_module.render_target()->GetSurfaceInfo().width,
      renderer_module.render_target()->GetSurfaceInfo().height);

  // Construct our render tree and associated animations to be passed into
  // the renderer pipeline for display.
  RenderTreeWithAnimations scene =
      AddBlankBackgroundToScene(
          CreateAllScenesCombinedScene(
              renderer_module.pipeline()->GetResourceProvider(),
              output_dimensions,
              base::Time::Now()),
          output_dimensions);

  // Pass the render tree along with associated animations into the renderer
  // module to be displayed.
  renderer_module.pipeline()->Submit(scene.render_tree, scene.animations);

  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(30));

  return 0;
}
