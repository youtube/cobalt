// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_types.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_node.h"

namespace blink {

class NGBlockBreakToken;
class NGTableBorders;

class CORE_EXPORT NGTableLayoutAlgorithm
    : public NGLayoutAlgorithm<NGTableNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGTableLayoutAlgorithm(const NGLayoutAlgorithmParams& params)
      : NGLayoutAlgorithm(params) {}
  const NGLayoutResult* Layout() override;

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) override;

  static LayoutUnit ComputeTableInlineSize(const NGTableNode& node,
                                           const NGConstraintSpace& space,
                                           const NGBoxStrut& border_padding);

  // Useful when trying to compute table's block sizes.
  // Table's css block size specifies size of the grid, not size
  // of the wrapper. Wrapper's block size = grid size + caption block size.
  static LayoutUnit ComputeCaptionBlockSize(const NGTableNode& node,
                                            const NGConstraintSpace& space,
                                            const LayoutUnit table_inline_size);

  // In order to correctly determine the available block-size given to the
  // table-grid, we need to layout all the captions ahead of time. This struct
  // stores the necessary information to add them to the fragment later.
  struct CaptionResult {
    DISALLOW_NEW();

   public:
    void Trace(Visitor* visitor) const {
      visitor->Trace(node);
      visitor->Trace(layout_result);
    }

    NGBlockNode node;
    Member<const NGLayoutResult> layout_result;
    const NGBoxStrut margins;
  };

 private:
  const NGLayoutResult* RelayoutAsLastTableBox();

  void ComputeRows(const LayoutUnit table_grid_inline_size,
                   const NGTableGroupedChildren& grouped_children,
                   const Vector<NGTableColumnLocation>& column_locations,
                   const NGTableBorders& table_borders,
                   const LogicalSize& border_spacing,
                   const NGBoxStrut& table_border_padding,
                   const LayoutUnit captions_block_size,
                   NGTableTypes::Rows* rows,
                   NGTableTypes::CellBlockConstraints* cell_block_constraints,
                   NGTableTypes::Sections* sections,
                   LayoutUnit* minimal_table_grid_block_size);

  void ComputeTableSpecificFragmentData(
      const NGTableGroupedChildren& grouped_children,
      const Vector<NGTableColumnLocation>& column_locations,
      const NGTableTypes::Rows& rows,
      const NGTableBorders& table_borders,
      const LogicalRect& table_grid_rect,
      LayoutUnit table_grid_block_size);

  const NGLayoutResult* GenerateFragment(
      LayoutUnit table_inline_size,
      LayoutUnit minimal_table_grid_block_size,
      const NGTableGroupedChildren& grouped_children,
      const Vector<NGTableColumnLocation>& column_locations,
      const NGTableTypes::Rows& rows,
      const NGTableTypes::CellBlockConstraints& cell_block_constraints,
      const NGTableTypes::Sections& sections,
      const HeapVector<CaptionResult>& captions,
      const NGTableBorders& table_borders,
      const LogicalSize& border_spacing);

  LayoutUnit total_table_min_block_size_;

  // Set to true when we're re-laying out without repeating table headers and
  // footers.
  bool is_known_to_be_last_table_box_ = false;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGTableLayoutAlgorithm::CaptionResult)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_H_
