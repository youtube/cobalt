// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_sizing_algorithm.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/masonry/masonry_running_positions.h"

namespace blink {

namespace {

}  // namespace

class MasonryLayoutAlgorithmTest : public BaseLayoutAlgorithmTest {
 protected:
  void SetUp() override { BaseLayoutAlgorithmTest::SetUp(); }

  void ComputeGeometry(const MasonryLayoutAlgorithm& algorithm) {
    wtf_size_t start_offset;
    const auto& style = algorithm.Style();
    const GridLineResolver line_resolver(style, /*auto_repetitions=*/0);

    grid_axis_tracks_ = algorithm.BuildGridAxisTracks(
        line_resolver, SizingConstraint::kLayout, start_offset);

    const auto grid_axis_direction = grid_axis_tracks_->Direction();
    ASSERT_EQ(grid_axis_direction, style.MasonryTrackSizingDirection());

    for (const auto& masonry_item :
         algorithm.BuildVirtualMasonryItems(line_resolver, start_offset)) {
      MasonryItemCachedData item_data;

      item_data.resolved_span =
          masonry_item.resolved_position.Span(grid_axis_direction);
      if (masonry_item.contribution_sizes) {
        item_data.contribution_sizes = *masonry_item.contribution_sizes;
      }
      virtual_items_data_.emplace_back(std::move(item_data));
    }
  }

  wtf_size_t VirtualItemCount() { return virtual_items_data_.size(); }
  const GridRangeVector& Ranges() { return grid_axis_tracks_->ranges_; }

  Vector<LayoutUnit> TrackSizes() {
    Vector<LayoutUnit> track_sizes;
    for (wtf_size_t i = 0; i < grid_axis_tracks_->GetSetCount(); ++i) {
      track_sizes.push_back(grid_axis_tracks_->GetSetOffset(i + 1) -
                            grid_axis_tracks_->GetSetOffset(i));
    }
    return track_sizes;
  }

  LayoutUnit MaxContentContribution(wtf_size_t index) {
    return VirtualItemData(index).contribution_sizes.max_size;
  }

  LayoutUnit MinContentContribution(wtf_size_t index) {
    return VirtualItemData(index).contribution_sizes.min_size;
  }

  const GridSpan& VirtualItemSpan(wtf_size_t index) {
    return VirtualItemData(index).resolved_span;
  }

  const Vector<LayoutUnit>& GetRunningPositions(
      const MasonryRunningPositions& running_positions) {
    return running_positions.running_positions_;
  }

  Vector<LayoutUnit> GetMaxPositionsForAllTracks(
      const MasonryRunningPositions& running_positions,
      wtf_size_t span_size) {
    return running_positions.GetMaxPositionsForAllTracks(span_size);
  }

  MasonryRunningPositions InitializeMasonryRunningPositions(
      const Vector<LayoutUnit>& running_positions,
      LayoutUnit tie_threshold) {
    return MasonryRunningPositions(running_positions, tie_threshold);
  }

  void SetAutoPlacementCursor(wtf_size_t cursor,
                              MasonryRunningPositions& running_positions) {
    running_positions.SetAutoPlacementCursorForTesting(cursor);
  }

 private:
  struct MasonryItemCachedData {
    MinMaxSizes contribution_sizes;
    GridSpan resolved_span{GridSpan::IndefiniteGridSpan()};
  };

  const MasonryItemCachedData& VirtualItemData(wtf_size_t index) {
    DCHECK_LT(index, virtual_items_data_.size());
    return virtual_items_data_[index];
  }

  std::optional<GridSizingTrackCollection> grid_axis_tracks_;

  // Virtual items represent the contributions of item groups in track sizing
  // and are not directly related to any children of the container.
  Vector<MasonryItemCachedData> virtual_items_data_;
};

TEST_F(MasonryLayoutAlgorithmTest, ConstructMasonryItems) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #masonry {
      display: masonry;
      grid-template-columns: auto auto [header-start] auto auto [header-end];
    }
    </style>
    <div id="masonry">
      <div>1</div>
      <div style="grid-column: 3 / span 2">2</div>
      <div style="grid-column: span 2">3</div>
      <div style="grid-column: span 3">4</div>
      <div style="grid-column: 2 / 5">5</div>
      <div style="grid-column: header-start / header-end">1</div>
      <div style="grid-column: 1 / header-start">2</div>
      <div style="grid-column: 3 / header-end">2</div>
    </div>
  )HTML");

  MasonryNode node(GetLayoutBoxByElementId("masonry"));

  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  const auto masonry_items =
      node.ConstructMasonryItems(line_resolver, /*start_offset=*/0);

  const Vector<GridSpan> expected_spans = {
      GridSpan::IndefiniteGridSpan(1),
      GridSpan::TranslatedDefiniteGridSpan(2, 4),
      GridSpan::IndefiniteGridSpan(2),
      GridSpan::IndefiniteGridSpan(3),
      GridSpan::TranslatedDefiniteGridSpan(1, 4),
      GridSpan::TranslatedDefiniteGridSpan(2, 4),
      GridSpan::TranslatedDefiniteGridSpan(0, 2),
      GridSpan::TranslatedDefiniteGridSpan(2, 4)};

  EXPECT_EQ(masonry_items.Size(), expected_spans.size());

  const auto grid_axis_direction = node.Style().MasonryTrackSizingDirection();
  for (wtf_size_t i = 0; const auto& masonry_item : masonry_items) {
    EXPECT_EQ(masonry_item.resolved_position.Span(grid_axis_direction),
              expected_spans[i++]);
  }
}

TEST_F(MasonryLayoutAlgorithmTest, BuildRanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #masonry {
      display: masonry;
      grid-template-columns: 5% repeat(3, 10px auto) repeat(1, auto 5px 1fr);
    }
    </style>
    <div id="masonry">
      <div style="grid-column: span 2 / 1"></div>
      <div style="grid-column: 9 / span 5"></div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // The first item spans 2 tracks before the explicit grid, creating the first
  // range of 2 tracks. Then follows the template track ranges: one range of a
  // single track for the `5%`, then a range for the `repeat(3, ...)` which
  // spans 6 tracks. The last repeat creates a range of 3 tracks, but it's split
  // by the second item, creating one range of 1 track and another of 2 tracks.
  // Finally, the second item spans a range of 3 tracks past the explicit grid.
  const Vector<wtf_size_t> expected_start_lines = {0, 2, 3, 9, 10, 12};
  const Vector<wtf_size_t> expected_track_counts = {2, 1, 6, 1, 2, 3};

  const auto& ranges = Ranges();
  EXPECT_EQ(ranges.size(), expected_start_lines.size());

  for (wtf_size_t i = 0; i < ranges.size(); ++i) {
    EXPECT_EQ(ranges[i].start_line, expected_start_lines[i]);
    EXPECT_EQ(ranges[i].track_count, expected_track_counts[i]);
    EXPECT_FALSE(ranges[i].IsCollapsed());
  }
}

TEST_F(MasonryLayoutAlgorithmTest, BuildFixedTrackSizes) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #masonry {
      display: masonry;
      grid-template-columns: 5% repeat(3, 10px 15%) repeat(1, 15px 5px 20px);
    }
    </style>
    <div id="masonry"></div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(TrackSizes(), Vector<LayoutUnit>({LayoutUnit(5), LayoutUnit(30),
                                              LayoutUnit(45), LayoutUnit(15),
                                              LayoutUnit(5), LayoutUnit(20)}));
}

TEST_F(MasonryLayoutAlgorithmTest, CollectMasonryItemGroups) {
  SetBodyInnerHTML(R"HTML(
    <div id="masonry" style="display: masonry">
      <div></div>
      <div style="grid-column: 1"></div>
      <div style="grid-column: 1 / 4"></div>
      <div style="grid-column: span 3"></div>
      <div style="grid-column: span 3 / 4"></div>
      <div></div>
    </div>
  )HTML");

  MasonryNode node(GetLayoutBoxByElementId("masonry"));

  wtf_size_t max_end_line, start_offset;
  const GridLineResolver line_resolver(node.Style(), /*auto_repetitions=*/0);
  const auto item_groups =
      node.CollectItemGroups(line_resolver, max_end_line, start_offset);

  EXPECT_EQ(item_groups.size(), 4u);

  for (const auto& [items, properties] : item_groups) {
    wtf_size_t expected_size = 0;
    const auto& span = properties.Span();
    if (span == GridSpan::IndefiniteGridSpan(3) ||
        span == GridSpan::TranslatedDefiniteGridSpan(0, 1)) {
      expected_size = 1;
    } else if (span == GridSpan::IndefiniteGridSpan(1) ||
               span == GridSpan::TranslatedDefiniteGridSpan(0, 3)) {
      expected_size = 2;
    }
    EXPECT_EQ(items.size(), expected_size);
  }
}

TEST_F(MasonryLayoutAlgorithmTest, ExplicitlyPlacedVirtualItems) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { font: 10px/1 Ahem }
    #masonry {
      display: masonry;
      grid-template-columns: repeat(2, 100px);
    }
    </style>
    <div id="masonry">
      <div style="grid-column: 1">XX XX</div>
      <div style="grid-column: -4 / 3">XXX X</div>
      <div style="grid-column: span 3 / 3">X XX X</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  const auto item_count = VirtualItemCount();
  EXPECT_EQ(item_count, 2u);

  for (wtf_size_t i = 0; i < item_count; ++i) {
    LayoutUnit expected_max_size, expected_min_size;
    const auto& span = VirtualItemSpan(i);
    if (span == GridSpan::TranslatedDefiniteGridSpan(1, 2)) {
      expected_max_size = LayoutUnit(50);
      expected_min_size = LayoutUnit(20);
    } else if (span == GridSpan::TranslatedDefiniteGridSpan(0, 3)) {
      expected_max_size = LayoutUnit(60);
      expected_min_size = LayoutUnit(30);
    }
    EXPECT_EQ(MaxContentContribution(i), expected_max_size);
    EXPECT_EQ(MinContentContribution(i), expected_min_size);
  }
}

TEST_F(MasonryLayoutAlgorithmTest, AutoPlacedVirtualItems) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { font: 10px/1 Ahem }
    #masonry {
      display: masonry;
      grid-template-columns: repeat(3, auto);
    }
    </style>
    <div id="masonry">
      <div>X X X X X</div>
      <div style="grid-column: span 2">XXX X</div>
      <div>XX XX XX XX XX</div>
      <div style="grid-column: span 2">X XX X</div>
      <div>X XX XXX XX X</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  const auto item_count = VirtualItemCount();
  EXPECT_EQ(item_count, 5u);

  for (wtf_size_t i = 0; i < item_count; ++i) {
    LayoutUnit expected_max_size, expected_min_size;
    const auto& span = VirtualItemSpan(i);
    if (span == GridSpan::TranslatedDefiniteGridSpan(0, 2) ||
        span == GridSpan::TranslatedDefiniteGridSpan(1, 3)) {
      expected_max_size = LayoutUnit(60);
      expected_min_size = LayoutUnit(30);
    } else if (span == GridSpan::TranslatedDefiniteGridSpan(0, 1) ||
               span == GridSpan::TranslatedDefiniteGridSpan(1, 2) ||
               span == GridSpan::TranslatedDefiniteGridSpan(2, 3)) {
      expected_max_size = LayoutUnit(140);
      expected_min_size = LayoutUnit(30);
    }
    EXPECT_EQ(MaxContentContribution(i), expected_max_size);
    EXPECT_EQ(MinContentContribution(i), expected_min_size);
  }
}

TEST_F(MasonryLayoutAlgorithmTest, BuildIntrinsicTrackSizes) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { font: 10px/1 Ahem }
    #masonry {
      display: masonry;
      grid-template-columns: min-content max-content;
    }
    </style>
    <div id="masonry">
      <div style="grid-column: 1">XX XX</div>
      <div style="grid-column: 2">XX XX</div>
      <div style="grid-column: 1 / 3">XXX XXXXXX XXXXXXXXX</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(TrackSizes(),
            Vector<LayoutUnit>({LayoutUnit(30), LayoutUnit(170)}));
}

TEST_F(MasonryLayoutAlgorithmTest, MaximizeAndStretchAutoTracks) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    body { font: 10px/1 Ahem }
    #masonry {
      display: masonry;
      grid-template-columns: minmax(15px, min-content) max-content auto;
    }
    </style>
    <div id="masonry">
      <div style="grid-column: 1">XXX XXX</div>
      <div style="grid-column: 1 / 3">X XX X</div>
    </div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  // First track starts at 15px, but should be resolved to 30px (which is the
  // min-content size of the first item) later in the maximize tracks step.
  // To acomodate the max-content size of the second item, which is 60px minus
  // 15px that the first track already has, the second track expands to 45px.
  // Finally, the last track takes the remaining space after the first two
  // tracks are maximized, which is 100px - 30px - 45px = 25px.
  EXPECT_EQ(TrackSizes(), Vector<LayoutUnit>({LayoutUnit(30), LayoutUnit(45),
                                              LayoutUnit(25)}));
}

TEST_F(MasonryLayoutAlgorithmTest, ExpandFlexibleTracks) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #masonry {
      display: masonry;
      grid-template-columns: 1fr 5fr 3fr 1fr;
    }
    </style>
    <div id="masonry"></div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(TrackSizes(), Vector<LayoutUnit>({LayoutUnit(10), LayoutUnit(50),
                                              LayoutUnit(30), LayoutUnit(10)}));
}

TEST_F(MasonryLayoutAlgorithmTest, BuildRowSizes) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #masonry {
      height: 100px;
      display: masonry;
      masonry-direction: row;
      grid-template-rows: 20px 1fr 30%;
    }
    </style>
    <div id="masonry"></div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeGeometry(algorithm);

  EXPECT_EQ(TrackSizes(), Vector<LayoutUnit>({LayoutUnit(20), LayoutUnit(50),
                                              LayoutUnit(30)}));
}

TEST_F(MasonryLayoutAlgorithmTest, UpdateRunningPositionsForSpan) {
  MasonryRunningPositions running_positions(
      /*track_count=*/4,
      /*initial_running_position=*/LayoutUnit(),
      /*tie_threshold=*/LayoutUnit());

  Vector<LayoutUnit> expected_running_positions = {
      LayoutUnit(0), LayoutUnit(3), LayoutUnit(3), LayoutUnit(0)};
  running_positions.UpdateRunningPositionsForSpan(
      GridSpan::TranslatedDefiniteGridSpan(1, 3), LayoutUnit(3));
  EXPECT_EQ(expected_running_positions, GetRunningPositions(running_positions));

  expected_running_positions = {LayoutUnit(4), LayoutUnit(4), LayoutUnit(4),
                                LayoutUnit(4)};
  running_positions.UpdateRunningPositionsForSpan(
      GridSpan::TranslatedDefiniteGridSpan(0, 4), LayoutUnit(4));
  EXPECT_EQ(expected_running_positions, GetRunningPositions(running_positions));

  expected_running_positions = {LayoutUnit(4), LayoutUnit(4), LayoutUnit(5),
                                LayoutUnit(5)};
  running_positions.UpdateRunningPositionsForSpan(
      GridSpan::TranslatedDefiniteGridSpan(2, 4), LayoutUnit(5));
  EXPECT_EQ(expected_running_positions, GetRunningPositions(running_positions));
}

TEST_F(MasonryLayoutAlgorithmTest, GetFirstEligibleLine) {
  auto running_positions = InitializeMasonryRunningPositions(
      {LayoutUnit(2.0), LayoutUnit(3.0), LayoutUnit(3.5), LayoutUnit(2.5)},
      /*tie_threshold=*/LayoutUnit(0.5));

  SetAutoPlacementCursor(1, running_positions);
  LayoutUnit max_position;
  EXPECT_EQ(
      running_positions.GetFirstEligibleLine(/*span_size=*/2, max_position),
      GridSpan::TranslatedDefiniteGridSpan(1, 3));
  EXPECT_EQ(max_position, LayoutUnit(3.5));

  EXPECT_EQ(
      running_positions.GetFirstEligibleLine(/*span_size=*/1, max_position),
      GridSpan::TranslatedDefiniteGridSpan(3, 4));
  EXPECT_EQ(max_position, LayoutUnit(2.5));

  EXPECT_EQ(
      running_positions.GetFirstEligibleLine(/*span_size=*/4, max_position),
      GridSpan::TranslatedDefiniteGridSpan(0, 4));
  EXPECT_EQ(max_position, LayoutUnit(3.5));

  SetAutoPlacementCursor(2, running_positions);
  EXPECT_EQ(
      running_positions.GetFirstEligibleLine(/*span_size=*/2, max_position),
      GridSpan::TranslatedDefiniteGridSpan(2, 4));
  EXPECT_EQ(max_position, LayoutUnit(3.5));

  SetAutoPlacementCursor(3, running_positions);
  EXPECT_EQ(
      running_positions.GetFirstEligibleLine(/*span_size=*/2, max_position),
      GridSpan::TranslatedDefiniteGridSpan(0, 2));
  EXPECT_EQ(max_position, LayoutUnit(3));

  SetAutoPlacementCursor(4, running_positions);
  EXPECT_EQ(
      running_positions.GetFirstEligibleLine(/*span_size=*/2, max_position),
      GridSpan::TranslatedDefiniteGridSpan(0, 2));
  EXPECT_EQ(max_position, LayoutUnit(3));
}

TEST_F(MasonryLayoutAlgorithmTest, GetMaxPositionsForAllTracks) {
  auto running_positions = InitializeMasonryRunningPositions(
      {LayoutUnit(2.0), LayoutUnit(3.0), LayoutUnit(3.5), LayoutUnit(2.5)},
      /*tie_threshold=*/LayoutUnit());

  EXPECT_EQ(
      GetMaxPositionsForAllTracks(running_positions, /*span_size=*/2),
      Vector<LayoutUnit>({LayoutUnit(3), LayoutUnit(3.5), LayoutUnit(3.5)}));
  EXPECT_EQ(GetMaxPositionsForAllTracks(running_positions, /*span_size=*/4),
            Vector<LayoutUnit>({LayoutUnit(3.5)}));
  EXPECT_EQ(GetMaxPositionsForAllTracks(running_positions, /*span_size=*/1),
            Vector<LayoutUnit>({LayoutUnit(2.0), LayoutUnit(3.0),
                                LayoutUnit(3.5), LayoutUnit(2.5)}));
}

}  // namespace blink
