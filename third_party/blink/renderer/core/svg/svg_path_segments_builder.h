// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_SEGMENTS_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_SEGMENTS_BUILDER_H_

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/svg/svg_path_consumer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class SVGPathSegment;

// Builds a `sequence<SVGPathSegment>` from the segments emitted by an
// `svg_path_parser::ParsePath()` walk, suitable for returning from
// `SVGPathElement.getPathData()`.
class SVGPathSegmentsBuilder final : public SVGPathConsumer {
  STACK_ALLOCATED();

 public:
  SVGPathSegmentsBuilder();

  void EmitSegment(const PathSegmentData&) override;

  HeapVector<Member<SVGPathSegment>> Finalize();

 private:
  HeapVector<Member<SVGPathSegment>> result_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_SEGMENTS_BUILDER_H_
