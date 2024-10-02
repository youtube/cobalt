// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timeline_offset.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/resolver/element_resolve_context.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

namespace {

void ThrowExcpetionForInvalidTimelineOffset(ExceptionState& exception_state) {
  exception_state.ThrowTypeError(
      "Animation range must be a name <length-percent> pair");
}

}  // anonymous namespace

/* static */
String TimelineOffset::TimelineRangeNameToString(
    TimelineOffset::NamedRange range_name) {
  switch (range_name) {
    case NamedRange::kNone:
      return "none";

    case NamedRange::kCover:
      return "cover";

    case NamedRange::kContain:
      return "contain";

    case NamedRange::kEntry:
      return "entry";

    case NamedRange::kEntryCrossing:
      return "entry-crossing";

    case NamedRange::kExit:
      return "exit";

    case NamedRange::kExitCrossing:
      return "exit-crossing";
  }
}

String TimelineOffset::ToString() const {
  if (name == NamedRange::kNone) {
    return "normal";
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*MakeGarbageCollected<CSSIdentifierValue>(name));
  list->Append(*CSSValue::Create(offset, 1));
  return list->CssText();
}

/* static */
absl::optional<TimelineOffset> TimelineOffset::Create(
    Element* element,
    String css_text,
    double default_percent,
    ExceptionState& exception_state) {
  if (!element) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Unable to parse TimelineOffset from CSS text with a null effect or "
        "target");
    return absl::nullopt;
  }

  Document& document = element->GetDocument();

  CSSTokenizer tokenizer(css_text);
  Vector<CSSParserToken, 32> tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  range.ConsumeWhitespace();

  const CSSValue* value = css_parsing_utils::ConsumeAnimationRange(
      range, *document.ElementSheet().Contents()->ParserContext(),
      /* default_offset_percent */ default_percent);

  if (!value || !range.AtEnd()) {
    ThrowExcpetionForInvalidTimelineOffset(exception_state);
    return absl::nullopt;
  }

  if (IsA<CSSIdentifierValue>(value)) {
    DCHECK_EQ(CSSValueID::kNormal, To<CSSIdentifierValue>(*value).GetValueID());
    return absl::nullopt;
  }

  const auto& list = To<CSSValueList>(*value);

  // TODO(kevers): Keep track of style dependent lengths in order
  // to re-resolve on a style update.
  DCHECK(list.length());
  NamedRange range_name = NamedRange::kNone;
  Length offset = Length::Percent(default_percent);
  if (list.Item(0).IsIdentifierValue()) {
    range_name = To<CSSIdentifierValue>(list.Item(0)).ConvertTo<NamedRange>();
    if (list.length() == 2u) {
      offset = ResolveLength(element, &list.Item(1));
    }
  } else {
    offset = ResolveLength(element, &list.Item(0));
  }

  return TimelineOffset(range_name, offset);
}

/* static */
absl::optional<TimelineOffset> TimelineOffset::Create(
    Element* element,
    const V8UnionStringOrTimelineRangeOffset* range_offset,
    double default_percent,
    ExceptionState& exception_state) {
  if (range_offset->IsString()) {
    return Create(element, range_offset->GetAsString(), default_percent,
                  exception_state);
  }

  TimelineRangeOffset* value = range_offset->GetAsTimelineRangeOffset();
  NamedRange name =
      value->hasRangeName() ? value->rangeName().AsEnum() : NamedRange::kNone;

  Length parsed_offset;
  if (value->hasOffset()) {
    CSSNumericValue* offset = value->offset();
    const CSSPrimitiveValue* css_value =
        DynamicTo<CSSPrimitiveValue>(offset->ToCSSValue());

    if (!css_value || (!css_value->IsPx() && !css_value->IsPercentage() &&
                       !css_value->IsCalculatedPercentageWithLength())) {
      exception_state.ThrowTypeError(
          "CSSNumericValue must be a length or percentage for animation "
          "range.");
      return absl::nullopt;
    }

    if (css_value->IsPx()) {
      parsed_offset = Length::Fixed(css_value->GetDoubleValue());
    } else if (css_value->IsPercentage()) {
      parsed_offset = Length::Percent(css_value->GetDoubleValue());
    } else {
      DCHECK(css_value->IsCalculatedPercentageWithLength());
      parsed_offset = TimelineOffset::ResolveLength(element, css_value);
    }
  } else {
    parsed_offset = Length::Percent(default_percent);
  }

  return TimelineOffset(name, parsed_offset);
}

/* static */
Length TimelineOffset::ResolveLength(Element* element, const CSSValue* value) {
  ElementResolveContext element_resolve_context(*element);
  Document& document = element->GetDocument();
  // TODO(kevers): Re-resolve any value that is not px or % on a style change.
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData length_conversion_data(
      element->ComputedStyleRef(), element_resolve_context.ParentStyle(),
      element_resolve_context.RootElementStyle(), document.GetLayoutView(),
      CSSToLengthConversionData::ContainerSizes(element),
      element->GetComputedStyle()->EffectiveZoom(), ignored_flags);

  return DynamicTo<CSSPrimitiveValue>(value)->ConvertToLength(
      length_conversion_data);
}

}  // namespace blink
