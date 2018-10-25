// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/debug/backend/render_layer.h"

#include "cobalt/render_tree/composition_node.h"

namespace cobalt {
namespace debug {
namespace backend {

RenderLayer::RenderLayer(const OnChangedCallback& on_changed_callback)
    : on_changed_callback_(on_changed_callback),
      back_layer_(NULL),
      front_layer_(NULL) {}

void RenderLayer::SetBackLayer(const scoped_refptr<render_tree::Node>& layer) {
  back_layer_ = layer;
  Combine();
}

void RenderLayer::SetFrontLayer(const scoped_refptr<render_tree::Node>& layer) {
  front_layer_ = layer;
  Combine();
}

void RenderLayer::Combine() {
  render_tree::CompositionNode::Builder builder;
  if (back_layer_) {
    builder.AddChild(back_layer_);
  }
  if (front_layer_) {
    builder.AddChild(front_layer_);
  }
  scoped_refptr<render_tree::Node> output =
      new render_tree::CompositionNode(builder);
  on_changed_callback_.Run(output);
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
