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

#ifndef COBALT_DEBUG_BACKEND_RENDER_OVERLAY_H_
#define COBALT_DEBUG_BACKEND_RENDER_OVERLAY_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/layout/layout_manager.h"
#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace debug {
namespace backend {

// Inserted into the render callback pipeline to add additional nodes to the
// render tree.
class RenderOverlay {
 public:
  typedef layout::LayoutManager::LayoutResults LayoutResults;

  typedef base::Callback<void(const LayoutResults&)>
      OnRenderTreeProducedCallback;

  explicit RenderOverlay(
      const OnRenderTreeProducedCallback& render_tree_produced_callback);

  // Called when the input layout changes.
  void OnRenderTreeProduced(const LayoutResults& layout_results);

  // Updates the overlay that will be added to the input layout.
  void SetOverlay(const scoped_refptr<render_tree::Node>& overlay);

  void ClearInput();

 private:
  // Adds the overlay (if any) to the input layout and passes on to the
  // callback specified in the constructor.
  void Process();

  OnRenderTreeProducedCallback render_tree_produced_callback_;
  scoped_refptr<render_tree::Node> overlay_;
  LayoutResults input_layout_;
  base::Optional<base::TimeTicks> input_receipt_time_;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_RENDER_OVERLAY_H_
