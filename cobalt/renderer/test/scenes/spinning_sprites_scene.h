// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_TEST_SCENES_SPINNING_SPRITES_SCENE_H_
#define COBALT_RENDERER_TEST_SCENES_SPINNING_SPRITES_SCENE_H_

#include "base/time/time.h"
#include "cobalt/math/size_f.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/test/scenes/scene_helpers.h"

namespace cobalt {
namespace renderer {
namespace test {
namespace scenes {

// This scene will create a plurality of randomly positioned and sized sprites
// that are all spinning at different rates.
scoped_refptr<render_tree::Node> CreateSpinningSpritesScene(
    render_tree::ResourceProvider* resource_provider,
    const math::SizeF& output_dimensions, base::TimeDelta start_time);

}  // namespace scenes
}  // namespace test
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_TEST_SCENES_SPINNING_SPRITES_SCENE_H_
