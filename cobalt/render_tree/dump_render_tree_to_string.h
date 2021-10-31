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

#ifndef COBALT_RENDER_TREE_DUMP_RENDER_TREE_TO_STRING_H_
#define COBALT_RENDER_TREE_DUMP_RENDER_TREE_TO_STRING_H_

#include <string>

#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace render_tree {

// Returns a string that is a serialized text representation of the render tree.
// The string does not contain all of the information present within the input
// tree, but rather information that was [subjectively] deemed relevant to
// debugging it.
std::string DumpRenderTreeToString(render_tree::Node* node);

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_DUMP_RENDER_TREE_TO_STRING_H_
