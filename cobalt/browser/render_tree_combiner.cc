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
#include <utility>
#include <vector>

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
    const base::optional<renderer::Submission>& render_tree_submission) {
  render_tree_ = render_tree_submission;
  receipt_time_ = base::TimeTicks::HighResNow();
}

base::optional<renderer::Submission>
RenderTreeCombiner::Layer::GetCurrentSubmission() {
  if (!render_tree_) {
    return base::nullopt;
  }

  base::optional<base::TimeDelta> current_time_offset = CurrentTimeOffset();
  DCHECK(current_time_offset);
  renderer::Submission submission(render_tree_->render_tree,
                                  *current_time_offset);
  submission.timeline_info = render_tree_->timeline_info;
  submission.on_rasterized_callbacks.assign(
      render_tree_->on_rasterized_callbacks.begin(),
      render_tree_->on_rasterized_callbacks.end());

  return submission;
}

base::optional<base::TimeDelta> RenderTreeCombiner::Layer::CurrentTimeOffset() {
  if (!receipt_time_) {
    return base::nullopt;
  } else {
    return render_tree_->time_offset +
           (base::TimeTicks::HighResNow() - *receipt_time_);
  }
}

RenderTreeCombiner::RenderTreeCombiner() : timeline_layer_(NULL) {}

scoped_ptr<RenderTreeCombiner::Layer> RenderTreeCombiner::CreateLayer(
    int z_index) {
  if (layers_.count(z_index) > 0) {
    return scoped_ptr<RenderTreeCombiner::Layer>(NULL);
  }
  RenderTreeCombiner::Layer* layer = new Layer(this);
  layers_[z_index] = layer;

  return scoped_ptr<RenderTreeCombiner::Layer>(layers_[z_index]);
}

void RenderTreeCombiner::SetTimelineLayer(Layer* layer) {
  if (layer != NULL) {
    DCHECK(OwnsLayer(layer));
  }

  timeline_layer_ = layer;
}

void RenderTreeCombiner::RemoveLayer(const Layer* layer) {
  if (timeline_layer_ == layer) {
    SetTimelineLayer(NULL);
  }

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
  Layer* first_layer_with_render_tree = NULL;
  std::vector<base::Closure> on_rasterized_callbacks;
  for (auto it = layers_.begin(); it != layers_.end(); ++it) {
    RenderTreeCombiner::Layer* layer = it->second;
    if (layer->render_tree_) {
      builder.AddChild(layer->render_tree_->render_tree);
      first_layer_with_render_tree = layer;
      on_rasterized_callbacks.insert(
          on_rasterized_callbacks.end(),
          layer->render_tree_->on_rasterized_callbacks.begin(),
          layer->render_tree_->on_rasterized_callbacks.end());
    }
  }
  if (!first_layer_with_render_tree) {
    return base::nullopt;
  }

  Layer* timeline_layer = (timeline_layer_ && timeline_layer_->render_tree_)
                              ? timeline_layer_
                              : first_layer_with_render_tree;

  renderer::Submission submission(new render_tree::CompositionNode(builder),
                                  *timeline_layer->CurrentTimeOffset());
  submission.timeline_info = timeline_layer->render_tree_->timeline_info;
  submission.on_rasterized_callbacks = std::move(on_rasterized_callbacks);

  return submission;
}

bool RenderTreeCombiner::OwnsLayer(Layer* layer) {
  for (const auto& iter_layer : layers_) {
    if (iter_layer.second == layer) {
      return true;
    }
  }
  return false;
}

}  // namespace browser
}  // namespace cobalt
