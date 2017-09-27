// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/render_tree_combiner.h"

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "base/time.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/renderer/submission.h"

namespace cobalt {
namespace browser {

RenderTreeCombiner::Layer::Layer(RenderTreeCombiner* render_tree_combiner)
    : render_tree_combiner_(render_tree_combiner),
      render_tree_(base::nullopt),
      receipt_time_(base::nullopt) {}

RenderTreeCombiner::Layer::~Layer() {
  DCHECK(render_tree_combiner_);
  render_tree_combiner_->RemoveLayer(this);
}

void RenderTreeCombiner::Layer::Submit(
    const base::optional<renderer::Submission>& render_tree_submission,
    bool receive_time) {
  render_tree_ = render_tree_submission;
  if (receive_time) {
    receipt_time_ = base::TimeTicks::HighResNow();
  } else {
    receipt_time_ = base::nullopt;
  }
}

RenderTreeCombiner::RenderTreeCombiner() {}

scoped_ptr<RenderTreeCombiner::Layer> RenderTreeCombiner::CreateLayer(
    int z_index) {
  if (layers_.count(z_index) > 0) {
    return scoped_ptr<RenderTreeCombiner::Layer>(NULL);
  }
  RenderTreeCombiner::Layer* layer = new Layer(this);
  layers_[z_index] = layer;

  return scoped_ptr<RenderTreeCombiner::Layer>(layers_[z_index]);
}

void RenderTreeCombiner::RemoveLayer(const Layer* layer) {
  for (auto it = layers_.begin(); it != layers_.end(); /* no increment */) {
    if (it->second == layer) {
      it = layers_.erase(it);
    } else {
      ++it;
    }
  }
}

base::optional<renderer::Submission>
RenderTreeCombiner::GetCurrentSubmission() {
  render_tree::CompositionNode::Builder builder;

  // Add children for all layers in order.
  base::optional<renderer::Submission> first_tree = base::nullopt;
  base::optional<renderer::Submission> combined_submission = base::nullopt;
  for (auto it = layers_.begin(); it != layers_.end(); ++it) {
    RenderTreeCombiner::Layer* layer = it->second;
    if (layer->render_tree_) {
      builder.AddChild(layer->render_tree_->render_tree);
      first_tree = layer->render_tree_;
      // Make the combined submission with the first receipt_time_ we find.
      if (!combined_submission && layer->receipt_time_) {
        combined_submission = renderer::Submission(*layer->render_tree_);
        combined_submission->time_offset =
            layer->render_tree_->time_offset +
            (base::TimeTicks::HighResNow() - *layer->receipt_time_);
      }
    }
  }
  if (!first_tree) {
    return base::nullopt;
  }
  if (!combined_submission) {
    // None of the layers store the time.
    combined_submission = renderer::Submission(*first_tree);
  }

  combined_submission->render_tree = new render_tree::CompositionNode(builder);
  return *combined_submission;
}
}  // namespace browser
}  // namespace cobalt
