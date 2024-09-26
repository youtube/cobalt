// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fieldset_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"

namespace blink {

void NGBaseLayoutAlgorithmTest::SetUp() {
  EnableCompositing();
  RenderingTest::SetUp();
}

void NGBaseLayoutAlgorithmTest::AdvanceToLayoutPhase() {
  if (GetDocument().Lifecycle().GetState() ==
      DocumentLifecycle::kInPerformLayout)
    return;
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInPerformLayout);
}

const NGPhysicalBoxFragment* NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithm(
    NGBlockNode node,
    const NGConstraintSpace& space,
    const NGBreakToken* break_token) {
  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);

  const NGLayoutResult* result =
      NGBlockLayoutAlgorithm(
          {node, fragment_geometry, space, To<NGBlockBreakToken>(break_token)})
          .Layout();

  return To<NGPhysicalBoxFragment>(&result->PhysicalFragment());
}

std::pair<const NGPhysicalBoxFragment*, NGConstraintSpace>
NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithmForElement(Element* element) {
  auto* block_flow = To<LayoutBlockFlow>(element->GetLayoutObject());
  NGBlockNode node(block_flow);
  NGConstraintSpace space =
      NGConstraintSpace::CreateFromLayoutObject(*block_flow);
  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);

  const NGLayoutResult* result =
      NGBlockLayoutAlgorithm({node, fragment_geometry, space}).Layout();
  return std::make_pair(To<NGPhysicalBoxFragment>(&result->PhysicalFragment()),
                        std::move(space));
}

const NGPhysicalBoxFragment*
NGBaseLayoutAlgorithmTest::RunFieldsetLayoutAlgorithm(
    NGBlockNode node,
    const NGConstraintSpace& space,
    const NGBreakToken* break_token) {
  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);

  const NGLayoutResult* result =
      NGFieldsetLayoutAlgorithm(
          {node, fragment_geometry, space, To<NGBlockBreakToken>(break_token)})
          .Layout();

  return To<NGPhysicalBoxFragment>(&result->PhysicalFragment());
}

const NGPhysicalBoxFragment*
NGBaseLayoutAlgorithmTest::GetBoxFragmentByElementId(const char* id) {
  LayoutObject* layout_object = GetLayoutObjectByElementId(id);
  CHECK(layout_object && layout_object->IsLayoutNGObject());
  const NGPhysicalBoxFragment* fragment =
      To<LayoutBlockFlow>(layout_object)->GetPhysicalFragment(0);
  CHECK(fragment);
  return fragment;
}

const NGPhysicalBoxFragment* NGBaseLayoutAlgorithmTest::CurrentFragmentFor(
    const LayoutNGBlockFlow* block_flow) {
  return block_flow->GetPhysicalFragment(0);
}

const NGPhysicalBoxFragment* FragmentChildIterator::NextChild(
    PhysicalOffset* fragment_offset) {
  if (!parent_)
    return nullptr;
  if (index_ >= parent_->Children().size())
    return nullptr;
  while (parent_->Children()[index_]->Type() !=
         NGPhysicalFragment::kFragmentBox) {
    ++index_;
    if (index_ >= parent_->Children().size())
      return nullptr;
  }
  auto& child = parent_->Children()[index_++];
  if (fragment_offset)
    *fragment_offset = child.Offset();
  return To<NGPhysicalBoxFragment>(child.get());
}

NGConstraintSpace ConstructBlockLayoutTestConstraintSpace(
    WritingDirectionMode writing_direction,
    LogicalSize size,
    bool stretch_inline_size_if_auto,
    bool is_new_formatting_context,
    LayoutUnit fragmentainer_space_available) {
  NGFragmentationType block_fragmentation =
      fragmentainer_space_available != kIndefiniteSize
          ? NGFragmentationType::kFragmentColumn
          : NGFragmentationType::kFragmentNone;

  NGConstraintSpaceBuilder builder(writing_direction.GetWritingMode(),
                                   writing_direction,
                                   is_new_formatting_context);
  builder.SetAvailableSize(size);
  builder.SetPercentageResolutionSize(size);
  builder.SetInlineAutoBehavior(stretch_inline_size_if_auto
                                    ? NGAutoBehavior::kStretchImplicit
                                    : NGAutoBehavior::kFitContent);
  builder.SetFragmentainerBlockSize(fragmentainer_space_available);
  builder.SetFragmentationType(block_fragmentation);
  if (block_fragmentation != NGFragmentationType::kFragmentNone)
    builder.SetShouldPropagateChildBreakValues();
  return builder.ToConstraintSpace();
}

}  // namespace blink
