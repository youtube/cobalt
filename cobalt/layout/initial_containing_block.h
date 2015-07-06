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

#ifndef LAYOUT_INITIAL_CONTAINING_BLOCK_H_
#define LAYOUT_INITIAL_CONTAINING_BLOCK_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/dom/window.h"
#include "cobalt/layout/block_formatting_block_container_box.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

// The containing block in which the root element lives is a rectangle called
// the initial containing block. For continuous media, it has the dimensions
// of the viewport and is anchored at the canvas origin. This calculates the
// style for the initial containing block.
//   http://www.w3.org/TR/CSS2/visudet.html#containing-block-details
scoped_refptr<cssom::CSSStyleDeclarationData>
    CreateInitialContainingBlockComputedStyle(
        const scoped_refptr<dom::Window>& window);

// This creates the initial containing block after adding background color
// and image to the initial style, when needed.
//   http://www.w3.org/TR/CSS2/visudet.html#containing-block-details
scoped_ptr<BlockLevelBlockContainerBox> CreateInitialContainingBlock(
    const scoped_refptr<dom::Window>& window,
    const UsedStyleProvider* used_style_provider);

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_INITIAL_CONTAINING_BLOCK_H_
