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
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/layout/benchmark_stat_names.h"
#include "cobalt/layout/box_generator.h"
#include "cobalt/layout/initial_containing_block.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/render_tree/animations/node_animations_map.h"

namespace cobalt {
namespace layout {

namespace {

class ScopedParagraph {
 public:
  explicit ScopedParagraph(const scoped_refptr<Paragraph>& paragraph)
      : paragraph_(paragraph) {}

  ~ScopedParagraph() { paragraph_->Close(); }

  scoped_refptr<Paragraph>& get() { return paragraph_; }

 private:
  scoped_refptr<Paragraph> paragraph_;
};

}  // namespace

void UpdateComputedStylesAndLayoutBoxTree(
    const icu::Locale& locale, const scoped_refptr<dom::Document>& document,
    UsedStyleProvider* used_style_provider,
    icu::BreakIterator* line_break_iterator,
    icu::BreakIterator* character_break_iterator,
    scoped_refptr<BlockLevelBlockContainerBox>* initial_containing_block) {
  TRACE_EVENT0("cobalt::layout", "UpdateComputedStylesAndLayoutBoxTree()");
  // Layout-related cleanup is performed on the UsedStyleProvider in this
  // object's destructor.
  UsedStyleProviderLayoutScope used_style_provider_layout_scope(
      used_style_provider);

  // Update the computed style of all elements in the DOM, if necessary.
  document->UpdateComputedStyles();

  // Create initial containing block.
  *initial_containing_block = CreateInitialContainingBlock(
      document->initial_computed_style(), document, used_style_provider);

  // Generate boxes.
  {
    TRACE_EVENT0("cobalt::layout", kBenchmarkStatBoxGeneration);
    ScopedParagraph scoped_paragraph(
        new Paragraph(locale, (*initial_containing_block)->GetBaseDirection(),
                      Paragraph::DirectionalEmbeddingStack(),
                      line_break_iterator, character_break_iterator));
    BoxGenerator root_box_generator(
        (*initial_containing_block)->css_computed_style_declaration(),
        (*initial_containing_block)
            ->css_computed_style_declaration()
            ->animations(),
        used_style_provider, line_break_iterator, character_break_iterator,
        &(scoped_paragraph.get()));
    document->html()->Accept(&root_box_generator);
    const Boxes& root_boxes = root_box_generator.boxes();
    for (Boxes::const_iterator root_box_iterator = root_boxes.begin();
         root_box_iterator != root_boxes.end(); ++root_box_iterator) {
      (*initial_containing_block)->AddChild(*root_box_iterator);
    }
  }

  // Split bidi level runs.
  // The bidi levels were calculated for the paragraphs during box generation.
  // Now the text boxes are split between level runs, so that they will be
  // reversible during layout without requiring additional run-induced splits.
  {
    TRACE_EVENT0("cobalt::layout", "SplitBidiLevelRuns");
    (*initial_containing_block)->SplitBidiLevelRuns();
  }

  // Update node cross-references.
  // "Cross-references" here refers to box node references to other boxes in the
  // box tree.  For example, stacking contexts and container boxes are setup
  // for each node in this pass.
  {
    TRACE_EVENT0("cobalt::layout", kBenchmarkStatUpdateCrossReferences);
    // Since scrolling is not supported in Cobalt, setting the fixed containing
    // block to be equal to the initial containing block will work perfectly.
    (*initial_containing_block)->UpdateCrossReferences();
  }

  // Layout.
  {
    TRACE_EVENT0("cobalt::layout", kBenchmarkStatUpdateUsedSizes);
    (*initial_containing_block)->set_left(LayoutUnit());
    (*initial_containing_block)->set_top(LayoutUnit());
    (*initial_containing_block)->UpdateSize(LayoutParams());
  }
}

RenderTreeWithAnimations Layout(
    const icu::Locale& locale, const scoped_refptr<dom::Document>& document,
    UsedStyleProvider* used_style_provider,
    icu::BreakIterator* line_break_iterator,
    icu::BreakIterator* character_break_iterator,
    scoped_refptr<BlockLevelBlockContainerBox>* initial_containing_block) {
  TRACE_EVENT0("cobalt::layout", "Layout()");
  UpdateComputedStylesAndLayoutBoxTree(
      locale, document, used_style_provider, line_break_iterator,
      character_break_iterator, initial_containing_block);

  // Add to render tree.
  render_tree::animations::NodeAnimationsMap::Builder
      node_animations_map_builder;
  render_tree::CompositionNode::Builder render_tree_root_builder;
  {
    TRACE_EVENT0("cobalt::layout", kBenchmarkStatRenderAndAnimate);
    (*initial_containing_block)
        ->RenderAndAnimate(&render_tree_root_builder,
                           &node_animations_map_builder, math::Vector2dF(0, 0));
  }

  return RenderTreeWithAnimations(
      new render_tree::CompositionNode(render_tree_root_builder.Pass()),
      new render_tree::animations::NodeAnimationsMap(
          node_animations_map_builder.Pass()));
}

}  // namespace layout
}  // namespace cobalt
