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

#ifndef COBALT_RENDERER_TEST_SCENES_GROWING_RECT_SCENE_H_
#define COBALT_RENDERER_TEST_SCENES_GROWING_RECT_SCENE_H_

#include "base/time.h"
#include "cobalt/math/size_f.h"
#include "cobalt/renderer/test/scenes/scene_helpers.h"

namespace cobalt {
namespace renderer {
namespace test {
namespace scenes {

// This scene gives a gray rectangle centered in the screen that slowly grows
// to fill the screen.
scoped_refptr<render_tree::Node> CreateGrowingRectScene(
    const math::SizeF& output_dimensions, base::TimeDelta start_time);

}  // namespace scenes
}  // namespace test
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_TEST_SCENES_GROWING_RECT_SCENE_H_
