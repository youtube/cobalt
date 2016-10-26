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

#ifndef COBALT_BROWSER_RENDER_TREE_COMBINER_H_
#define COBALT_BROWSER_RENDER_TREE_COMBINER_H_

#include "cobalt/renderer/renderer_module.h"
#include "cobalt/renderer/submission.h"

namespace cobalt {
namespace browser {

// Combines the main and debug console render trees. Caches the individual
// trees as they are produced. Re-renders when either tree changes.
// This class is only fully implemented when ENABLE_DEBUG_CONSOLE is defined,
// otherwise (e.g. in release builds) a stub implementation is used.
class RenderTreeCombiner {
 public:
  explicit RenderTreeCombiner(renderer::RendererModule* renderer_module,
                              const math::Size& viewport_size);
  ~RenderTreeCombiner();

  void Reset();

  // Update the main web module render tree.
  void UpdateMainRenderTree(const renderer::Submission& render_tree_submission);

  // Update the debug console render tree.
  void UpdateDebugConsoleRenderTree(
      const base::optional<renderer::Submission>& render_tree_submission);

#if defined(ENABLE_DEBUG_CONSOLE)
  bool render_debug_console() const { return render_debug_console_; }
  void set_render_debug_console(bool render_debug_console) {
    render_debug_console_ = render_debug_console;
  }
#else   // ENABLE_DEBUG_CONSOLE
  bool render_debug_console() const { return false; }
  void set_render_debug_console(bool render_debug_console) {
    UNREFERENCED_PARAMETER(render_debug_console);
  }
#endif  // ENABLE_DEBUG_CONSOLE

 private:
#if defined(ENABLE_DEBUG_CONSOLE)
  // Combines the two cached render trees (main/debug) and renders the result.
  void SubmitToRenderer();

  bool render_debug_console_;

  // Local reference to the render pipeline, so we can submit the combined tree.
  // Reference counted pointer not necessary here.
  renderer::RendererModule* renderer_module_;

  // The size of the output viewport.
  math::Size viewport_size_;

  // Local references to the main and debug console render trees/animation maps
  // so we can combine them.
  base::optional<renderer::Submission> main_render_tree_;

  // This is the time that we received the last main render tree submission.
  // used so that we know what time to forward the submission to the pipeline
  // with.
  base::optional<base::TimeTicks> main_render_tree_receipt_time_;

  // The debug console render tree submission.
  base::optional<renderer::Submission> debug_console_render_tree_;
#else   // ENABLE_DEBUG_CONSOLE
  // Use this local reference even in release builds to submit the main tree.
  renderer::RendererModule* renderer_module_;
#endif  // ENABLE_DEBUG_CONSOLE
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_RENDER_TREE_COMBINER_H_
