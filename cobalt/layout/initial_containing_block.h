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

#ifndef COBALT_LAYOUT_INITIAL_CONTAINING_BLOCK_H_
#define COBALT_LAYOUT_INITIAL_CONTAINING_BLOCK_H_

#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/window.h"
#include "cobalt/layout/block_formatting_block_container_box.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

// The containing block in which the root element lives is a rectangle called
// the initial containing block. For continuous media, it has the dimensions
// of the viewport and is anchored at the canvas origin.
//   https://www.w3.org/TR/CSS2/visudet.html#containing-block-details

struct InitialContainingBlockCreationResults {
  // Created initial containing block box results.
  scoped_refptr<BlockLevelBlockContainerBox> box;

  // The initial containing block may have its background style propagated into
  // it from the HTML or BODY element.  This value will point to which element
  // the value was propagated from, or it will be NULL if no style was
  // propagated.
  dom::HTMLElement* background_style_source;
};

// This creates the initial containing block after adding background color
// and image to the initial style, when needed.
InitialContainingBlockCreationResults CreateInitialContainingBlock(
    const cssom::CSSComputedStyleData& default_initial_containing_block_style,
    const scoped_refptr<dom::Document>& document,
    UsedStyleProvider* used_style_provider,
    LayoutStatTracker* layout_stat_tracker);

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_INITIAL_CONTAINING_BLOCK_H_
