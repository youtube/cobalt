// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LAYOUT_LAYOUT_H_
#define COBALT_LAYOUT_LAYOUT_H_

#include "cobalt/dom/document.h"
#include "cobalt/layout/block_formatting_block_container_box.h"
#include "cobalt/loader/image/image_cache.h"
#include "cobalt/math/size_f.h"
#include "cobalt/render_tree/animations/animate_node.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider.h"
#include "third_party/icu/source/common/unicode/brkiter.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace cobalt {
namespace layout {

class UsedStyleProvider;

// Layout engine supports a subset of CSS.
// TODO: Add markdown doc of supported CSS subset.
//
// Most of the code conforms to CSS Level 3 specifications, although the basic
// box model is intentionally implemented after CSS 2.1
// (https://www.w3.org/TR/CSS2/visuren.html) as recommended by a newer draft
// (http://dev.w3.org/csswg/css-box/) which is undergoing active changes.

// Update the computed styles, then generate and layout the box tree produced
// by the given document.
void UpdateComputedStylesAndLayoutBoxTree(
    const icu::Locale& locale, const scoped_refptr<dom::Document>& document,
    int dom_max_element_depth, UsedStyleProvider* used_style_provider,
    LayoutStatTracker* layout_stat_tracker,
    icu::BreakIterator* line_break_iterator,
    icu::BreakIterator* character_break_iterator,
    scoped_refptr<BlockLevelBlockContainerBox>* initial_containing_block,
    bool clear_window_with_background_color);

// Generates the render tree (along with corresponding animations) of the box
// tree contained within the provided containing block.
scoped_refptr<render_tree::Node> GenerateRenderTreeFromBoxTree(
    UsedStyleProvider* used_style_provider,
    LayoutStatTracker* layout_stat_tracker,
    scoped_refptr<BlockLevelBlockContainerBox>* initial_containing_block);

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_LAYOUT_H_
