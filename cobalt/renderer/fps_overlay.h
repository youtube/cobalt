// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_FPS_OVERLAY_H_
#define COBALT_RENDERER_FPS_OVERLAY_H_

#include "base/memory/ref_counted.h"
#include "cobalt/base/c_val_collection_timer_stats.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace renderer {

class FpsOverlay {
 public:
  explicit FpsOverlay(render_tree::ResourceProvider* resource_provider);

  void UpdateOverlay(
      const base::CValCollectionTimerStatsFlushResults& fps_stats);
  scoped_refptr<render_tree::Node> AnnotateRenderTreeWithOverlay(
      render_tree::Node* original_tree);

 private:
  render_tree::ResourceProvider* resource_provider_;
  scoped_refptr<render_tree::Font> font_;

  scoped_refptr<render_tree::Node> cached_overlay_;
};

}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_FPS_OVERLAY_H_
