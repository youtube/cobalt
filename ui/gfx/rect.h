/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UI_GFX_RECT_H_
#define UI_GFX_RECT_H_

#include "cobalt/math/rect.h"
#include "ui/gfx/rect_f.h"

namespace gfx {

// Introduce cobalt::math::Rect into namespace gfx as gfx::Rect is used by
// media.
using cobalt::math::Rect;
using cobalt::math::RectF;

}  // namespace gfx

#endif  // UI_GFX_RECT_H_
