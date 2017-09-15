// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SHADER_PRELOAD_TREE_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SHADER_PRELOAD_TREE_H_

#include "base/memory/ref_counted.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// Generates a tree which includes common shaders that skia will generate
// and use. Submitting this tree to the skia renderer will cause the
// shaders to generated and compiled.
scoped_refptr<render_tree::Node> GenerateShaderPreloadTree(
    render_tree::ResourceProvider* resource_provider);

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SHADER_PRELOAD_TREE_H_
