// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef COBALT_OVERLAY_INFO_QR_CODE_OVERLAY_H_
#define COBALT_OVERLAY_INFO_QR_CODE_OVERLAY_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "cobalt/math/size.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace overlay_info {

class QrCodeOverlay {
 public:
  typedef base::Callback<void(const scoped_refptr<render_tree::Node>&)>
      RenderTreeProducedCB;

  QrCodeOverlay(const math::Size& screen_size,
                render_tree::ResourceProvider* resource_provider,
                const RenderTreeProducedCB& render_tree_produced_cb);

  void SetSize(const math::Size& size);
  void SetResourceProvider(render_tree::ResourceProvider* resource_provider);

 private:
  void UpdateRenderTree();

  RenderTreeProducedCB render_tree_produced_cb_;
  math::Size screen_size_;
  render_tree::ResourceProvider* resource_provider_;
};

}  // namespace overlay_info
}  // namespace cobalt

#endif  // COBALT_OVERLAY_INFO_QR_CODE_OVERLAY_H_
