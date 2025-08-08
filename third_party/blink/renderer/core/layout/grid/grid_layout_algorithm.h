// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/grid/grid_break_token_data.h"
#include "third_party/blink/renderer/core/layout/grid/grid_node.h"
#include "third_party/blink/renderer/core/layout/grid/grid_placement.h"
#include "third_party/blink/renderer/core/layout/grid/grid_sizing_tree.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// This enum corresponds to each step used to accommodate grid items across
// intrinsic tracks according to their min and max track sizing functions, as
// defined in https://drafts.csswg.org/css-grid-2/#algo-spanning-items.
enum class GridItemContributionType {
  kForIntrinsicMinimums,
  kForContentBasedMinimums,
  kForMaxContentMinimums,
  kForIntrinsicMaximums,
  kForMaxContentMaximums,
  kForFreeSpace,
};

using GridItemDataPtrVector = Vector<GridItemData*, 16>;
using GridSetPtrVector = Vector<GridSet*, 16>;

class CORE_EXPORT GridLayoutAlgorithm
    : public NGLayoutAlgorithm<GridNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit GridLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  const NGLayoutResult* Layout() override;
  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) override;

  // Computes the containing block rect of out of flow items from stored data in
  // |GridLayoutData|.
  static LogicalRect ComputeOutOfFlowItemContainingRect(
      const GridPlacementData& placement_data,
      const GridLayoutData& layout_data,
      const ComputedStyle& grid_style,
      const BoxStrut& borders,
      const LogicalSize& border_box_size,
      GridItemData* out_of_flow_item);

  // Helper that computes tracks sizes in a given range.
  static Vector<std::div_t> ComputeTrackSizesInRange(
      const GridLayoutTrackCollection& track_collection,
      wtf_size_t range_starting_set_index,
      wtf_size_t range_set_count);

 private:
  friend class GridLayoutAlgorithmTest;

  // Aggregate all direct out of flow children from the current grid container
  // to |oof_children|, unless |oof_children| is not provided.
  wtf_size_t BuildGridSizingSubtree(
      GridSizingTree* sizing_tree,
      HeapVector<Member<LayoutBox>>* oof_children = nullptr,
      const SubgriddedItemData& opt_subgrid_data = kNoSubgriddedItemData,
      const GridLineResolver* parent_line_resolver = nullptr,
      bool must_ignore_children = false) const;

  GridSizingTree BuildGridSizingTree(
      HeapVector<Member<LayoutBox>>* oof_children = nullptr) const;
  GridSizingTree BuildGridSizingTreeIgnoringChildren() const;

  const NGLayoutResult* LayoutInternal();

  LayoutUnit Baseline(const GridLayoutData& layout_data,
                      const GridItemData& grid_item,
                      GridTrackSizingDirection track_direction) const;

  void ComputeGridGeometry(const GridSizingTree& grid_sizing_tree,
                           LayoutUnit* intrinsic_block_size);

  LayoutUnit ComputeIntrinsicBlockSizeIgnoringChildren() const;

  // Returns the size that a grid item will distribute across the tracks with an
  // intrinsic sizing function it spans in the relevant track direction.
  LayoutUnit ContributionSizeForGridItem(
      const GridSizingSubtree& sizing_subtree,
      GridItemContributionType contribution_type,
      GridTrackSizingDirection track_direction,
      SizingConstraint sizing_constraint,
      GridItemData* grid_item) const;

  wtf_size_t ComputeAutomaticRepetitions(
      const GridSpan& subgrid_span,
      GridTrackSizingDirection track_direction) const;

  // Subgrids compute auto repetitions differently than standalone grids.
  wtf_size_t ComputeAutomaticRepetitionsForSubgrid(
      wtf_size_t subgrid_span_size,
      GridTrackSizingDirection track_direction) const;

  // Determines the major/minor alignment baselines for each row/column based on
  // each item in |grid_items|, and stores the results in |track_collection|.
  void ComputeGridItemBaselines(const GridLayoutSubtree& layout_subtree,
                                const GridSizingSubtree& sizing_subtree,
                                GridTrackSizingDirection track_direction,
                                SizingConstraint sizing_constraint) const;

  std::unique_ptr<GridLayoutTrackCollection> CreateSubgridTrackCollection(
      const SubgriddedItemData& subgrid_data,
      GridTrackSizingDirection track_direction) const;

  // Initialize the track collections of a given grid sizing data.
  void InitializeTrackCollection(const SubgriddedItemData& opt_subgrid_data,
                                 GridTrackSizingDirection track_direction,
                                 GridLayoutData* layout_data) const;

  // Initializes the track sizes of a grid sizing subtree.
  void InitializeTrackSizes(const GridSizingSubtree& sizing_subtree,
                            const SubgriddedItemData& opt_subgrid_data,
                            const absl::optional<GridTrackSizingDirection>&
                                opt_track_direction) const;

  // Helper that calls the method above for the entire grid sizing tree.
  void InitializeTrackSizes(const GridSizingTree& sizing_tree,
                            const absl::optional<GridTrackSizingDirection>&
                                opt_track_direction = absl::nullopt) const;

  // Calculates from the min and max track sizing functions the used track size.
  void ComputeUsedTrackSizes(const GridSizingSubtree& sizing_subtree,
                             GridTrackSizingDirection track_direction,
                             SizingConstraint sizing_constraint,
                             bool* opt_needs_additional_pass) const;

  // Computes and caches the used track sizes of a grid sizing subtree.
  void CompleteTrackSizingAlgorithm(const GridSizingSubtree& sizing_subtree,
                                    const SubgriddedItemData& opt_subgrid_data,
                                    GridTrackSizingDirection track_direction,
                                    SizingConstraint sizing_constraint,
                                    bool* opt_needs_additional_pass) const;

  // Helper that calls the method above for the entire grid sizing tree.
  void CompleteTrackSizingAlgorithm(
      const GridSizingTree& sizing_tree,
      GridTrackSizingDirection track_direction,
      SizingConstraint sizing_constraint,
      bool* opt_needs_additional_pass = nullptr) const;

  // Performs the final baseline alignment pass of a grid sizing subtree.
  void ComputeBaselineAlignment(
      const GridLayoutSubtree& layout_subtree,
      const GridSizingSubtree& sizing_subtree,
      const SubgriddedItemData& opt_subgrid_data,
      const absl::optional<GridTrackSizingDirection>& opt_track_direction,
      SizingConstraint) const;

  // Helper that calls the method above for the entire grid sizing tree.
  void CompleteFinalBaselineAlignment(const GridSizingTree& sizing_tree) const;

  // Helper which iterates over the sizing tree, and instantiates a subgrid
  // algorithm to invoke the callback with.
  template <typename CallbackFunc>
  void ForEachSubgrid(const GridSizingSubtree& sizing_subtree,
                      const CallbackFunc& callback_func) const;

  LayoutUnit ComputeSubgridContributionSize(
      const GridSizingSubtree& sizing_subtree,
      GridTrackSizingDirection track_direction,
      SizingConstraint sizing_constraint) const;

  // These methods implement the steps of the algorithm for intrinsic track size
  // resolution defined in https://drafts.csswg.org/css-grid-2/#algo-content.
  void ResolveIntrinsicTrackSizes(const GridSizingSubtree& sizing_subtree,
                                  GridTrackSizingDirection track_direction,
                                  SizingConstraint sizing_constraint) const;

  void IncreaseTrackSizesToAccommodateGridItems(
      GridItemDataPtrVector::iterator group_begin,
      GridItemDataPtrVector::iterator group_end,
      const GridSizingSubtree& sizing_subtree,
      bool is_group_spanning_flex_track,
      SizingConstraint sizing_constraint,
      GridItemContributionType contribution_type,
      GridSizingTrackCollection* track_collection) const;

  void MaximizeTracks(SizingConstraint sizing_constraint,
                      GridSizingTrackCollection* track_collection) const;

  void StretchAutoTracks(SizingConstraint sizing_constraint,
                         GridSizingTrackCollection* track_collection) const;

  void ExpandFlexibleTracks(const GridSizingSubtree& sizing_subtree,
                            GridTrackSizingDirection track_direction,
                            SizingConstraint sizing_constraint) const;

  // Gets the specified [column|row]-gap of the grid.
  LayoutUnit GutterSize(
      GridTrackSizingDirection track_direction,
      LayoutUnit parent_grid_gutter_size = LayoutUnit()) const;

  LayoutUnit DetermineFreeSpace(
      SizingConstraint sizing_constraint,
      const GridSizingTrackCollection& track_collection) const;

  NGConstraintSpace CreateConstraintSpace(
      NGCacheSlot cache_slot,
      const GridItemData& grid_item,
      const LogicalSize& containing_grid_area_size,
      const LogicalSize& fixed_available_size,
      GridLayoutSubtree&& opt_layout_subtree = GridLayoutSubtree(),
      bool min_block_size_should_encompass_intrinsic_size = false,
      absl::optional<LayoutUnit> opt_fragment_relative_block_offset =
          absl::nullopt) const;

  // `containing_grid_area` is an optional out parameter that holds the computed
  // grid area (offset and size) of the specified grid item.
  NGConstraintSpace CreateConstraintSpaceForLayout(
      const GridItemData& grid_item,
      const GridLayoutData& layout_data,
      GridLayoutSubtree&& opt_layout_subtree = GridLayoutSubtree(),
      LogicalRect* containing_grid_area = nullptr,
      LayoutUnit unavailable_block_size = LayoutUnit(),
      bool min_block_size_should_encompass_intrinsic_size = false,
      absl::optional<LayoutUnit> opt_fragment_relative_block_offset =
          absl::nullopt) const;

  NGConstraintSpace CreateConstraintSpaceForMeasure(
      const SubgriddedItemData& subgridded_item,
      GridTrackSizingDirection track_direction,
      const LogicalSize& fixed_available_size = kIndefiniteLogicalSize) const;

  NGConstraintSpace CreateConstraintSpaceForSubgridAlgorithm(
      const SubgriddedItemData& subgrid_data) const;

  // Layout the |grid_items|, and add them to the builder.
  //
  // If |out_grid_items_placement_data| is present determine the offset for
  // each of the |grid_items| but *don't* add the resulting fragment to the
  // builder.
  //
  // This is used for fragmentation which requires us to know the final offset
  // of each item before fragmentation occurs.
  void PlaceGridItems(
      const GridSizingTree& sizing_tree,
      Vector<EBreakBetween>* out_row_break_between,
      Vector<GridItemPlacementData>* out_grid_items_placement_data = nullptr);

  // Layout the |grid_items| for fragmentation (when there is a known
  // fragmentainer size).
  //
  // This will go through all the grid_items and place fragments which belong
  // within this fragmentainer.
  void PlaceGridItemsForFragmentation(
      const GridSizingTree& sizing_tree,
      const Vector<EBreakBetween>& row_break_between,
      Vector<GridItemPlacementData>* grid_item_placement_data,
      Vector<LayoutUnit>* row_offset_adjustments,
      LayoutUnit* intrinsic_block_size,
      LayoutUnit* consumed_grid_block_size);

  // Computes the static position, grid area and its offset of out of flow
  // elements in the grid (as provided by `oof_children`).
  void PlaceOutOfFlowItems(const GridLayoutData& layout_data,
                           const LayoutUnit block_size,
                           HeapVector<Member<LayoutBox>>& oof_children);

  LayoutUnit ComputeGridItemAvailableSize(
      const GridItemData& grid_item,
      const GridLayoutTrackCollection& track_collection,
      LayoutUnit* start_offset = nullptr) const;

  LogicalSize grid_available_size_;
  LogicalSize grid_min_available_size_;
  LogicalSize grid_max_available_size_;

  absl::optional<LayoutUnit> contain_intrinsic_block_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_LAYOUT_ALGORITHM_H_
