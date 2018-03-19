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

#ifndef COBALT_BROWSER_RENDER_TREE_COMBINER_H_
#define COBALT_BROWSER_RENDER_TREE_COMBINER_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "base/time.h"
#include "cobalt/renderer/submission.h"

namespace cobalt {
namespace browser {

// Combines rendering layers (such as the main, splash screen, and
// debug console). Caches the individual trees as they are produced.
// Re-renders when any tree changes.
class RenderTreeCombiner {
 public:
  // Layer represents the render layer corresponding to the main web
  // module, the splash screen, or the debug console and are used to
  // create and submit a combined tree to the RendererModule's
  // pipeline. Layers are combined in order of the |z_index| specifed
  // at the Layers' creation. The RenderTreeCombiner stores pointers
  // to Layers. The Layers are owned by the caller of
  // RenderTreeCombiner::CreateLayer.
  class Layer {
   public:
    ~Layer();

    void Reset() {
      render_tree_ = base::nullopt;
      receipt_time_ = base::nullopt;
    }

    // Submit render tree to the layer, and specify whether the time
    // received should be stored.
    void Submit(
        const base::optional<renderer::Submission>& render_tree_submission);

    bool HasRenderTree() { return !!render_tree_; }

    // Returns a current submission object that can be passed into a renderer
    // for rasterization.  If the render tree does not exist, this will
    // return a base::nullopt.
    base::optional<renderer::Submission> GetCurrentSubmission();

   private:
    friend class RenderTreeCombiner;

    explicit Layer(RenderTreeCombiner* render_tree_combiner = NULL);

    // Returns the current submission time for this particular layer.  This is
    // called by the RenderTreeCombiner on the |timeline_layer_| to determine
    // which value to pass in as the submission time for the renderer.
    base::optional<base::TimeDelta> CurrentTimeOffset();

    RenderTreeCombiner* render_tree_combiner_;

    base::optional<renderer::Submission> render_tree_;
    base::optional<base::TimeTicks> receipt_time_;
  };

  RenderTreeCombiner();
  ~RenderTreeCombiner() {}

  // Create a Layer with a given |z_index|. If a Layer already exists
  // at |z_index|, return NULL, and no Layer is created.
  scoped_ptr<Layer> CreateLayer(int z_index);

  // Returns a current submission object that can be passed into a renderer
  // for rasterization.  If no layers with render trees exist, this will return
  // a base::nullopt.
  base::optional<renderer::Submission> GetCurrentSubmission();

  // Names a single layer as the one responsible for providing the timeline
  // id and configuration to the output combined render tree.  Only a single
  // layer can be responsible for providing the timeline.
  void SetTimelineLayer(Layer* layer);

 private:
  // Returns true if the specified layer exists in this render tree combiner's
  // current list of layers (e.g. |layers_|).
  bool OwnsLayer(Layer* layer);

  // The layers keyed on their z_index.
  std::map<int, Layer*> layers_;

  // Removes a layer from |layers_|. Called by the Layer destructor.
  void RemoveLayer(const Layer* layer);

  // Which layer is currently controlling the receipt time submitted to the
  // rasterizer.
  RenderTreeCombiner::Layer* timeline_layer_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_RENDER_TREE_COMBINER_H_
