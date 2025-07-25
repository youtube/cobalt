// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_LOGICAL_SIZE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_LOGICAL_SIZE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

struct LogicalOffset;

// LogicalSize is the size of rect (typically a fragment) in the logical
// coordinate system.
// For more information about physical and logical coordinate systems, see:
// https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/layout/README.md#coordinate-spaces
struct CORE_EXPORT LogicalSize {
  constexpr LogicalSize() = default;
  constexpr LogicalSize(LayoutUnit inline_size, LayoutUnit block_size)
      : inline_size(inline_size), block_size(block_size) {}

  // This is deleted to avoid unwanted lossy conversion from float or double to
  // LayoutUnit or int. Use explicit LayoutUnit constructor for each parameter
  // instead.
  LogicalSize(double, double) = delete;

  // For testing only. It's defined in core/testing/core_unit_test_helper.h.
  // 'constexpr' is to let compiler detect usage from production code.
  constexpr LogicalSize(int inline_size, int block_size);

  // Use ToPhysicalSize to convert to a physical size.

  LayoutUnit inline_size;
  LayoutUnit block_size;

  constexpr bool operator==(const LogicalSize& other) const {
    return std::tie(other.inline_size, other.block_size) ==
           std::tie(inline_size, block_size);
  }
  constexpr bool operator!=(const LogicalSize& other) const {
    return !(*this == other);
  }

  constexpr bool IsEmpty() const {
    return inline_size == LayoutUnit() || block_size == LayoutUnit();
  }

  void Expand(LayoutUnit inline_offset, LayoutUnit block_offset) {
    inline_size += inline_offset;
    block_size += block_offset;
  }

  void Shrink(LayoutUnit inline_offset, LayoutUnit block_offset) {
    inline_size -= inline_offset;
    block_size -= block_offset;
  }

  LogicalSize ClampNegativeToZero() const {
    return LogicalSize(inline_size.ClampNegativeToZero(),
                       block_size.ClampNegativeToZero());
  }
};

constexpr LogicalSize kIndefiniteLogicalSize(kIndefiniteSize, kIndefiniteSize);

inline LogicalSize operator-(const LogicalSize& a, const BoxStrut& b) {
  return {a.inline_size - b.InlineSum(), a.block_size - b.BlockSum()};
}

inline LogicalSize& operator-=(LogicalSize& a, const BoxStrut& b) {
  a.inline_size -= b.InlineSum();
  a.block_size -= b.BlockSum();
  return a;
}

inline LogicalOffset operator+(const LogicalOffset& offset,
                               const LogicalSize& size) {
  return {offset.inline_offset + size.inline_size,
          offset.block_offset + size.block_size};
}

inline LogicalOffset& operator+=(LogicalOffset& offset,
                                 const LogicalSize& size) {
  offset = offset + size;
  return offset;
}

CORE_EXPORT std::ostream& operator<<(std::ostream&, const LogicalSize&);

// LogicalDelta resolves the ambiguity of subtractions.
//
// "offset - offset" is ambiguous because both of below are true:
//   offset + offset = offset
//   offset + size = offset
//
// LogicalDelta resolves this ambiguity by allowing implicit conversions both
// to LogicalOffset and to LogicalSize.
struct CORE_EXPORT LogicalDelta : public LogicalSize {
 public:
  using LogicalSize::LogicalSize;
  constexpr operator LogicalOffset() const { return {inline_size, block_size}; }
};

inline LogicalDelta operator-(const LogicalOffset& a, const LogicalOffset& b) {
  return {a.inline_offset - b.inline_offset, a.block_offset - b.block_offset};
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_LOGICAL_SIZE_H_
