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

#include <cmath>

#include "base/at_exit.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/init_cobalt.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/renderer/test/render_tree_builders/all_node_types_builder.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"

using cobalt::render_tree::ResourceProvider;
using cobalt::renderer::test::render_tree_builders::AllNodeTypesBuilder;

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  cobalt::InitCobalt(argc, argv);

  cobalt::trace_event::ScopedTraceToFile trace_to_file(
      FilePath(FILE_PATH_LITERAL("renderer_sandbox_trace.json")));

  // Construct a renderer module using default options.
  cobalt::renderer::RendererModule::Options renderer_module_options;
  cobalt::renderer::RendererModule renderer_module(renderer_module_options);

  // Construct our render tree builder which will be the source of our render
  // trees within the main loop.
  AllNodeTypesBuilder render_tree_builder(
      renderer_module.pipeline()->GetResourceProvider());

  // Repeatedly render/animate and flip the screen.
  int frame = 0;
  base::Time start_time = base::Time::Now();
  while (true) {
    double seconds_elapsed = (base::Time::Now() - start_time).InSecondsF();

    // Stop after 30 seconds have passed.
    if (seconds_elapsed > 30) {
      break;
    }

    ++frame;
    TRACE_EVENT1("renderer_sandbox", "frame", "frame_number", frame);
    // Build the render tree that we will output to the screen
    scoped_refptr<cobalt::render_tree::Node> render_tree =
        render_tree_builder.Build(
            seconds_elapsed,
            cobalt::math::SizeF(
                renderer_module.render_target()->GetSurfaceInfo().width,
                renderer_module.render_target()->GetSurfaceInfo().height));

    // Submit the render tree to be rendered.
    {
      TRACE_EVENT0("renderer_sandbox", "submit");
      renderer_module.pipeline()->Submit(render_tree);
    }
  }

  return 0;
}
