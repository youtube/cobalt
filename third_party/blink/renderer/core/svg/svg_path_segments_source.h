// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_SEGMENTS_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_SEGMENTS_SOURCE_H_

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class SVGPathSegment;

// Path source over a setPathData() segment list, for ParsePath(). An invalid
// entry (or leading non-moveto) yields kPathSegUnknown, stopping the parse.
class SVGPathSegmentsSource {
  STACK_ALLOCATED();

 public:
  explicit SVGPathSegmentsSource(
      const HeapVector<Member<SVGPathSegment>>& segments)
      : segments_(segments) {}
  SVGPathSegmentsSource(const SVGPathSegmentsSource&) = delete;
  SVGPathSegmentsSource& operator=(const SVGPathSegmentsSource&) = delete;

  bool HasMoreData() const { return index_ < segments_.size(); }
  PathSegmentData ParseSegment();

 private:
  const HeapVector<Member<SVGPathSegment>>& segments_;
  wtf_size_t index_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_SEGMENTS_SOURCE_H_
