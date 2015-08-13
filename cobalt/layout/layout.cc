/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/layout/layout.h"

#include "base/debug/trace_event.h"
#include "cobalt/cssom/computed_style.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/specified_style.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/rule_matching.h"
#include "cobalt/layout/block_formatting_block_container_box.h"
#include "cobalt/layout/box_generator.h"
#include "cobalt/layout/initial_containing_block.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/render_tree/animations/node_animations_map.h"

namespace cobalt {
namespace layout {

RenderTreeWithAnimations Layout(
    const scoped_refptr<dom::Window>& window,
    const scoped_refptr<cssom::CSSStyleSheet>& user_agent_style_sheet,
    render_tree::ResourceProvider* resource_provider,
    icu::BreakIterator* line_break_iterator, loader::ImageCache* image_cache) {
  TRACE_EVENT0("cobalt::layout", "Layout()");

  scoped_refptr<dom::Document> document = window->document();

  // Update the computed style of all elements in the DOM, if necessary.
  scoped_refptr<cssom::CSSStyleDeclarationData> initial_containing_block_style =
      CreateInitialContainingBlockComputedStyle(window);

  document->UpdateComputedStyles(
      initial_containing_block_style,
      user_agent_style_sheet);

  UsedStyleProvider used_style_provider(resource_provider, image_cache);

  // Create initial containing block.
  scoped_ptr<BlockLevelBlockContainerBox> initial_containing_block =
      CreateInitialContainingBlock(
          initial_containing_block_style, window->document(),
          &used_style_provider);

  // Generate boxes.
  {
    TRACE_EVENT0("cobalt::layout", "BoxGeneration");
    scoped_refptr<Paragraph> paragraph;
    BoxGenerator root_box_generator(initial_containing_block->computed_style(),
                                    &used_style_provider, line_break_iterator,
                                    &paragraph);
    document->html()->Accept(&root_box_generator);
    BoxGenerator::Boxes root_boxes = root_box_generator.PassBoxes();
    for (BoxGenerator::Boxes::iterator root_box_iterator = root_boxes.begin();
         root_box_iterator != root_boxes.end(); ++root_box_iterator) {
      // Transfer the ownership of the root box from |ScopedVector|
      // to |scoped_ptr|.
      scoped_ptr<Box> root_box(*root_box_iterator);
      *root_box_iterator = NULL;

      initial_containing_block->AddChild(root_box.Pass());
    }
  }

  // Split bidi level runs.
  // The bidi levels were calculated for the paragraphs during box generation.
  // Now the text boxes are split between level runs, so that they will be
  // reversible during layout without requiring additional run-induced splits.
  {
    TRACE_EVENT0("cobalt::layout", "SplitBidiLevelRuns");
    initial_containing_block->SplitBidiLevelRuns();
  }

  // Update node cross-references.
  // "Cross-references" here refers to box node references to other boxes in the
  // box tree.  For example, stacking contexts and container boxes are setup
  // for each node in this pass.
  {
    TRACE_EVENT0("cobalt::layout", "UpdateCrossReferences");
    initial_containing_block->UpdateCrossReferences();
  }

  // Layout.
  {
    TRACE_EVENT0("cobalt::layout", "UpdateUsedSizes");
    initial_containing_block->set_used_left(0);
    initial_containing_block->set_used_top(0);
    initial_containing_block->UpdateUsedSizeIfInvalid(LayoutParams());
  }

  // Add to render tree.
  render_tree::animations::NodeAnimationsMap::Builder
      node_animations_map_builder;
  render_tree::CompositionNode::Builder render_tree_root_builder;
  {
    TRACE_EVENT0("cobalt::layout", "AddToRenderTree");
    initial_containing_block->AddToRenderTree(&render_tree_root_builder,
                                              &node_animations_map_builder);
  }

  return RenderTreeWithAnimations(
      new render_tree::CompositionNode(render_tree_root_builder.Pass()),
      new render_tree::animations::NodeAnimationsMap(
          node_animations_map_builder.Pass()));
}

}  // namespace layout
}  // namespace cobalt
