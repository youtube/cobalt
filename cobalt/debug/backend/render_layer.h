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

#ifndef COBALT_DEBUG_BACKEND_RENDER_LAYER_H_
#define COBALT_DEBUG_BACKEND_RENDER_LAYER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace debug {
namespace backend {

// Combines two render trees (layers) and passes the result on to a
// callback function when either tree changes.
// The signature of the callback is identical to that of the functions that set
// each layer, allowing multiple instances to be chained together to implement
// a simple stack of render layers.
class RenderLayer : public base::SupportsWeakPtr<RenderLayer> {
 public:
  // Callback
  typedef base::Callback<void(const scoped_refptr<render_tree::Node>& layer)>
      OnChangedCallback;

  explicit RenderLayer(const OnChangedCallback& on_changed_callback);

  void SetBackLayer(const scoped_refptr<render_tree::Node>& layer);
  void SetFrontLayer(const scoped_refptr<render_tree::Node>& layer);

 private:
  // Called when either the back or front layers are set.
  // Creates a CompositionNode to combine the front and back layers and passes
  // it to the callback specified in the constructor, |on_changed_callback_|.
  void Combine();

  OnChangedCallback on_changed_callback_;
  scoped_refptr<render_tree::Node> back_layer_;
  scoped_refptr<render_tree::Node> front_layer_;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_RENDER_LAYER_H_
