/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef BROWSER_RENDER_TREE_COMBINER_H_
#define BROWSER_RENDER_TREE_COMBINER_H_

#include "cobalt/browser/debug_console.h"
#include "cobalt/renderer/pipeline.h"

namespace cobalt {
namespace browser {

// Combines the main and debug console render trees
// Caches the individual trees as they are produced.
// Re-renders when either tree changes.
class RenderTreeCombiner {
 public:
  explicit RenderTreeCombiner(renderer::Pipeline* renderer_pipeline);
  ~RenderTreeCombiner();

  void SetDebugConsoleMode(DebugConsole::DebugConsoleMode debug_console_mode);

  // Update the main web module render tree.
  void UpdateMainRenderTree(
      const scoped_refptr<render_tree::Node>& render_tree,
      const scoped_refptr<render_tree::animations::NodeAnimationsMap>&
          animations_map);

  // Update the debug console render tree.
  void UpdateDebugConsoleRenderTree(
      const scoped_refptr<render_tree::Node>& render_tree,
      const scoped_refptr<render_tree::animations::NodeAnimationsMap>&
          animations_map);

 private:
  // Combines the two cached render trees (main/debug) and renders the result.
  void SubmitToRenderer();

  // Local references to the main and debug console render trees/animation maps
  // so we can combine them.
  scoped_refptr<render_tree::Node> main_render_tree_;
  scoped_refptr<render_tree::animations::NodeAnimationsMap>
      main_animations_map_;
  scoped_refptr<render_tree::Node> debug_console_render_tree_;
  scoped_refptr<render_tree::animations::NodeAnimationsMap>
      debug_console_animations_map_;

  DebugConsole::DebugConsoleMode debug_console_mode_;

  // Local reference to the render pipeline, so we can submit the combined tree.
  // Reference counted pointer not necessary here.
  renderer::Pipeline* renderer_pipeline_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // BROWSER_RENDER_TREE_COMBINER_H_
