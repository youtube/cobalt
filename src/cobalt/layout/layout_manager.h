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

#ifndef COBALT_LAYOUT_LAYOUT_MANAGER_H_
#define COBALT_LAYOUT_LAYOUT_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/window.h"
#include "cobalt/layout/layout_stat_tracker.h"
#include "cobalt/loader/image/image_cache.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace layout {

// Produces the render tree each time when the document needs layout update.
class LayoutManager {
 public:
  struct LayoutResults {
    LayoutResults(const scoped_refptr<render_tree::Node>& render_tree,
                  const base::TimeDelta& layout_time)
        : render_tree(render_tree),
          layout_time(layout_time) {}

    // The render tree produced by a layout.
    scoped_refptr<render_tree::Node> render_tree;

    // The time that the render tree was created, which will be used as a
    // reference point for updating the animations in the above render tree.
    base::TimeDelta layout_time;
  };

  typedef base::Callback<void(const LayoutResults&)>
      OnRenderTreeProducedCallback;

  // Specifies what event should trigger a layout, and hence what event
  // will result in a render tree being produced and passed in a call to
  // on_render_tree_produced_callback_.
  enum LayoutTrigger {
    kOnDocumentMutation,
#if defined(ENABLE_TEST_RUNNER)
    kTestRunnerMode,
#endif  // ENABLE_TEST_RUNNER
  };

  LayoutManager(const scoped_refptr<dom::Window>& window,
                const OnRenderTreeProducedCallback& on_render_tree_produced,
                LayoutTrigger layout_trigger, const int dom_max_element_depth,
                const float layout_refresh_rate, const std::string& language,
                LayoutStatTracker* layout_stat_tracker);
  ~LayoutManager();

  void Suspend();
  void Resume();

 private:
  class Impl;
  const scoped_ptr<Impl> impl_;

  DISALLOW_COPY_AND_ASSIGN(LayoutManager);
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_LAYOUT_MANAGER_H_
