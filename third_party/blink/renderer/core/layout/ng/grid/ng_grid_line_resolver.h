// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LINE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LINE_RESOLVER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/style/computed_grid_track_list.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/core/style/grid_enums.h"
#include "third_party/blink/renderer/core/style/named_grid_lines_map.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

struct GridSpan;
class ComputedStyle;
class GridPosition;
class NGGridNamedLineCollection;

// This is a utility class with all the code related to grid items positions
// resolution.
class NGGridLineResolver {
  DISALLOW_NEW();

 public:
  NGGridLineResolver() = default;

  explicit NGGridLineResolver(const ComputedStyle& grid_style,
                              wtf_size_t column_auto_repetitions,
                              wtf_size_t row_auto_repetitions)
      : style_(&grid_style),
        column_auto_repetitions_(column_auto_repetitions),
        row_auto_repetitions_(row_auto_repetitions) {}

  // Subgrids need to map named lines from every parent grid. This constructor
  // should be used exclusively by subgrids to differentiate such scenario.
  explicit NGGridLineResolver(const ComputedStyle& grid_style,
                              const NGGridLineResolver& parent_line_resolver,
                              GridArea subgrid_area,
                              wtf_size_t column_auto_repetitions,
                              wtf_size_t row_auto_repetitions);

  bool operator==(const NGGridLineResolver& other) const;

  wtf_size_t ExplicitGridColumnCount() const;

  wtf_size_t ExplicitGridRowCount() const;

  wtf_size_t ExplicitGridTrackCount(
      GridTrackSizingDirection track_direction) const;

  wtf_size_t AutoRepetitions(GridTrackSizingDirection track_direction) const;

  wtf_size_t AutoRepeatTrackCount(
      GridTrackSizingDirection track_direction) const;

  wtf_size_t SubgridSpanSize(GridTrackSizingDirection track_direction) const;

  bool HasStandaloneAxis(GridTrackSizingDirection track_direction) const;

  wtf_size_t SpanSizeForAutoPlacedItem(
      const ComputedStyle& grid_item_style,
      GridTrackSizingDirection track_direction) const;

  GridSpan ResolveGridPositionsFromStyle(
      const ComputedStyle& grid_item_style,
      GridTrackSizingDirection track_direction) const;

 private:
  const NamedGridLinesMap& ImplicitNamedLinesMap(
      GridTrackSizingDirection track_direction) const;

  const NamedGridLinesMap& ExplicitNamedLinesMap(
      GridTrackSizingDirection track_direction) const;

  const NamedGridLinesMap& AutoRepeatLineNamesMap(
      GridTrackSizingDirection track_direction) const;

  const blink::ComputedGridTrackList& ComputedGridTrackList(
      GridTrackSizingDirection track_direction) const;

  GridSpan ResolveGridPositionAgainstOppositePosition(
      int opposite_line,
      const GridPosition& position,
      GridPositionSide side) const;

  GridSpan ResolveNamedGridLinePositionAgainstOppositePosition(
      int opposite_line,
      const GridPosition& position,
      GridPositionSide side) const;

  int ResolveGridPosition(const GridPosition& position,
                          GridPositionSide side) const;

  wtf_size_t ExplicitGridSizeForSide(GridPositionSide side) const;

  wtf_size_t LookAheadForNamedGridLine(
      int start,
      wtf_size_t number_of_lines,
      wtf_size_t grid_last_line,
      NGGridNamedLineCollection& lines_collection) const;

  int LookBackForNamedGridLine(
      int end,
      wtf_size_t number_of_lines,
      int grid_last_line,
      NGGridNamedLineCollection& lines_collection) const;

  wtf_size_t SpanSizeFromPositions(const GridPosition& initial_position,
                                   const GridPosition& final_position) const;

  int ResolveNamedGridLinePosition(const GridPosition& position,
                                   GridPositionSide side) const;

  void InitialAndFinalPositionsFromStyle(
      const ComputedStyle& grid_item_style,
      GridTrackSizingDirection track_direction,
      GridPosition& initial_position,
      GridPosition& final_position) const;

  GridSpan DefiniteGridSpanWithNamedSpanAgainstOpposite(
      int opposite_line,
      const GridPosition& position,
      GridPositionSide side,
      int last_line,
      NGGridNamedLineCollection& lines_collection) const;

  bool IsSubgridded(GridTrackSizingDirection track_direction) const;

  scoped_refptr<const ComputedStyle> style_;

  wtf_size_t column_auto_repetitions_{1};
  wtf_size_t row_auto_repetitions_{1};
  wtf_size_t subgridded_column_span_size_{kNotFound};
  wtf_size_t subgridded_row_span_size_{kNotFound};

  absl::optional<NamedGridLinesMap>
      subgridded_columns_merged_explicit_grid_line_names_;
  absl::optional<NamedGridLinesMap>
      subgridded_rows_merged_explicit_grid_line_names_;

  absl::optional<NamedGridLinesMap>
      subgridded_columns_merged_implicit_grid_line_names_;
  absl::optional<NamedGridLinesMap>
      subgridded_rows_merged_implicit_grid_line_names_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_LINE_RESOLVER_H_
