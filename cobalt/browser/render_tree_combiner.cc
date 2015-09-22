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

#include "cobalt/browser/render_tree_combiner.h"

#include "cobalt/render_tree/composition_node.h"

namespace cobalt {
namespace browser {

RenderTreeCombiner::RenderTreeCombiner(renderer::Pipeline* renderer_pipeline)
    : debug_console_mode_(DebugConsole::kDebugConsoleOff),
      renderer_pipeline_(renderer_pipeline) {}

RenderTreeCombiner::~RenderTreeCombiner() {}

void RenderTreeCombiner::SetDebugConsoleMode(
    DebugConsole::DebugConsoleMode debug_console_mode) {
  debug_console_mode_ = debug_console_mode;
}

void RenderTreeCombiner::UpdateMainRenderTree(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<render_tree::animations::NodeAnimationsMap>&
        node_animations_map) {
  main_render_tree_ = render_tree;
  main_animations_map_ = node_animations_map;
  SubmitToRenderer();
}

void RenderTreeCombiner::UpdateDebugConsoleRenderTree(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<render_tree::animations::NodeAnimationsMap>&
        node_animations_map) {
  debug_console_render_tree_ = render_tree;
  debug_console_animations_map_ = node_animations_map;
  SubmitToRenderer();
}

void RenderTreeCombiner::SubmitToRenderer() {
  switch (debug_console_mode_) {
    case DebugConsole::kDebugConsoleOn:
      if (main_render_tree_.get() && debug_console_render_tree_.get()) {
        render_tree::CompositionNode::Builder builder;
        builder.AddChild(main_render_tree_, math::Matrix3F::Identity());
        builder.AddChild(debug_console_render_tree_,
                         math::Matrix3F::Identity());
        scoped_refptr<render_tree::Node> combined_tree =
            new render_tree::CompositionNode(builder);
        renderer_pipeline_->Submit(combined_tree, main_animations_map_,
                                   base::Time::Now() - base::Time::UnixEpoch());
      }
      break;
    case DebugConsole::kDebugConsoleOnly:
      if (debug_console_render_tree_.get()) {
        renderer_pipeline_->Submit(debug_console_render_tree_,
                                   debug_console_animations_map_,
                                   base::Time::Now() - base::Time::UnixEpoch());
      }
      break;
    case DebugConsole::kDebugConsoleHide:
    case DebugConsole::kDebugConsoleOff:
    default:
      if (main_render_tree_.get()) {
        renderer_pipeline_->Submit(main_render_tree_, main_animations_map_,
                                   base::Time::Now() - base::Time::UnixEpoch());
      }
  }
}

}  // namespace browser
}  // namespace cobalt
