// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_OFFSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_OFFSET_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_timelinerangeoffset.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

class Document;
class Element;
class CSSValue;

struct TimelineOffset {
  using NamedRange = V8TimelineRange::Enum;

  TimelineOffset() = default;
  TimelineOffset(NamedRange name,
                 Length offset,
                 absl::optional<String> style_dependent_offset = absl::nullopt)
      : name(name),
        offset(offset),
        style_dependent_offset(style_dependent_offset) {}

  bool UpdateOffset(Element* element, CSSValue* value);

  NamedRange name = NamedRange::kNone;
  Length offset = Length::Fixed();
  absl::optional<String> style_dependent_offset;

  bool operator==(const TimelineOffset& other) const {
    return name == other.name && offset == other.offset &&
           style_dependent_offset == other.style_dependent_offset;
  }

  bool operator!=(const TimelineOffset& other) const {
    return !(*this == other);
  }

  static String TimelineRangeNameToString(NamedRange range_name);

  static absl::optional<TimelineOffset> Create(Element* element,
                                               String value,
                                               double default_percent,
                                               ExceptionState& exception_state);

  static absl::optional<TimelineOffset> Create(
      Element* element,
      const V8UnionStringOrTimelineRangeOffset* range_offset,
      double default_percent,
      ExceptionState& exception_state);

  // A length is style dependent if using a font relative or viewport relative
  // unit. We also classify all styles involving calc or var as style dependent.
  // A style-dependent value needs to be re-resolved after a style change.
  static bool IsStyleDependent(const CSSValue* value);

  static CSSValue* ParseOffset(Document* document, String css_text);

  static Length ResolveLength(Element* element, const CSSValue* value);

  String ToString() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_OFFSET_H_
