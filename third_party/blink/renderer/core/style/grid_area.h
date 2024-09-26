/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_AREA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_AREA_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/style/grid_enums.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Legacy grid expands out auto-repeaters, so it has a lower cap than GridNG.
// Note that this actually allows a [-999,999] range.
const int kLegacyGridMaxTracks = 1000;
const int kGridMaxTracks = 10000000;

// A span in a single direction (either rows or columns). Note that |start_line|
// and |end_line| are grid lines' indexes.
// Despite line numbers in the spec start in "1", the indexes here start in "0".
struct GridSpan {
  USING_FAST_MALLOC(GridSpan);

 public:
  static GridSpan UntranslatedDefiniteGridSpan(int start_line, int end_line) {
    return GridSpan(start_line, end_line, kUntranslatedDefinite);
  }

  static GridSpan TranslatedDefiniteGridSpan(wtf_size_t start_line,
                                             wtf_size_t end_line) {
    return GridSpan(start_line, end_line, kTranslatedDefinite);
  }

  static GridSpan IndefiniteGridSpan(wtf_size_t span_size = 1) {
    return GridSpan(wtf_size_t{0}, span_size, kIndefinite);
  }

  bool operator==(const GridSpan& o) const {
    return type_ == o.type_ && start_line_ == o.start_line_ &&
           end_line_ == o.end_line_;
  }

  bool operator<(const GridSpan& o) const {
    DCHECK(IsTranslatedDefinite());
    return start_line_ < o.start_line_ ||
           (start_line_ == o.start_line_ && end_line_ < o.end_line_);
  }

  bool operator<=(const GridSpan& o) const {
    DCHECK(IsTranslatedDefinite());
    return *this < o || *this == o;
  }

  bool Contains(wtf_size_t line) const {
    DCHECK(IsTranslatedDefinite());
    DCHECK_GE(start_line_, 0);
    DCHECK_GE(end_line_, 0);
    return line >= static_cast<wtf_size_t>(start_line_) &&
           line <= static_cast<wtf_size_t>(end_line_);
  }

  wtf_size_t IntegerSpan() const {
    DCHECK(IsTranslatedDefinite());
    DCHECK_GT(end_line_, start_line_);
    return end_line_ - start_line_;
  }

  wtf_size_t IndefiniteSpanSize() const {
    DCHECK(IsIndefinite());
    DCHECK_EQ(start_line_, 0);
    DCHECK_GT(end_line_, 0);
    return end_line_;
  }

  int UntranslatedStartLine() const {
    DCHECK_EQ(type_, kUntranslatedDefinite);
    return start_line_;
  }

  int UntranslatedEndLine() const {
    DCHECK_EQ(type_, kUntranslatedDefinite);
    return end_line_;
  }

  wtf_size_t StartLine() const {
    DCHECK(IsTranslatedDefinite());
    DCHECK_GE(start_line_, 0);
    return start_line_;
  }

  wtf_size_t EndLine() const {
    DCHECK(IsTranslatedDefinite());
    DCHECK_GT(end_line_, 0);
    return end_line_;
  }

  struct GridSpanIterator {
    GridSpanIterator(wtf_size_t v) : value(v) {}

    wtf_size_t operator*() const { return value; }
    wtf_size_t operator++() { return value++; }
    bool operator!=(GridSpanIterator other) const {
      return value != other.value;
    }

    wtf_size_t value;
  };

  GridSpanIterator begin() const {
    DCHECK(IsTranslatedDefinite());
    return start_line_;
  }

  GridSpanIterator end() const {
    DCHECK(IsTranslatedDefinite());
    return end_line_;
  }

  bool IsUntranslatedDefinite() const { return type_ == kUntranslatedDefinite; }
  bool IsTranslatedDefinite() const { return type_ == kTranslatedDefinite; }
  bool IsIndefinite() const { return type_ == kIndefinite; }

  void Translate(wtf_size_t offset) {
    DCHECK_NE(type_, kIndefinite);
    *this =
        GridSpan(start_line_ + offset, end_line_ + offset, kTranslatedDefinite);
  }

 private:
  enum GridSpanType { kUntranslatedDefinite, kTranslatedDefinite, kIndefinite };

  template <typename T>
  GridSpan(T start_line, T end_line, GridSpanType type) : type_(type) {
    const int grid_max_tracks = kGridMaxTracks;
    start_line_ =
        ClampTo<int>(start_line, -grid_max_tracks, grid_max_tracks - 1);
    end_line_ = ClampTo<int>(end_line, start_line_ + 1, grid_max_tracks);

#if DCHECK_IS_ON()
    DCHECK_LT(start_line_, end_line_);
    if (type == kTranslatedDefinite) {
      DCHECK_GE(start_line_, 0);
    }
#endif
  }

  int start_line_;
  int end_line_;
  GridSpanType type_;
};

// This represents a grid area that spans in both rows' and columns' direction.
struct GridArea {
  USING_FAST_MALLOC(GridArea);

 public:
  // HashMap requires a default constuctor.
  GridArea()
      : columns(GridSpan::IndefiniteGridSpan()),
        rows(GridSpan::IndefiniteGridSpan()) {}

  GridArea(const GridSpan& r, const GridSpan& c) : columns(c), rows(r) {}

  const GridSpan& Span(GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? columns : rows;
  }

  void SetSpan(const GridSpan& span, GridTrackSizingDirection track_direction) {
    if (track_direction == kForColumns) {
      columns = span;
    } else {
      rows = span;
    }
  }

  wtf_size_t StartLine(GridTrackSizingDirection track_direction) const {
    return Span(track_direction).StartLine();
  }

  wtf_size_t EndLine(GridTrackSizingDirection track_direction) const {
    return Span(track_direction).EndLine();
  }

  wtf_size_t SpanSize(GridTrackSizingDirection track_direction) const {
    return Span(track_direction).IntegerSpan();
  }

  bool operator==(const GridArea& o) const {
    return columns == o.columns && rows == o.rows;
  }

  bool operator!=(const GridArea& o) const { return !(*this == o); }

  GridSpan columns;
  GridSpan rows;
};

typedef HashMap<String, GridArea> NamedGridAreaMap;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_AREA_H_
