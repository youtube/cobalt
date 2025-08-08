// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/animation/timeline_offset.h"
#include "third_party/blink/renderer/core/css/css_content_distribution_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_pending_system_font_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_property_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/font_variant_alternates_parser.h"
#include "third_party/blink/renderer/core/css/parser/font_variant_east_asian_parser.h"
#include "third_party/blink/renderer/core/css/parser/font_variant_ligatures_parser.h"
#include "third_party/blink/renderer/core/css/parser/font_variant_numeric_parser.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/properties/shorthands.h"
#include "third_party/blink/renderer/core/css/zoom_adjusted_pixel_value.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

// Implementations of methods in Shorthand subclasses that aren't generated.

namespace blink {
namespace css_shorthand {

namespace {

// New animation-* properties are  "reset only":
// https://github.com/w3c/csswg-drafts/issues/6946#issuecomment-1233190360
bool IsResetOnlyAnimationProperty(CSSPropertyID property) {
  switch (property) {
    case CSSPropertyID::kAnimationDelayEnd:
    case CSSPropertyID::kAnimationTimeline:
    case CSSPropertyID::kAnimationRangeStart:
    case CSSPropertyID::kAnimationRangeEnd:
      return true;
    default:
      return false;
  }
}

// Legacy parsing allows <string>s for animation-name.
CSSValue* ConsumeAnimationValue(CSSPropertyID property,
                                CSSParserTokenRange& range,
                                const CSSParserContext& context,
                                bool use_legacy_parsing) {
  switch (property) {
    case CSSPropertyID::kAnimationDelay:
      DCHECK(!RuntimeEnabledFeatures::CSSAnimationDelayStartEndEnabled());
      return css_parsing_utils::ConsumeTime(
          range, context, CSSPrimitiveValue::ValueRange::kAll);
    case CSSPropertyID::kAnimationDelayStart:
      DCHECK(RuntimeEnabledFeatures::CSSAnimationDelayStartEndEnabled());
      return css_parsing_utils::ConsumeAnimationDelay(range, context);
    case CSSPropertyID::kAnimationDelayEnd:
      // New animation-* properties are  "reset only", see
      // IsResetOnlyAnimationProperty.
      //
      // Returning nullptr here means that AnimationDelayEnd::InitialValue will
      // be used.
      DCHECK(RuntimeEnabledFeatures::CSSAnimationDelayStartEndEnabled());
      return nullptr;
    case CSSPropertyID::kAnimationDirection:
      return css_parsing_utils::ConsumeIdent<
          CSSValueID::kNormal, CSSValueID::kAlternate, CSSValueID::kReverse,
          CSSValueID::kAlternateReverse>(range);
    case CSSPropertyID::kAnimationDuration:
      return css_parsing_utils::ConsumeAnimationDuration(range, context);
    case CSSPropertyID::kAnimationFillMode:
      return css_parsing_utils::ConsumeIdent<
          CSSValueID::kNone, CSSValueID::kForwards, CSSValueID::kBackwards,
          CSSValueID::kBoth>(range);
    case CSSPropertyID::kAnimationIterationCount:
      return css_parsing_utils::ConsumeAnimationIterationCount(range, context);
    case CSSPropertyID::kAnimationName:
      return css_parsing_utils::ConsumeAnimationName(range, context,
                                                     use_legacy_parsing);
    case CSSPropertyID::kAnimationPlayState:
      return css_parsing_utils::ConsumeIdent<CSSValueID::kRunning,
                                             CSSValueID::kPaused>(range);
    case CSSPropertyID::kAnimationTimingFunction:
      return css_parsing_utils::ConsumeAnimationTimingFunction(range, context);
    case CSSPropertyID::kAnimationTimeline:
    case CSSPropertyID::kAnimationRangeStart:
    case CSSPropertyID::kAnimationRangeEnd:
      // New animation-* properties are  "reset only", see kAnimationDelayEnd.
      DCHECK(RuntimeEnabledFeatures::ScrollTimelineEnabled());
      return nullptr;
    default:
      NOTREACHED();
      return nullptr;
  }
}

bool ParseAnimationShorthand(const StylePropertyShorthand& shorthand,
                             bool important,
                             CSSParserTokenRange& range,
                             const CSSParserContext& context,
                             const CSSParserLocalContext& local_context,
                             HeapVector<CSSPropertyValue, 64>& properties) {
  const unsigned longhand_count = shorthand.length();

  HeapVector<Member<CSSValueList>, css_parsing_utils::kMaxNumAnimationLonghands>
      longhands(longhand_count);
  if (!css_parsing_utils::ConsumeAnimationShorthand(
          shorthand, longhands, ConsumeAnimationValue,
          IsResetOnlyAnimationProperty, range, context,
          local_context.UseAliasParsing())) {
    return false;
  }

  for (unsigned i = 0; i < longhand_count; ++i) {
    css_parsing_utils::AddProperty(
        shorthand.properties()[i]->PropertyID(), shorthand.id(), *longhands[i],
        important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
        properties);
  }
  return range.AtEnd();
}

const CSSValue* CSSValueFromComputedAnimation(
    const StylePropertyShorthand& shorthand,
    const CSSAnimationData* animation_data) {
  if (animation_data) {
    // The shorthand can not represent the following properties if they have
    // non-initial values. This is because they are always reset to their
    // initial value by the shorthand.
    if (!animation_data->HasSingleInitialTimeline() ||
        !animation_data->HasSingleInitialDelayEnd() ||
        !animation_data->HasSingleInitialRangeStart() ||
        !animation_data->HasSingleInitialRangeEnd()) {
      return nullptr;
    }

    CSSValueList* animations_list = CSSValueList::CreateCommaSeparated();
    for (wtf_size_t i = 0; i < animation_data->NameList().size(); ++i) {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(*ComputedStyleUtils::ValueForAnimationDuration(
          CSSTimingData::GetRepeated(animation_data->DurationList(), i),
          /* resolve_auto_to_zero */ true));
      list->Append(*ComputedStyleUtils::ValueForAnimationTimingFunction(
          CSSTimingData::GetRepeated(animation_data->TimingFunctionList(), i)));
      list->Append(*ComputedStyleUtils::ValueForAnimationDelayStart(
          CSSTimingData::GetRepeated(animation_data->DelayStartList(), i)));
      list->Append(*ComputedStyleUtils::ValueForAnimationIterationCount(
          CSSTimingData::GetRepeated(animation_data->IterationCountList(), i)));
      list->Append(*ComputedStyleUtils::ValueForAnimationDirection(
          CSSTimingData::GetRepeated(animation_data->DirectionList(), i)));
      list->Append(*ComputedStyleUtils::ValueForAnimationFillMode(
          CSSTimingData::GetRepeated(animation_data->FillModeList(), i)));
      list->Append(*ComputedStyleUtils::ValueForAnimationPlayState(
          CSSTimingData::GetRepeated(animation_data->PlayStateList(), i)));
      list->Append(*MakeGarbageCollected<CSSCustomIdentValue>(
          animation_data->NameList()[i]));
      animations_list->Append(*list);
    }
    return animations_list;
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  // animation-name default value.
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kNone));
  list->Append(*ComputedStyleUtils::ValueForAnimationDuration(
      CSSAnimationData::InitialDuration(),
      /* resolve_auto_to_zero */ true));
  list->Append(*ComputedStyleUtils::ValueForAnimationTimingFunction(
      CSSAnimationData::InitialTimingFunction()));
  list->Append(*ComputedStyleUtils::ValueForAnimationDelayStart(
      CSSAnimationData::InitialDelayStart()));
  list->Append(*ComputedStyleUtils::ValueForAnimationIterationCount(
      CSSAnimationData::InitialIterationCount()));
  list->Append(*ComputedStyleUtils::ValueForAnimationDirection(
      CSSAnimationData::InitialDirection()));
  list->Append(*ComputedStyleUtils::ValueForAnimationFillMode(
      CSSAnimationData::InitialFillMode()));
  list->Append(*ComputedStyleUtils::ValueForAnimationPlayState(
      CSSAnimationData::InitialPlayState()));
  return list;
}

bool ParseBackgroundOrMaskPosition(
    const StylePropertyShorthand& shorthand,
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    absl::optional<WebFeature> three_value_position,
    HeapVector<CSSPropertyValue, 64>& properties) {
  CSSValue* result_x = nullptr;
  CSSValue* result_y = nullptr;
  if (!css_parsing_utils::ConsumeBackgroundPosition(
          range, context, css_parsing_utils::UnitlessQuirk::kAllow,
          three_value_position, result_x, result_y) ||
      !range.AtEnd()) {
    return false;
  }
  const CSSProperty** longhands = shorthand.properties();
  DCHECK_EQ(2u, shorthand.length());
  css_parsing_utils::AddProperty(
      longhands[0]->PropertyID(), shorthand.id(), *result_x, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      longhands[1]->PropertyID(), shorthand.id(), *result_y, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

}  // namespace

bool Animation::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return ParseAnimationShorthand(animationShorthand(), important, range,
                                 context, local_context, properties);
}

const CSSValue* Animation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSValueFromComputedAnimation(animationShorthand(),
                                       style.Animations());
}

bool AlternativeAnimationWithTimeline::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return ParseAnimationShorthand(alternativeAnimationWithTimelineShorthand(),
                                 important, range, context, local_context,
                                 properties);
}

const CSSValue*
AlternativeAnimationWithTimeline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSValueFromComputedAnimation(
      alternativeAnimationWithTimelineShorthand(), style.Animations());
}

bool AlternativeAnimationWithDelayStartEnd::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return ParseAnimationShorthand(
      alternativeAnimationWithDelayStartEndShorthand(), important, range,
      context, local_context, properties);
}

const CSSValue*
AlternativeAnimationWithDelayStartEnd::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return CSSValueFromComputedAnimation(
      alternativeAnimationWithDelayStartEndShorthand(), style.Animations());
}

namespace {

// Consume a single <animation-delay-start> and a single
// <animation-delay-end>, and append the result to `start_list` and
// `end_list` respectively.
bool ConsumeAnimationDelayItemInto(CSSParserTokenRange& range,
                                   const CSSParserContext& context,
                                   CSSValueList* start_list,
                                   CSSValueList* end_list) {
  using css_parsing_utils::ConsumeAnimationDelay;

  const CSSValue* start_delay = ConsumeAnimationDelay(range, context);
  const CSSValue* end_delay = ConsumeAnimationDelay(range, context);

  if (!start_delay) {
    return false;
  }

  // If the <animation-delay-end> value is omitted, it is set to zero.
  //
  // https://drafts.csswg.org/scroll-animations-1/#propdef-animation-delay
  if (!end_delay) {
    end_delay = CSSNumericLiteralValue::Create(
        0, CSSPrimitiveValue::UnitType::kSeconds);
  }

  DCHECK(start_delay);
  DCHECK(end_delay);

  start_list->Append(*start_delay);
  end_list->Append(*end_delay);

  return true;
}

// Consume a single <animation-range-start> and a single
// <animation-range-end>, and append the result to `start_list` and
// `end_list` respectively.
bool ConsumeAnimationRangeItemInto(CSSParserTokenRange& range,
                                   const CSSParserContext& context,
                                   CSSValueList* start_list,
                                   CSSValueList* end_list) {
  using css_parsing_utils::ConsumeAnimationRange;
  using css_parsing_utils::ConsumeTimelineRangeName;

  const CSSValue* start_range =
      ConsumeAnimationRange(range, context, /* default_offset_percent */ 0.0);
  const CSSValue* end_range =
      ConsumeAnimationRange(range, context, /* default_offset_percent */ 100.0);

  // The form 'name X' must expand to 'name X name 100%'.
  //
  // https://github.com/w3c/csswg-drafts/issues/8438
  if (start_range && start_range->IsValueList() && !end_range) {
    CSSValueList* implied_end = CSSValueList::CreateSpaceSeparated();
    const CSSValue& name = To<CSSValueList>(start_range)->First();
    if (name.IsIdentifierValue()) {
      implied_end->Append(name);
      end_range = implied_end;
    }
  }

  if (!start_range) {
    return false;
  }
  if (!end_range) {
    end_range = CSSIdentifierValue::Create(CSSValueID::kNormal);
  }

  DCHECK(start_range);
  DCHECK(end_range);

  start_list->Append(*start_range);
  end_list->Append(*end_range);

  return true;
}

}  // namespace

bool AlternativeAnimationDelay::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  DCHECK(RuntimeEnabledFeatures::CSSAnimationDelayStartEndEnabled());

  using css_parsing_utils::AddProperty;
  using css_parsing_utils::ConsumeCommaIncludingWhitespace;
  using css_parsing_utils::IsImplicitProperty;

  const StylePropertyShorthand shorthand = alternativeAnimationDelayShorthand();
  DCHECK_EQ(2u, shorthand.length());
  DCHECK_EQ(&GetCSSPropertyAnimationDelayStart(), shorthand.properties()[0]);
  DCHECK_EQ(&GetCSSPropertyAnimationDelayEnd(), shorthand.properties()[1]);

  CSSValueList* start_list = CSSValueList::CreateCommaSeparated();
  CSSValueList* end_list = CSSValueList::CreateCommaSeparated();

  do {
    if (!ConsumeAnimationDelayItemInto(range, context, start_list, end_list)) {
      return false;
    }
  } while (ConsumeCommaIncludingWhitespace(range));

  DCHECK(start_list->length());
  DCHECK(end_list->length());
  DCHECK_EQ(start_list->length(), end_list->length());

  AddProperty(CSSPropertyID::kAnimationDelayStart,
              CSSPropertyID::kAlternativeAnimationDelay, *start_list, important,
              IsImplicitProperty::kNotImplicit, properties);
  AddProperty(CSSPropertyID::kAnimationDelayEnd,
              CSSPropertyID::kAlternativeAnimationDelay, *end_list, important,
              IsImplicitProperty::kNotImplicit, properties);

  return range.AtEnd();
}

const CSSValue* AlternativeAnimationDelay::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const Vector<Timing::Delay>& delay_start_list =
      style.Animations()
          ? style.Animations()->DelayStartList()
          : Vector<Timing::Delay>{CSSAnimationData::InitialDelayStart()};
  const Vector<Timing::Delay>& delay_end_list =
      style.Animations()
          ? style.Animations()->DelayEndList()
          : Vector<Timing::Delay>{CSSAnimationData::InitialDelayEnd()};

  if (delay_start_list.size() != delay_end_list.size()) {
    return nullptr;
  }

  auto* outer_list = CSSValueList::CreateCommaSeparated();

  for (wtf_size_t i = 0; i < delay_start_list.size(); ++i) {
    const Timing::Delay& end = delay_end_list[i];

    auto* inner_list = CSSValueList::CreateSpaceSeparated();
    inner_list->Append(
        *ComputedStyleUtils::ValueForAnimationDelayStart(delay_start_list[i]));
    if (end != CSSTimingData::InitialDelayEnd()) {
      inner_list->Append(
          *ComputedStyleUtils::ValueForAnimationDelayEnd(delay_end_list[i]));
    }
    outer_list->Append(*inner_list);
  }

  return outer_list;
}

bool AnimationRange::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  DCHECK(RuntimeEnabledFeatures::ScrollTimelineEnabled());

  using css_parsing_utils::AddProperty;
  using css_parsing_utils::ConsumeCommaIncludingWhitespace;
  using css_parsing_utils::IsImplicitProperty;

  const StylePropertyShorthand shorthand = animationRangeShorthand();
  DCHECK_EQ(2u, shorthand.length());
  DCHECK_EQ(&GetCSSPropertyAnimationRangeStart(), shorthand.properties()[0]);
  DCHECK_EQ(&GetCSSPropertyAnimationRangeEnd(), shorthand.properties()[1]);

  CSSValueList* start_list = CSSValueList::CreateCommaSeparated();
  CSSValueList* end_list = CSSValueList::CreateCommaSeparated();

  do {
    if (!ConsumeAnimationRangeItemInto(range, context, start_list, end_list)) {
      return false;
    }
  } while (ConsumeCommaIncludingWhitespace(range));

  DCHECK(start_list->length());
  DCHECK(end_list->length());
  DCHECK_EQ(start_list->length(), end_list->length());

  AddProperty(CSSPropertyID::kAnimationRangeStart,
              CSSPropertyID::kAnimationRange, *start_list, important,
              IsImplicitProperty::kNotImplicit, properties);
  AddProperty(CSSPropertyID::kAnimationRangeEnd, CSSPropertyID::kAnimationRange,
              *end_list, important, IsImplicitProperty::kNotImplicit,
              properties);

  return range.AtEnd();
}

const CSSValue* AnimationRange::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const Vector<absl::optional<TimelineOffset>>& range_start_list =
      style.Animations() ? style.Animations()->RangeStartList()
                         : Vector<absl::optional<TimelineOffset>>{
                               CSSAnimationData::InitialRangeStart()};
  const Vector<absl::optional<TimelineOffset>>& range_end_list =
      style.Animations() ? style.Animations()->RangeEndList()
                         : Vector<absl::optional<TimelineOffset>>{
                               CSSAnimationData::InitialRangeEnd()};

  if (range_start_list.size() != range_end_list.size()) {
    return nullptr;
  }

  TimelineOffset default_start(TimelineOffset::NamedRange::kNone,
                               Length::Percent(0));
  TimelineOffset default_end(TimelineOffset::NamedRange::kNone,
                             Length::Percent(100));

  auto* outer_list = CSSValueList::CreateCommaSeparated();

  for (wtf_size_t i = 0; i < range_start_list.size(); ++i) {
    const absl::optional<TimelineOffset>& start = range_start_list[i];
    const absl::optional<TimelineOffset>& end = range_end_list[i];

    auto* inner_list = CSSValueList::CreateSpaceSeparated();
    inner_list->Append(
        *ComputedStyleUtils::ValueForAnimationRangeStart(start, style));

    // The form "name X name 100%" must contract to "name X".
    //
    // https://github.com/w3c/csswg-drafts/issues/8438
    TimelineOffset omittable_end(start.value_or(default_start).name,
                                 Length::Percent(100));
    if (end.value_or(default_end) != omittable_end) {
      inner_list->Append(
          *ComputedStyleUtils::ValueForAnimationRangeEnd(end, style));
    }
    outer_list->Append(*inner_list);
  }

  return outer_list;
}

bool Background::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ParseBackgroundOrMask(important, range, context,
                                                  local_context, properties);
}

const CSSValue* Background::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForBackgroundShorthand(style, layout_object,
                                                          allow_visited_style);
}

bool BackgroundPosition::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return ParseBackgroundOrMaskPosition(
      backgroundPositionShorthand(), important, range, context,
      WebFeature::kThreeValuedPositionBackground, properties);
}

const CSSValue* BackgroundPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::BackgroundPositionOrWebkitMaskPosition(
      *this, style, &style.BackgroundLayers());
}

bool BorderBlockColor::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      borderBlockColorShorthand(), important, context, range, properties);
}

const CSSValue* BorderBlockColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      borderBlockColorShorthand(), style, layout_object, allow_visited_style);
}

bool BorderBlock::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  const CSSValue* width = nullptr;
  const CSSValue* style = nullptr;
  const CSSValue* color = nullptr;

  if (!css_parsing_utils::ConsumeBorderShorthand(range, context, width, style,
                                                 color)) {
    return false;
  };

  css_parsing_utils::AddExpandedPropertyForValue(
      CSSPropertyID::kBorderBlockWidth, *width, important, properties);
  css_parsing_utils::AddExpandedPropertyForValue(
      CSSPropertyID::kBorderBlockStyle, *style, important, properties);
  css_parsing_utils::AddExpandedPropertyForValue(
      CSSPropertyID::kBorderBlockColor, *color, important, properties);

  return range.AtEnd();
}

const CSSValue* BorderBlock::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const CSSValue* value_start =
      GetCSSPropertyBorderBlockStart().CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style);
  const CSSValue* value_end =
      GetCSSPropertyBorderBlockEnd().CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style);
  if (!base::ValuesEquivalent(value_start, value_end)) {
    return nullptr;
  }
  return value_start;
}

bool BorderBlockEnd::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      borderBlockEndShorthand(), important, context, range, properties);
}

bool BorderBlockStart::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      borderBlockStartShorthand(), important, context, range, properties);
}

bool BorderBlockStyle::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      borderBlockStyleShorthand(), important, context, range, properties);
}

const CSSValue* BorderBlockStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      borderBlockStyleShorthand(), style, layout_object, allow_visited_style);
}

bool BorderBlockWidth::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      borderBlockWidthShorthand(), important, context, range, properties);
}

const CSSValue* BorderBlockWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      borderBlockWidthShorthand(), style, layout_object, allow_visited_style);
}

bool BorderBottom::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      borderBottomShorthand(), important, context, range, properties);
}

const CSSValue* BorderBottom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      borderBottomShorthand(), style, layout_object, allow_visited_style);
}

bool BorderColor::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia4Longhands(
      borderColorShorthand(), important, context, range, properties);
}

const CSSValue* BorderColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForSidesShorthand(
      borderColorShorthand(), style, layout_object, allow_visited_style);
}

bool Border::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  const CSSValue* width = nullptr;
  const CSSValue* style = nullptr;
  const CSSValue* color = nullptr;

  if (!css_parsing_utils::ConsumeBorderShorthand(range, context, width, style,
                                                 color)) {
    return false;
  };

  css_parsing_utils::AddExpandedPropertyForValue(CSSPropertyID::kBorderWidth,
                                                 *width, important, properties);
  css_parsing_utils::AddExpandedPropertyForValue(CSSPropertyID::kBorderStyle,
                                                 *style, important, properties);
  css_parsing_utils::AddExpandedPropertyForValue(CSSPropertyID::kBorderColor,
                                                 *color, important, properties);
  css_parsing_utils::AddExpandedPropertyForValue(CSSPropertyID::kBorderImage,
                                                 *CSSInitialValue::Create(),
                                                 important, properties);

  return range.AtEnd();
}

const CSSValue* Border::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const CSSValue* value = GetCSSPropertyBorderTop().CSSValueFromComputedStyle(
      style, layout_object, allow_visited_style);
  static const CSSProperty* kProperties[3] = {&GetCSSPropertyBorderRight(),
                                              &GetCSSPropertyBorderBottom(),
                                              &GetCSSPropertyBorderLeft()};
  for (size_t i = 0; i < std::size(kProperties); ++i) {
    const CSSValue* value_for_side = kProperties[i]->CSSValueFromComputedStyle(
        style, layout_object, allow_visited_style);
    if (!base::ValuesEquivalent(value, value_for_side)) {
      return nullptr;
    }
  }
  return value;
}

bool BorderImage::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValue* source = nullptr;
  CSSValue* slice = nullptr;
  CSSValue* width = nullptr;
  CSSValue* outset = nullptr;
  CSSValue* repeat = nullptr;

  if (!css_parsing_utils::ConsumeBorderImageComponents(
          range, context, source, slice, width, outset, repeat,
          css_parsing_utils::DefaultFill::kNoFill)) {
    return false;
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kBorderImageSource, CSSPropertyID::kBorderImage,
      source
          ? *source
          : *To<Longhand>(&GetCSSPropertyBorderImageSource())->InitialValue(),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kBorderImageSlice, CSSPropertyID::kBorderImage,
      slice ? *slice
            : *To<Longhand>(&GetCSSPropertyBorderImageSlice())->InitialValue(),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kBorderImageWidth, CSSPropertyID::kBorderImage,
      width ? *width
            : *To<Longhand>(&GetCSSPropertyBorderImageWidth())->InitialValue(),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kBorderImageOutset, CSSPropertyID::kBorderImage,
      outset
          ? *outset
          : *To<Longhand>(&GetCSSPropertyBorderImageOutset())->InitialValue(),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kBorderImageRepeat, CSSPropertyID::kBorderImage,
      repeat
          ? *repeat
          : *To<Longhand>(&GetCSSPropertyBorderImageRepeat())->InitialValue(),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);

  return true;
}

const CSSValue* BorderImage::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImage(style.BorderImage(), style,
                                                    allow_visited_style);
}

bool BorderInlineColor::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      borderInlineColorShorthand(), important, context, range, properties);
}

const CSSValue* BorderInlineColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      borderInlineColorShorthand(), style, layout_object, allow_visited_style);
}

bool BorderInline::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  const CSSValue* width = nullptr;
  const CSSValue* style = nullptr;
  const CSSValue* color = nullptr;

  if (!css_parsing_utils::ConsumeBorderShorthand(range, context, width, style,
                                                 color)) {
    return false;
  };

  css_parsing_utils::AddExpandedPropertyForValue(
      CSSPropertyID::kBorderInlineWidth, *width, important, properties);
  css_parsing_utils::AddExpandedPropertyForValue(
      CSSPropertyID::kBorderInlineStyle, *style, important, properties);
  css_parsing_utils::AddExpandedPropertyForValue(
      CSSPropertyID::kBorderInlineColor, *color, important, properties);

  return range.AtEnd();
}

const CSSValue* BorderInline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const CSSValue* value_start =
      GetCSSPropertyBorderInlineStart().CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style);
  const CSSValue* value_end =
      GetCSSPropertyBorderInlineEnd().CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style);
  if (!base::ValuesEquivalent(value_start, value_end)) {
    return nullptr;
  }
  return value_start;
}

bool BorderInlineEnd::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      borderInlineEndShorthand(), important, context, range, properties);
}

bool BorderInlineStart::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      borderInlineStartShorthand(), important, context, range, properties);
}

bool BorderInlineStyle::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      borderInlineStyleShorthand(), important, context, range, properties);
}

const CSSValue* BorderInlineStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      borderInlineStyleShorthand(), style, layout_object, allow_visited_style);
}

bool BorderInlineWidth::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      borderInlineWidthShorthand(), important, context, range, properties);
}

const CSSValue* BorderInlineWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      borderInlineWidthShorthand(), style, layout_object, allow_visited_style);
}

bool BorderLeft::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      borderLeftShorthand(), important, context, range, properties);
}

const CSSValue* BorderLeft::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      borderLeftShorthand(), style, layout_object, allow_visited_style);
}

bool BorderRadius::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValue* horizontal_radii[4] = {nullptr};
  CSSValue* vertical_radii[4] = {nullptr};

  if (!css_parsing_utils::ConsumeRadii(horizontal_radii, vertical_radii, range,
                                       context,
                                       local_context.UseAliasParsing())) {
    return false;
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kBorderTopLeftRadius, CSSPropertyID::kBorderRadius,
      *MakeGarbageCollected<CSSValuePair>(horizontal_radii[0],
                                          vertical_radii[0],
                                          CSSValuePair::kDropIdenticalValues),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kBorderTopRightRadius, CSSPropertyID::kBorderRadius,
      *MakeGarbageCollected<CSSValuePair>(horizontal_radii[1],
                                          vertical_radii[1],
                                          CSSValuePair::kDropIdenticalValues),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kBorderBottomRightRadius, CSSPropertyID::kBorderRadius,
      *MakeGarbageCollected<CSSValuePair>(horizontal_radii[2],
                                          vertical_radii[2],
                                          CSSValuePair::kDropIdenticalValues),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kBorderBottomLeftRadius, CSSPropertyID::kBorderRadius,
      *MakeGarbageCollected<CSSValuePair>(horizontal_radii[3],
                                          vertical_radii[3],
                                          CSSValuePair::kDropIdenticalValues),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  return true;
}

const CSSValue* BorderRadius::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForBorderRadiusShorthand(style);
}

bool BorderRight::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      borderRightShorthand(), important, context, range, properties);
}

const CSSValue* BorderRight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      borderRightShorthand(), style, layout_object, allow_visited_style);
}

bool BorderSpacing::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValue* horizontal_spacing =
      ConsumeLength(range, context, CSSPrimitiveValue::ValueRange::kNonNegative,
                    css_parsing_utils::UnitlessQuirk::kAllow);
  if (!horizontal_spacing) {
    return false;
  }
  CSSValue* vertical_spacing = horizontal_spacing;
  if (!range.AtEnd()) {
    vertical_spacing = ConsumeLength(
        range, context, CSSPrimitiveValue::ValueRange::kNonNegative,
        css_parsing_utils::UnitlessQuirk::kAllow);
  }
  if (!vertical_spacing || !range.AtEnd()) {
    return false;
  }
  css_parsing_utils::AddProperty(
      CSSPropertyID::kWebkitBorderHorizontalSpacing,
      CSSPropertyID::kBorderSpacing, *horizontal_spacing, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kWebkitBorderVerticalSpacing,
      CSSPropertyID::kBorderSpacing, *vertical_spacing, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* BorderSpacing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ZoomAdjustedPixelValue(style.HorizontalBorderSpacing(), style));
  list->Append(*ZoomAdjustedPixelValue(style.VerticalBorderSpacing(), style));
  return list;
}

bool BorderStyle::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia4Longhands(
      borderStyleShorthand(), important, context, range, properties);
}

const CSSValue* BorderStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForSidesShorthand(
      borderStyleShorthand(), style, layout_object, allow_visited_style);
}

bool BorderTop::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      borderTopShorthand(), important, context, range, properties);
}

const CSSValue* BorderTop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      borderTopShorthand(), style, layout_object, allow_visited_style);
}

bool BorderWidth::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia4Longhands(
      borderWidthShorthand(), important, context, range, properties);
}

const CSSValue* BorderWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForSidesShorthand(
      borderWidthShorthand(), style, layout_object, allow_visited_style);
}

bool ColumnRule::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      columnRuleShorthand(), important, context, range, properties);
}

const CSSValue* ColumnRule::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      columnRuleShorthand(), style, layout_object, allow_visited_style);
}

bool Columns::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValue* column_width = nullptr;
  CSSValue* column_count = nullptr;
  if (!css_parsing_utils::ConsumeColumnWidthOrCount(
          range, context, column_width, column_count)) {
    return false;
  }
  css_parsing_utils::ConsumeColumnWidthOrCount(range, context, column_width,
                                               column_count);
  if (!range.AtEnd()) {
    return false;
  }
  if (!column_width) {
    column_width = CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  if (!column_count) {
    column_count = CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  css_parsing_utils::AddProperty(
      CSSPropertyID::kColumnWidth, CSSPropertyID::kInvalid, *column_width,
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kColumnCount, CSSPropertyID::kInvalid, *column_count,
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  return true;
}

const CSSValue* Columns::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      columnsShorthand(), style, layout_object, allow_visited_style);
}

bool ContainIntrinsicSize::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      containIntrinsicSizeShorthand(), important, context, range, properties);
}

const CSSValue* ContainIntrinsicSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const StylePropertyShorthand& shorthand = containIntrinsicSizeShorthand();
  const auto& width = style.ContainIntrinsicWidth();
  const auto& height = style.ContainIntrinsicHeight();
  if (width != height) {
    return ComputedStyleUtils::ValuesForShorthandProperty(
        shorthand, style, layout_object, allow_visited_style);
  }
  return shorthand.properties()[0]->CSSValueFromComputedStyle(
      style, layout_object, allow_visited_style);
}

bool Container::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  const CSSValue* name =
      css_parsing_utils::ConsumeContainerName(range, context);
  if (!name) {
    return false;
  }

  const CSSValue* type = CSSIdentifierValue::Create(CSSValueID::kNormal);
  if (css_parsing_utils::ConsumeSlashIncludingWhitespace(range)) {
    if (!(type = css_parsing_utils::ConsumeContainerType(range))) {
      return false;
    }
  }

  if (!range.AtEnd()) {
    return false;
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kContainerName, CSSPropertyID::kContainer, *name,
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);

  css_parsing_utils::AddProperty(
      CSSPropertyID::kContainerType, CSSPropertyID::kContainer, *type,
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);

  return true;
}

const CSSValue* Container::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForContainerShorthand(style, layout_object,
                                                         allow_visited_style);
}

bool Flex::ParseShorthand(bool important,
                          CSSParserTokenRange& range,
                          const CSSParserContext& context,
                          const CSSParserLocalContext&,
                          HeapVector<CSSPropertyValue, 64>& properties) const {
  static const double kUnsetValue = -1;
  double flex_grow = kUnsetValue;
  double flex_shrink = kUnsetValue;
  CSSValue* flex_basis = nullptr;

  if (range.Peek().Id() == CSSValueID::kNone) {
    flex_grow = 0;
    flex_shrink = 0;
    flex_basis = CSSIdentifierValue::Create(CSSValueID::kAuto);
    range.ConsumeIncludingWhitespace();
  } else {
    unsigned index = 0;
    while (!range.AtEnd() && index++ < 3) {
      double num;
      if (css_parsing_utils::ConsumeNumberRaw(range, context, num)) {
        if (num < 0) {
          return false;
        }
        if (flex_grow == kUnsetValue) {
          flex_grow = num;
        } else if (flex_shrink == kUnsetValue) {
          flex_shrink = num;
        } else if (!num) {
          // flex only allows a basis of 0 (sans units) if
          // flex-grow and flex-shrink values have already been
          // set.
          flex_basis = CSSNumericLiteralValue::Create(
              0, CSSPrimitiveValue::UnitType::kPixels);
        } else {
          return false;
        }
      } else if (!flex_basis) {
        if (css_parsing_utils::IdentMatches<
                CSSValueID::kAuto, CSSValueID::kContent,
                CSSValueID::kMinContent, CSSValueID::kMaxContent,
                CSSValueID::kFitContent>(range.Peek().Id())) {
          flex_basis = css_parsing_utils::ConsumeIdent(range);
        }
        if (!flex_basis) {
          flex_basis = css_parsing_utils::ConsumeLengthOrPercent(
              range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
        }
        if (index == 2 && !range.AtEnd()) {
          return false;
        }
      }
    }
    if (index == 0) {
      return false;
    }
    if (flex_grow == kUnsetValue) {
      flex_grow = 1;
    }
    if (flex_shrink == kUnsetValue) {
      flex_shrink = 1;
    }
    if (!flex_basis) {
      flex_basis = CSSNumericLiteralValue::Create(
          0, CSSPrimitiveValue::UnitType::kPercentage);
    }
  }

  if (!range.AtEnd()) {
    return false;
  }
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFlexGrow, CSSPropertyID::kFlex,
      *CSSNumericLiteralValue::Create(ClampTo<float>(flex_grow),
                                      CSSPrimitiveValue::UnitType::kNumber),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFlexShrink, CSSPropertyID::kFlex,
      *CSSNumericLiteralValue::Create(ClampTo<float>(flex_shrink),
                                      CSSPrimitiveValue::UnitType::kNumber),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);

  css_parsing_utils::AddProperty(
      CSSPropertyID::kFlexBasis, CSSPropertyID::kFlex, *flex_basis, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);

  return true;
}

const CSSValue* Flex::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      flexShorthand(), style, layout_object, allow_visited_style);
}

bool FlexFlow::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      flexFlowShorthand(), important, context, range, properties,
      /* use_initial_value_function */ true);
}

const CSSValue* FlexFlow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      flexFlowShorthand(), style, layout_object, allow_visited_style);
}
namespace {

bool ConsumeSystemFont(bool important,
                       CSSParserTokenRange& range,
                       HeapVector<CSSPropertyValue, 64>& properties) {
  CSSValueID system_font_id = range.ConsumeIncludingWhitespace().Id();
  DCHECK(CSSParserFastPaths::IsValidSystemFont(system_font_id));
  if (!range.AtEnd()) {
    return false;
  }

  css_parsing_utils::AddExpandedPropertyForValue(
      CSSPropertyID::kFont,
      *cssvalue::CSSPendingSystemFontValue::Create(system_font_id), important,
      properties);
  return true;
}

bool ConsumeFont(bool important,
                 CSSParserTokenRange& range,
                 const CSSParserContext& context,
                 HeapVector<CSSPropertyValue, 64>& properties) {
  // Optional font-style, font-variant, font-stretch and font-weight.
  // Each may be normal.
  CSSValue* font_style = nullptr;
  CSSIdentifierValue* font_variant_caps = nullptr;
  CSSValue* font_weight = nullptr;
  CSSValue* font_stretch = nullptr;
  const int kNumReorderableFontProperties = 4;
  for (int i = 0; i < kNumReorderableFontProperties && !range.AtEnd(); ++i) {
    CSSValueID id = range.Peek().Id();
    if (id == CSSValueID::kNormal) {
      css_parsing_utils::ConsumeIdent(range);
      continue;
    }
    if (!font_style &&
        (id == CSSValueID::kItalic || id == CSSValueID::kOblique)) {
      font_style = css_parsing_utils::ConsumeFontStyle(range, context);
      if (!font_style) {
        return false;
      }
      continue;
    }
    if (!font_variant_caps && id == CSSValueID::kSmallCaps) {
      // Font variant in the shorthand is particular, it only accepts normal or
      // small-caps.
      // See https://drafts.csswg.org/css-fonts/#propdef-font
      font_variant_caps = css_parsing_utils::ConsumeFontVariantCSS21(range);
      if (font_variant_caps) {
        continue;
      }
    }
    if (!font_weight) {
      font_weight = css_parsing_utils::ConsumeFontWeight(range, context);
      if (font_weight) {
        continue;
      }
    }
    // Stretch in the font shorthand can only take the CSS Fonts Level 3
    // keywords, not arbitrary values, compare
    // https://drafts.csswg.org/css-fonts-4/#font-prop
    // Bail out if the last possible property of the set in this loop could not
    // be parsed, this closes the first block of optional values of the font
    // shorthand, compare: [ [ <‘font-style’> || <font-variant-css21> ||
    // <‘font-weight’> || <font-stretch-css3> ]?
    if (font_stretch ||
        !(font_stretch = css_parsing_utils::ConsumeFontStretchKeywordOnly(
              range, context))) {
      break;
    }
  }

  if (range.AtEnd()) {
    return false;
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontStyle, CSSPropertyID::kFont,
      font_style ? *font_style
                 : *CSSIdentifierValue::Create(CSSValueID::kNormal),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontVariantCaps, CSSPropertyID::kFont,
      font_variant_caps ? *font_variant_caps
                        : *CSSIdentifierValue::Create(CSSValueID::kNormal),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);

  // All subproperties of the font, i.e. font-size-adjust, font-kerning, all
  // subproperties of font-variant, font-feature-settings,
  // font-language-override, font-optical-sizing and font-variation-settings
  // property should be reset to their initial values, compare
  // https://drafts.csswg.org/css-fonts-4/#font-prop
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontVariantLigatures, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontVariantNumeric, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontVariantEastAsian, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontVariantAlternates, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  if (RuntimeEnabledFeatures::CSSFontSizeAdjustEnabled()) {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kFontSizeAdjust, CSSPropertyID::kFont,
        *CSSIdentifierValue::Create(CSSValueID::kNone), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  }
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontKerning, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kAuto), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontOpticalSizing, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kAuto), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontFeatureSettings, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontVariationSettings, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  if (RuntimeEnabledFeatures::FontVariantPositionEnabled()) {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kFontVariantPosition, CSSPropertyID::kFont,
        *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontWeight, CSSPropertyID::kFont,
      font_weight ? *font_weight
                  : *CSSIdentifierValue::Create(CSSValueID::kNormal),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontStretch, CSSPropertyID::kFont,
      font_stretch ? *font_stretch
                   : *CSSIdentifierValue::Create(CSSValueID::kNormal),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);

  // Now a font size _must_ come.
  CSSValue* font_size = css_parsing_utils::ConsumeFontSize(range, context);
  if (!font_size || range.AtEnd()) {
    return false;
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontSize, CSSPropertyID::kFont, *font_size, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);

  if (css_parsing_utils::ConsumeSlashIncludingWhitespace(range)) {
    CSSValue* line_height =
        css_parsing_utils::ConsumeLineHeight(range, context);
    if (!line_height) {
      return false;
    }
    css_parsing_utils::AddProperty(
        CSSPropertyID::kLineHeight, CSSPropertyID::kFont, *line_height,
        important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
        properties);
  } else {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kLineHeight, CSSPropertyID::kFont,
        *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  }

  // Font family must come now.
  CSSValue* parsed_family_value = css_parsing_utils::ConsumeFontFamily(range);
  if (!parsed_family_value) {
    return false;
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontFamily, CSSPropertyID::kFont, *parsed_family_value,
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);

  return range.AtEnd();
}

}  // namespace

bool Font::ParseShorthand(bool important,
                          CSSParserTokenRange& range,
                          const CSSParserContext& context,
                          const CSSParserLocalContext&,
                          HeapVector<CSSPropertyValue, 64>& properties) const {
  const CSSParserToken& token = range.Peek();
  if (CSSParserFastPaths::IsValidSystemFont(token.Id())) {
    return ConsumeSystemFont(important, range, properties);
  }
  return ConsumeFont(important, range, context, properties);
}

const CSSValue* Font::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFont(style);
}

bool FontVariant::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  if (css_parsing_utils::IdentMatches<CSSValueID::kNormal, CSSValueID::kNone>(
          range.Peek().Id())) {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kFontVariantLigatures, CSSPropertyID::kFontVariant,
        *css_parsing_utils::ConsumeIdent(range), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
    css_parsing_utils::AddProperty(
        CSSPropertyID::kFontVariantCaps, CSSPropertyID::kFontVariant,
        *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
    css_parsing_utils::AddProperty(
        CSSPropertyID::kFontVariantNumeric, CSSPropertyID::kFontVariant,
        *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
    css_parsing_utils::AddProperty(
        CSSPropertyID::kFontVariantEastAsian, CSSPropertyID::kFontVariant,
        *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
    css_parsing_utils::AddProperty(
        CSSPropertyID::kFontVariantAlternates, CSSPropertyID::kFontVariant,
        *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
    if (RuntimeEnabledFeatures::FontVariantPositionEnabled()) {
      css_parsing_utils::AddProperty(
          CSSPropertyID::kFontVariantPosition, CSSPropertyID::kFontVariant,
          *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
          css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
    }
    return range.AtEnd();
  }

  CSSIdentifierValue* caps_value = nullptr;
  FontVariantLigaturesParser ligatures_parser;
  FontVariantNumericParser numeric_parser;
  FontVariantEastAsianParser east_asian_parser;
  FontVariantAlternatesParser alternates_parser;
  CSSIdentifierValue* position_value = nullptr;
  do {
    FontVariantLigaturesParser::ParseResult ligatures_parse_result =
        ligatures_parser.ConsumeLigature(range);
    FontVariantNumericParser::ParseResult numeric_parse_result =
        numeric_parser.ConsumeNumeric(range);
    FontVariantEastAsianParser::ParseResult east_asian_parse_result =
        east_asian_parser.ConsumeEastAsian(range);
    FontVariantAlternatesParser::ParseResult alternates_parse_result =
        alternates_parser.ConsumeAlternates(range, context);
    if (ligatures_parse_result ==
            FontVariantLigaturesParser::ParseResult::kConsumedValue ||
        numeric_parse_result ==
            FontVariantNumericParser::ParseResult::kConsumedValue ||
        east_asian_parse_result ==
            FontVariantEastAsianParser::ParseResult::kConsumedValue ||
        alternates_parse_result ==
            FontVariantAlternatesParser::ParseResult::kConsumedValue) {
      continue;
    }

    if (ligatures_parse_result ==
            FontVariantLigaturesParser::ParseResult::kDisallowedValue ||
        numeric_parse_result ==
            FontVariantNumericParser::ParseResult::kDisallowedValue ||
        east_asian_parse_result ==
            FontVariantEastAsianParser::ParseResult::kDisallowedValue ||
        alternates_parse_result ==
            FontVariantAlternatesParser::ParseResult::kDisallowedValue) {
      return false;
    }

    CSSValueID id = range.Peek().Id();
    switch (id) {
      case CSSValueID::kSmallCaps:
      case CSSValueID::kAllSmallCaps:
      case CSSValueID::kPetiteCaps:
      case CSSValueID::kAllPetiteCaps:
      case CSSValueID::kUnicase:
      case CSSValueID::kTitlingCaps:
        // Only one caps value permitted in font-variant grammar.
        if (caps_value) {
          return false;
        }
        caps_value = css_parsing_utils::ConsumeIdent(range);
        break;
      case CSSValueID::kSub:
      case CSSValueID::kSuper:
        // Only one position value permitted in font-variant grammar.
        if (position_value) {
          return false;
        }
        position_value = css_parsing_utils::ConsumeIdent(range);
        break;
      default:
        return false;
    }
  } while (!range.AtEnd());

  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontVariantLigatures, CSSPropertyID::kFontVariant,
      *ligatures_parser.FinalizeValue(), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontVariantNumeric, CSSPropertyID::kFontVariant,
      *numeric_parser.FinalizeValue(), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontVariantEastAsian, CSSPropertyID::kFontVariant,
      *east_asian_parser.FinalizeValue(), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontVariantCaps, CSSPropertyID::kFontVariant,
      caps_value ? *caps_value
                 : *CSSIdentifierValue::Create(CSSValueID::kNormal),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontVariantAlternates, CSSPropertyID::kFontVariant,
      *alternates_parser.FinalizeValue(), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  if (RuntimeEnabledFeatures::FontVariantPositionEnabled()) {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kFontVariantPosition, CSSPropertyID::kFontVariant,
        position_value ? *position_value
                       : *CSSIdentifierValue::Create(CSSValueID::kNormal),
        important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
        properties);
  }
  return true;
}

const CSSValue* FontVariant::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForFontVariantProperty(style, layout_object,
                                                          allow_visited_style);
}

bool FontSynthesis::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  if (range.Peek().Id() == CSSValueID::kNone) {
    range.ConsumeIncludingWhitespace();
    css_parsing_utils::AddProperty(
        CSSPropertyID::kFontSynthesisWeight, CSSPropertyID::kFontSynthesis,
        *CSSIdentifierValue::Create(CSSValueID::kNone), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
    css_parsing_utils::AddProperty(
        CSSPropertyID::kFontSynthesisStyle, CSSPropertyID::kFontSynthesis,
        *CSSIdentifierValue::Create(CSSValueID::kNone), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
    css_parsing_utils::AddProperty(
        CSSPropertyID::kFontSynthesisSmallCaps, CSSPropertyID::kFontSynthesis,
        *CSSIdentifierValue::Create(CSSValueID::kNone), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
    return range.AtEnd();
  }

  CSSValue* font_synthesis_weight = nullptr;
  CSSValue* font_synthesis_style = nullptr;
  CSSValue* font_synthesis_small_caps = nullptr;
  do {
    CSSValueID id = range.ConsumeIncludingWhitespace().Id();
    switch (id) {
      case CSSValueID::kWeight:
        if (font_synthesis_weight) {
          return false;
        }
        font_synthesis_weight = CSSIdentifierValue::Create(CSSValueID::kAuto);
        break;
      case CSSValueID::kStyle:
        if (font_synthesis_style) {
          return false;
        }
        font_synthesis_style = CSSIdentifierValue::Create(CSSValueID::kAuto);
        break;
      case CSSValueID::kSmallCaps:
        if (font_synthesis_small_caps) {
          return false;
        }
        font_synthesis_small_caps =
            CSSIdentifierValue::Create(CSSValueID::kAuto);
        break;
      default:
        return false;
    }
  } while (!range.AtEnd());

  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontSynthesisWeight, CSSPropertyID::kFontSynthesis,
      font_synthesis_weight ? *font_synthesis_weight
                            : *CSSIdentifierValue::Create(CSSValueID::kNone),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontSynthesisStyle, CSSPropertyID::kFontSynthesis,
      font_synthesis_style ? *font_synthesis_style
                           : *CSSIdentifierValue::Create(CSSValueID::kNone),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kFontSynthesisSmallCaps, CSSPropertyID::kFontSynthesis,
      font_synthesis_small_caps
          ? *font_synthesis_small_caps
          : *CSSIdentifierValue::Create(CSSValueID::kNone),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  return true;
}

const CSSValue* FontSynthesis::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForFontSynthesisProperty(
      style, layout_object, allow_visited_style);
}

bool Gap::ParseShorthand(bool important,
                         CSSParserTokenRange& range,
                         const CSSParserContext& context,
                         const CSSParserLocalContext&,
                         HeapVector<CSSPropertyValue, 64>& properties) const {
  DCHECK_EQ(shorthandForProperty(CSSPropertyID::kGap).length(), 2u);
  CSSValue* row_gap = css_parsing_utils::ConsumeGapLength(range, context);
  CSSValue* column_gap = css_parsing_utils::ConsumeGapLength(range, context);
  if (!row_gap || !range.AtEnd()) {
    return false;
  }
  if (!column_gap) {
    column_gap = row_gap;
  }
  css_parsing_utils::AddProperty(
      CSSPropertyID::kRowGap, CSSPropertyID::kGap, *row_gap, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kColumnGap, CSSPropertyID::kGap, *column_gap, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* Gap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForGapShorthand(
      gapShorthand(), style, layout_object, allow_visited_style);
}

bool GridArea::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  DCHECK_EQ(gridAreaShorthand().length(), 4u);

  CSSValue* row_start_value =
      css_parsing_utils::ConsumeGridLine(range, context);
  if (!row_start_value) {
    return false;
  }
  CSSValue* column_start_value = nullptr;
  CSSValue* row_end_value = nullptr;
  CSSValue* column_end_value = nullptr;
  if (css_parsing_utils::ConsumeSlashIncludingWhitespace(range)) {
    column_start_value = css_parsing_utils::ConsumeGridLine(range, context);
    if (!column_start_value) {
      return false;
    }
    if (css_parsing_utils::ConsumeSlashIncludingWhitespace(range)) {
      row_end_value = css_parsing_utils::ConsumeGridLine(range, context);
      if (!row_end_value) {
        return false;
      }
      if (css_parsing_utils::ConsumeSlashIncludingWhitespace(range)) {
        column_end_value = css_parsing_utils::ConsumeGridLine(range, context);
        if (!column_end_value) {
          return false;
        }
      }
    }
  }
  if (!range.AtEnd()) {
    return false;
  }
  if (!column_start_value) {
    column_start_value = row_start_value->IsCustomIdentValue()
                             ? row_start_value
                             : CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  if (!row_end_value) {
    row_end_value = row_start_value->IsCustomIdentValue()
                        ? row_start_value
                        : CSSIdentifierValue::Create(CSSValueID::kAuto);
  }
  if (!column_end_value) {
    column_end_value = column_start_value->IsCustomIdentValue()
                           ? column_start_value
                           : CSSIdentifierValue::Create(CSSValueID::kAuto);
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kGridRowStart, CSSPropertyID::kGridArea, *row_start_value,
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kGridColumnStart, CSSPropertyID::kGridArea,
      *column_start_value, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kGridRowEnd, CSSPropertyID::kGridArea, *row_end_value,
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kGridColumnEnd, CSSPropertyID::kGridArea,
      *column_end_value, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* GridArea::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForGridAreaShorthand(
      gridAreaShorthand(), style, layout_object, allow_visited_style);
}

bool GridColumn::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  const StylePropertyShorthand& shorthand =
      shorthandForProperty(CSSPropertyID::kGridColumn);
  DCHECK_EQ(shorthand.length(), 2u);

  CSSValue* start_value = nullptr;
  CSSValue* end_value = nullptr;
  if (!css_parsing_utils::ConsumeGridItemPositionShorthand(
          important, range, context, start_value, end_value)) {
    return false;
  }

  css_parsing_utils::AddProperty(
      shorthand.properties()[0]->PropertyID(), CSSPropertyID::kGridColumn,
      *start_value, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      shorthand.properties()[1]->PropertyID(), CSSPropertyID::kGridColumn,
      *end_value, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);

  return true;
}

const CSSValue* GridColumn::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForGridLineShorthand(
      gridColumnShorthand(), style, layout_object, allow_visited_style);
}

bool GridColumnGap::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValue* gap_length = css_parsing_utils::ConsumeGapLength(range, context);
  if (!gap_length || !range.AtEnd()) {
    return false;
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kColumnGap, CSSPropertyID::kGridColumnGap, *gap_length,
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  return true;
}

const CSSValue* GridColumnGap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      gridColumnGapShorthand(), style, layout_object, allow_visited_style);
}

namespace {

CSSValueList* ConsumeImplicitAutoFlow(
    CSSParserTokenRange& range,
    const CSSIdentifierValue& flow_direction) {
  // [ auto-flow && dense? ]
  CSSValue* dense_algorithm = nullptr;
  if (css_parsing_utils::ConsumeIdent<CSSValueID::kAutoFlow>(range)) {
    dense_algorithm =
        css_parsing_utils::ConsumeIdent<CSSValueID::kDense>(range);
  } else {
    dense_algorithm =
        css_parsing_utils::ConsumeIdent<CSSValueID::kDense>(range);
    if (!dense_algorithm) {
      return nullptr;
    }
    if (!css_parsing_utils::ConsumeIdent<CSSValueID::kAutoFlow>(range)) {
      return nullptr;
    }
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (flow_direction.GetValueID() == CSSValueID::kColumn || !dense_algorithm) {
    list->Append(flow_direction);
  }
  if (dense_algorithm) {
    list->Append(*dense_algorithm);
  }
  return list;
}

}  // namespace

bool Grid::ParseShorthand(bool important,
                          CSSParserTokenRange& range,
                          const CSSParserContext& context,
                          const CSSParserLocalContext&,
                          HeapVector<CSSPropertyValue, 64>& properties) const {
  DCHECK_EQ(shorthandForProperty(CSSPropertyID::kGrid).length(), 6u);

  CSSParserTokenRange range_copy = range;

  const CSSValue* template_rows = nullptr;
  const CSSValue* template_columns = nullptr;
  const CSSValue* template_areas = nullptr;

  if (css_parsing_utils::ConsumeGridTemplateShorthand(
          important, range, context, template_rows, template_columns,
          template_areas)) {
    DCHECK(template_rows);
    DCHECK(template_columns);
    DCHECK(template_areas);

    css_parsing_utils::AddProperty(
        CSSPropertyID::kGridTemplateRows, CSSPropertyID::kGrid, *template_rows,
        important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
        properties);
    css_parsing_utils::AddProperty(
        CSSPropertyID::kGridTemplateColumns, CSSPropertyID::kGrid,
        *template_columns, important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
    css_parsing_utils::AddProperty(
        CSSPropertyID::kGridTemplateAreas, CSSPropertyID::kGrid,
        *template_areas, important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);

    // It can only be specified the explicit or the implicit grid properties in
    // a single grid declaration. The sub-properties not specified are set to
    // their initial value, as normal for shorthands.
    css_parsing_utils::AddProperty(
        CSSPropertyID::kGridAutoFlow, CSSPropertyID::kGrid,
        *(To<Longhand>(GetCSSPropertyGridAutoFlow()).InitialValue()), important,
        css_parsing_utils::IsImplicitProperty::kImplicit, properties);
    css_parsing_utils::AddProperty(
        CSSPropertyID::kGridAutoColumns, CSSPropertyID::kGrid,
        *(To<Longhand>(GetCSSPropertyGridAutoColumns()).InitialValue()),
        important, css_parsing_utils::IsImplicitProperty::kImplicit,
        properties);
    css_parsing_utils::AddProperty(
        CSSPropertyID::kGridAutoRows, CSSPropertyID::kGrid,
        *(To<Longhand>(GetCSSPropertyGridAutoRows()).InitialValue()), important,
        css_parsing_utils::IsImplicitProperty::kImplicit, properties);
    return true;
  }

  range = range_copy;

  const CSSValue* auto_columns_value = nullptr;
  const CSSValue* auto_rows_value = nullptr;
  const CSSValueList* grid_auto_flow = nullptr;
  template_rows = nullptr;
  template_columns = nullptr;

  if (css_parsing_utils::IdentMatches<CSSValueID::kDense,
                                      CSSValueID::kAutoFlow>(
          range.Peek().Id())) {
    // 2- [ auto-flow && dense? ] <grid-auto-rows>? / <grid-template-columns>
    grid_auto_flow = ConsumeImplicitAutoFlow(
        range, *CSSIdentifierValue::Create(CSSValueID::kRow));
    if (!grid_auto_flow) {
      return false;
    }
    if (css_parsing_utils::ConsumeSlashIncludingWhitespace(range)) {
      auto_rows_value =
          To<Longhand>(GetCSSPropertyGridAutoRows()).InitialValue();
    } else {
      auto_rows_value = css_parsing_utils::ConsumeGridTrackList(
          range, context, css_parsing_utils::TrackListType::kGridAuto);
      if (!auto_rows_value) {
        return false;
      }
      if (!css_parsing_utils::ConsumeSlashIncludingWhitespace(range)) {
        return false;
      }
    }
    if (!(template_columns =
              css_parsing_utils::ConsumeGridTemplatesRowsOrColumns(range,
                                                                   context))) {
      return false;
    }
    template_rows =
        To<Longhand>(GetCSSPropertyGridTemplateRows()).InitialValue();
    auto_columns_value =
        To<Longhand>(GetCSSPropertyGridAutoColumns()).InitialValue();
  } else {
    // 3- <grid-template-rows> / [ auto-flow && dense? ] <grid-auto-columns>?
    template_rows =
        css_parsing_utils::ConsumeGridTemplatesRowsOrColumns(range, context);
    if (!template_rows) {
      return false;
    }
    if (!css_parsing_utils::ConsumeSlashIncludingWhitespace(range)) {
      return false;
    }
    grid_auto_flow = ConsumeImplicitAutoFlow(
        range, *CSSIdentifierValue::Create(CSSValueID::kColumn));
    if (!grid_auto_flow) {
      return false;
    }
    if (range.AtEnd()) {
      auto_columns_value =
          To<Longhand>(GetCSSPropertyGridAutoColumns()).InitialValue();
    } else {
      auto_columns_value = css_parsing_utils::ConsumeGridTrackList(
          range, context, css_parsing_utils::TrackListType::kGridAuto);
      if (!auto_columns_value) {
        return false;
      }
    }
    template_columns =
        To<Longhand>(GetCSSPropertyGridTemplateColumns()).InitialValue();
    auto_rows_value = To<Longhand>(GetCSSPropertyGridAutoRows()).InitialValue();
  }

  if (!range.AtEnd()) {
    return false;
  }

  // It can only be specified the explicit or the implicit grid properties in a
  // single grid declaration. The sub-properties not specified are set to their
  // initial value, as normal for shorthands.
  css_parsing_utils::AddProperty(
      CSSPropertyID::kGridTemplateColumns, CSSPropertyID::kGrid,
      *template_columns, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kGridTemplateRows, CSSPropertyID::kGrid, *template_rows,
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kGridTemplateAreas, CSSPropertyID::kGrid,
      *(To<Longhand>(GetCSSPropertyGridTemplateAreas()).InitialValue()),
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kGridAutoFlow, CSSPropertyID::kGrid, *grid_auto_flow,
      important, css_parsing_utils::IsImplicitProperty::kImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kGridAutoColumns, CSSPropertyID::kGrid,
      *auto_columns_value, important,
      css_parsing_utils::IsImplicitProperty::kImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kGridAutoRows, CSSPropertyID::kGrid, *auto_rows_value,
      important, css_parsing_utils::IsImplicitProperty::kImplicit, properties);
  return true;
}

bool Grid::IsLayoutDependent(const ComputedStyle* style,
                             LayoutObject* layout_object) const {
  return layout_object && layout_object->IsLayoutGrid();
}

const CSSValue* Grid::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForGridShorthand(
      gridShorthand(), style, layout_object, allow_visited_style);
}

bool GridGap::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  DCHECK_EQ(shorthandForProperty(CSSPropertyID::kGridGap).length(), 2u);
  CSSValue* row_gap = css_parsing_utils::ConsumeGapLength(range, context);
  CSSValue* column_gap = css_parsing_utils::ConsumeGapLength(range, context);
  if (!row_gap || !range.AtEnd()) {
    return false;
  }
  if (!column_gap) {
    column_gap = row_gap;
  }
  css_parsing_utils::AddProperty(
      CSSPropertyID::kRowGap, CSSPropertyID::kGap, *row_gap, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kColumnGap, CSSPropertyID::kGap, *column_gap, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* GridGap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      gridGapShorthand(), style, layout_object, allow_visited_style);
}

bool GridRow::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  const StylePropertyShorthand& shorthand =
      shorthandForProperty(CSSPropertyID::kGridRow);
  DCHECK_EQ(shorthand.length(), 2u);

  CSSValue* start_value = nullptr;
  CSSValue* end_value = nullptr;
  if (!css_parsing_utils::ConsumeGridItemPositionShorthand(
          important, range, context, start_value, end_value)) {
    return false;
  }

  css_parsing_utils::AddProperty(
      shorthand.properties()[0]->PropertyID(), CSSPropertyID::kGridRow,
      *start_value, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      shorthand.properties()[1]->PropertyID(), CSSPropertyID::kGridRow,
      *end_value, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);

  return true;
}

const CSSValue* GridRow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForGridLineShorthand(
      gridRowShorthand(), style, layout_object, allow_visited_style);
}

bool GridRowGap::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValue* gap_length = css_parsing_utils::ConsumeGapLength(range, context);
  if (!gap_length || !range.AtEnd()) {
    return false;
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kRowGap, CSSPropertyID::kGridRowGap, *gap_length,
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  return true;
}

const CSSValue* GridRowGap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      gridRowGapShorthand(), style, layout_object, allow_visited_style);
}

bool GridTemplate::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  const CSSValue* template_rows = nullptr;
  const CSSValue* template_columns = nullptr;
  const CSSValue* template_areas = nullptr;
  if (!css_parsing_utils::ConsumeGridTemplateShorthand(
          important, range, context, template_rows, template_columns,
          template_areas)) {
    return false;
  }

  DCHECK(template_rows);
  DCHECK(template_columns);
  DCHECK(template_areas);

  css_parsing_utils::AddProperty(
      CSSPropertyID::kGridTemplateRows, CSSPropertyID::kGridTemplate,
      *template_rows, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kGridTemplateColumns, CSSPropertyID::kGridTemplate,
      *template_columns, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kGridTemplateAreas, CSSPropertyID::kGridTemplate,
      *template_areas, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);

  return true;
}

bool GridTemplate::IsLayoutDependent(const ComputedStyle* style,
                                     LayoutObject* layout_object) const {
  return layout_object && layout_object->IsLayoutGrid();
}

const CSSValue* GridTemplate::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForGridTemplateShorthand(
      gridTemplateShorthand(), style, layout_object, allow_visited_style);
}

bool InsetBlock::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      insetBlockShorthand(), important, context, range, properties);
}

const CSSValue* InsetBlock::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      insetBlockShorthand(), style, layout_object, allow_visited_style);
}

bool InsetBlock::IsLayoutDependent(const ComputedStyle* style,
                                   LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

bool Inset::ParseShorthand(bool important,
                           CSSParserTokenRange& range,
                           const CSSParserContext& context,
                           const CSSParserLocalContext&,
                           HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia4Longhands(
      insetShorthand(), important, context, range, properties);
}

const CSSValue* Inset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForSidesShorthand(
      insetShorthand(), style, layout_object, allow_visited_style);
}

bool Inset::IsLayoutDependent(const ComputedStyle* style,
                              LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

bool InsetInline::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      insetInlineShorthand(), important, context, range, properties);
}

const CSSValue* InsetInline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      insetInlineShorthand(), style, layout_object, allow_visited_style);
}

bool InsetInline::IsLayoutDependent(const ComputedStyle* style,
                                    LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

bool ListStyle::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  const CSSValue* none = nullptr;
  const CSSValue* list_style_position = nullptr;
  const CSSValue* list_style_image = nullptr;
  const CSSValue* list_style_type = nullptr;
  do {
    if (!none) {
      none = css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(range);
      if (none) {
        continue;
      }
    }
    if (!list_style_position) {
      list_style_position = css_parsing_utils::ParseLonghand(
          CSSPropertyID::kListStylePosition, CSSPropertyID::kListStyle, context,
          range);
      if (list_style_position) {
        continue;
      }
    }
    if (!list_style_image) {
      list_style_image = css_parsing_utils::ParseLonghand(
          CSSPropertyID::kListStyleImage, CSSPropertyID::kListStyle, context,
          range);
      if (list_style_image) {
        continue;
      }
    }
    if (!list_style_type) {
      list_style_type = css_parsing_utils::ParseLonghand(
          CSSPropertyID::kListStyleType, CSSPropertyID::kListStyle, context,
          range);
      if (list_style_type) {
        continue;
      }
    }
    return false;
  } while (!range.AtEnd());
  if (none) {
    if (!list_style_type) {
      list_style_type = none;
    } else if (!list_style_image) {
      list_style_image = none;
    } else {
      return false;
    }
  }

  if (list_style_position) {
    AddProperty(CSSPropertyID::kListStylePosition, CSSPropertyID::kListStyle,
                *list_style_position, important,
                css_parsing_utils::IsImplicitProperty::kNotImplicit,
                properties);
  } else {
    AddProperty(CSSPropertyID::kListStylePosition, CSSPropertyID::kListStyle,
                *CSSInitialValue::Create(), important,
                css_parsing_utils::IsImplicitProperty::kNotImplicit,
                properties);
  }

  if (list_style_image) {
    AddProperty(CSSPropertyID::kListStyleImage, CSSPropertyID::kListStyle,
                *list_style_image, important,
                css_parsing_utils::IsImplicitProperty::kNotImplicit,
                properties);
  } else {
    AddProperty(CSSPropertyID::kListStyleImage, CSSPropertyID::kListStyle,
                *CSSInitialValue::Create(), important,
                css_parsing_utils::IsImplicitProperty::kNotImplicit,
                properties);
  }

  if (list_style_type) {
    AddProperty(CSSPropertyID::kListStyleType, CSSPropertyID::kListStyle,
                *list_style_type, important,
                css_parsing_utils::IsImplicitProperty::kNotImplicit,
                properties);
  } else {
    AddProperty(CSSPropertyID::kListStyleType, CSSPropertyID::kListStyle,
                *CSSInitialValue::Create(), important,
                css_parsing_utils::IsImplicitProperty::kNotImplicit,
                properties);
  }

  return true;
}

const CSSValue* ListStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      listStyleShorthand(), style, layout_object, allow_visited_style);
}

bool MarginBlock::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      marginBlockShorthand(), important, context, range, properties);
}

bool MarginBlock::IsLayoutDependent(const ComputedStyle* style,
                                    LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* MarginBlock::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      marginBlockShorthand(), style, layout_object, allow_visited_style);
}

bool Margin::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia4Longhands(
      marginShorthand(), important, context, range, properties);
}

bool Margin::IsLayoutDependent(const ComputedStyle* style,
                               LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->MarginBottom().IsFixed() ||
          !style->MarginTop().IsFixed() || !style->MarginLeft().IsFixed() ||
          !style->MarginRight().IsFixed() ||
          style->MayHavePositionFallbackList());
}

const CSSValue* Margin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForSidesShorthand(
      marginShorthand(), style, layout_object, allow_visited_style);
}

bool MarginInline::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      marginInlineShorthand(), important, context, range, properties);
}

bool MarginInline::IsLayoutDependent(const ComputedStyle* style,
                                     LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* MarginInline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      marginInlineShorthand(), style, layout_object, allow_visited_style);
}

bool Marker::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  const CSSValue* marker = css_parsing_utils::ParseLonghand(
      CSSPropertyID::kMarkerStart, CSSPropertyID::kMarker, context, range);
  if (!marker || !range.AtEnd()) {
    return false;
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kMarkerStart, CSSPropertyID::kMarker, *marker, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kMarkerMid, CSSPropertyID::kMarker, *marker, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kMarkerEnd, CSSPropertyID::kMarker, *marker, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* Marker::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const CSSValue* marker_start =
      ComputedStyleUtils::ValueForSVGResource(style.MarkerStartResource());
  if (*marker_start ==
          *ComputedStyleUtils::ValueForSVGResource(style.MarkerMidResource()) &&
      *marker_start ==
          *ComputedStyleUtils::ValueForSVGResource(style.MarkerEndResource())) {
    return marker_start;
  }
  return nullptr;
}

bool Offset::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  // TODO(meade): The propertyID parameter isn't used - it can be removed
  // once all of the ParseSingleValue implementations have been moved to the
  // CSSPropertys, and the base CSSProperty::ParseSingleValue contains
  // no functionality.
  const CSSValue* offset_position =
      To<Longhand>(GetCSSPropertyOffsetPosition())
          .ParseSingleValue(range, context, CSSParserLocalContext());
  const CSSValue* offset_path =
      css_parsing_utils::ConsumeOffsetPath(range, context);
  const CSSValue* offset_distance = nullptr;
  const CSSValue* offset_rotate = nullptr;
  if (offset_path) {
    offset_distance = css_parsing_utils::ConsumeLengthOrPercent(
        range, context, CSSPrimitiveValue::ValueRange::kAll);
    offset_rotate = css_parsing_utils::ConsumeOffsetRotate(range, context);
    if (offset_rotate && !offset_distance) {
      offset_distance = css_parsing_utils::ConsumeLengthOrPercent(
          range, context, CSSPrimitiveValue::ValueRange::kAll);
    }
  }
  const CSSValue* offset_anchor = nullptr;
  if (css_parsing_utils::ConsumeSlashIncludingWhitespace(range)) {
    offset_anchor =
        To<Longhand>(GetCSSPropertyOffsetAnchor())
            .ParseSingleValue(range, context, CSSParserLocalContext());
    if (!offset_anchor) {
      return false;
    }
  }
  if ((!offset_position && !offset_path) || !range.AtEnd()) {
    return false;
  }

  if ((offset_position || offset_anchor) &&
      !RuntimeEnabledFeatures::CSSOffsetPositionAnchorEnabled()) {
    return false;
  }

  if (offset_position) {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kOffsetPosition, CSSPropertyID::kOffset,
        *offset_position, important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  } else if (RuntimeEnabledFeatures::CSSOffsetPositionAnchorEnabled()) {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kOffsetPosition, CSSPropertyID::kOffset,
        *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  }

  if (offset_path) {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kOffsetPath, CSSPropertyID::kOffset, *offset_path,
        important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
        properties);
  } else {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kOffsetPath, CSSPropertyID::kOffset,
        *CSSIdentifierValue::Create(CSSValueID::kNone), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  }

  if (offset_distance) {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kOffsetDistance, CSSPropertyID::kOffset,
        *offset_distance, important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  } else {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kOffsetDistance, CSSPropertyID::kOffset,
        *CSSNumericLiteralValue::Create(0,
                                        CSSPrimitiveValue::UnitType::kPixels),
        important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
        properties);
  }

  if (offset_rotate) {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kOffsetRotate, CSSPropertyID::kOffset, *offset_rotate,
        important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
        properties);
  } else {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kOffsetRotate, CSSPropertyID::kOffset,
        *CSSIdentifierValue::Create(CSSValueID::kAuto), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  }

  if (offset_anchor) {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kOffsetAnchor, CSSPropertyID::kOffset, *offset_anchor,
        important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
        properties);
  } else if (RuntimeEnabledFeatures::CSSOffsetPositionAnchorEnabled()) {
    css_parsing_utils::AddProperty(
        CSSPropertyID::kOffsetAnchor, CSSPropertyID::kOffset,
        *CSSIdentifierValue::Create(CSSValueID::kAuto), important,
        css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  }

  return true;
}

const CSSValue* Offset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForOffset(style, layout_object,
                                            allow_visited_style);
}

bool Outline::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      outlineShorthand(), important, context, range, properties);
}

const CSSValue* Outline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      outlineShorthand(), style, layout_object, allow_visited_style);
}

bool Overflow::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      overflowShorthand(), important, context, range, properties);
}

const CSSValue* Overflow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSIdentifierValue::Create(style.OverflowX()));
  if (style.OverflowX() != style.OverflowY()) {
    list->Append(*CSSIdentifierValue::Create(style.OverflowY()));
  }

  return list;
}

bool OverscrollBehavior::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      overscrollBehaviorShorthand(), important, context, range, properties);
}

const CSSValue* OverscrollBehavior::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSIdentifierValue::Create(style.OverscrollBehaviorX()));
  if (style.OverscrollBehaviorX() != style.OverscrollBehaviorY()) {
    list->Append(*CSSIdentifierValue::Create(style.OverscrollBehaviorY()));
  }

  return list;
}

bool PaddingBlock::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      paddingBlockShorthand(), important, context, range, properties);
}

const CSSValue* PaddingBlock::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      paddingBlockShorthand(), style, layout_object, allow_visited_style);
}

bool Padding::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia4Longhands(
      paddingShorthand(), important, context, range, properties);
}

bool Padding::IsLayoutDependent(const ComputedStyle* style,
                                LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingBottom().IsFixed() ||
          !style->PaddingTop().IsFixed() || !style->PaddingLeft().IsFixed() ||
          !style->PaddingRight().IsFixed());
}

const CSSValue* Padding::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForSidesShorthand(
      paddingShorthand(), style, layout_object, allow_visited_style);
}

bool PaddingInline::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      paddingInlineShorthand(), important, context, range, properties);
}

const CSSValue* PaddingInline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      paddingInlineShorthand(), style, layout_object, allow_visited_style);
}

bool PageBreakAfter::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValueID value;
  if (!css_parsing_utils::ConsumeFromPageBreakBetween(range, value)) {
    return false;
  }

  DCHECK(IsValidCSSValueID(value));
  css_parsing_utils::AddProperty(
      CSSPropertyID::kBreakAfter, CSSPropertyID::kPageBreakAfter,
      *CSSIdentifierValue::Create(value), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* PageBreakAfter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForPageBreakBetween(style.BreakAfter());
}

bool PageBreakBefore::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValueID value;
  if (!css_parsing_utils::ConsumeFromPageBreakBetween(range, value)) {
    return false;
  }

  DCHECK(IsValidCSSValueID(value));
  css_parsing_utils::AddProperty(
      CSSPropertyID::kBreakBefore, CSSPropertyID::kPageBreakBefore,
      *CSSIdentifierValue::Create(value), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* PageBreakBefore::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForPageBreakBetween(style.BreakBefore());
}

bool PageBreakInside::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValueID value;
  if (!css_parsing_utils::ConsumeFromColumnOrPageBreakInside(range, value)) {
    return false;
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kBreakInside, CSSPropertyID::kPageBreakInside,
      *CSSIdentifierValue::Create(value), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* PageBreakInside::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForPageBreakInside(style.BreakInside());
}

bool PlaceContent::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  DCHECK_EQ(shorthandForProperty(CSSPropertyID::kPlaceContent).length(), 2u);

  CSSParserTokenRange range_copy = range;
  bool is_baseline = css_parsing_utils::IsBaselineKeyword(range.Peek().Id());
  const CSSValue* align_content_value =
      To<Longhand>(GetCSSPropertyAlignContent())
          .ParseSingleValue(range, context, local_context);
  if (!align_content_value) {
    return false;
  }

  const CSSValue* justify_content_value = nullptr;
  if (range.AtEnd()) {
    if (is_baseline) {
      justify_content_value =
          MakeGarbageCollected<cssvalue::CSSContentDistributionValue>(
              CSSValueID::kInvalid, CSSValueID::kStart, CSSValueID::kInvalid);
    } else {
      range = range_copy;
    }
  }
  if (!justify_content_value) {
    justify_content_value =
        To<Longhand>(GetCSSPropertyJustifyContent())
            .ParseSingleValue(range, context, local_context);
  }

  if (!justify_content_value || !range.AtEnd()) {
    return false;
  }

  DCHECK(align_content_value);
  DCHECK(justify_content_value);

  css_parsing_utils::AddProperty(
      CSSPropertyID::kAlignContent, CSSPropertyID::kPlaceContent,
      *align_content_value, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kJustifyContent, CSSPropertyID::kPlaceContent,
      *justify_content_value, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);

  return true;
}

const CSSValue* PlaceContent::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForPlaceShorthand(
      placeContentShorthand(), style, layout_object, allow_visited_style);
}

bool PlaceItems::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  DCHECK_EQ(shorthandForProperty(CSSPropertyID::kPlaceItems).length(), 2u);

  CSSParserTokenRange range_copy = range;
  const CSSValue* align_items_value =
      To<Longhand>(GetCSSPropertyAlignItems())
          .ParseSingleValue(range, context, local_context);
  if (!align_items_value) {
    return false;
  }

  if (range.AtEnd()) {
    range = range_copy;
  }

  const CSSValue* justify_items_value =
      To<Longhand>(GetCSSPropertyJustifyItems())
          .ParseSingleValue(range, context, local_context);
  if (!justify_items_value || !range.AtEnd()) {
    return false;
  }

  DCHECK(align_items_value);
  DCHECK(justify_items_value);

  css_parsing_utils::AddProperty(
      CSSPropertyID::kAlignItems, CSSPropertyID::kPlaceItems,
      *align_items_value, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kJustifyItems, CSSPropertyID::kPlaceItems,
      *justify_items_value, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);

  return true;
}

const CSSValue* PlaceItems::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForPlaceShorthand(
      placeItemsShorthand(), style, layout_object, allow_visited_style);
}

bool PlaceSelf::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  DCHECK_EQ(shorthandForProperty(CSSPropertyID::kPlaceSelf).length(), 2u);

  CSSParserTokenRange range_copy = range;
  const CSSValue* align_self_value =
      To<Longhand>(GetCSSPropertyAlignSelf())
          .ParseSingleValue(range, context, local_context);
  if (!align_self_value) {
    return false;
  }

  if (range.AtEnd()) {
    range = range_copy;
  }

  const CSSValue* justify_self_value =
      To<Longhand>(GetCSSPropertyJustifySelf())
          .ParseSingleValue(range, context, local_context);
  if (!justify_self_value || !range.AtEnd()) {
    return false;
  }

  DCHECK(align_self_value);
  DCHECK(justify_self_value);

  css_parsing_utils::AddProperty(
      CSSPropertyID::kAlignSelf, CSSPropertyID::kPlaceSelf, *align_self_value,
      important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
      properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kJustifySelf, CSSPropertyID::kPlaceSelf,
      *justify_self_value, important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);

  return true;
}

const CSSValue* PlaceSelf::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForPlaceShorthand(
      placeSelfShorthand(), style, layout_object, allow_visited_style);
}

bool ScrollMarginBlock::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      scrollMarginBlockShorthand(), important, context, range, properties);
}

const CSSValue* ScrollMarginBlock::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      scrollMarginBlockShorthand(), style, layout_object, allow_visited_style);
}

bool ScrollMargin::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia4Longhands(
      scrollMarginShorthand(), important, context, range, properties);
}

const CSSValue* ScrollMargin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForSidesShorthand(
      scrollMarginShorthand(), style, layout_object, allow_visited_style);
}

bool ScrollMarginInline::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      scrollMarginInlineShorthand(), important, context, range, properties);
}

const CSSValue* ScrollMarginInline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      scrollMarginInlineShorthand(), style, layout_object, allow_visited_style);
}

bool ScrollPaddingBlock::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      scrollPaddingBlockShorthand(), important, context, range, properties);
}

const CSSValue* ScrollPaddingBlock::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      scrollPaddingBlockShorthand(), style, layout_object, allow_visited_style);
}

bool ScrollPadding::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia4Longhands(
      scrollPaddingShorthand(), important, context, range, properties);
}

const CSSValue* ScrollPadding::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForSidesShorthand(
      scrollPaddingShorthand(), style, layout_object, allow_visited_style);
}

bool ScrollPaddingInline::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandVia2Longhands(
      scrollPaddingInlineShorthand(), important, context, range, properties);
}

const CSSValue* ScrollPaddingInline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForInlineBlockShorthand(
      scrollPaddingInlineShorthand(), style, layout_object,
      allow_visited_style);
}

namespace {

// Consume a single name, axis, and optionally inset, then append the result to
// `name_list`, `axis_list`, and `inset_list` respectively.
//
// Insets are only relevant for the view-timeline shorthand, and not for
// the scroll-timeline shorthand, hence `inset_list` may be nullptr.
//
// https://drafts.csswg.org/scroll-animations-1/#view-timeline-shorthand
// https://drafts.csswg.org/scroll-animations-1/#scroll-timeline-shorthand
bool ConsumeTimelineItemInto(CSSParserTokenRange& range,
                             const CSSParserContext& context,
                             CSSValueList* name_list,
                             CSSValueList* axis_list,
                             CSSValueList* inset_list) {
  using css_parsing_utils::ConsumeSingleTimelineAxis;
  using css_parsing_utils::ConsumeSingleTimelineInset;
  using css_parsing_utils::ConsumeSingleTimelineName;

  CSSValue* name = ConsumeSingleTimelineName(range, context);

  if (!name) {
    return false;
  }

  CSSValue* axis = nullptr;
  CSSValue* inset = nullptr;

  // [ <'view-timeline-axis'> || <'view-timeline-inset'> ]
  while (true) {
    if (!axis && (axis = ConsumeSingleTimelineAxis(range))) {
      continue;
    }
    if (inset_list && !inset &&
        (inset = ConsumeSingleTimelineInset(range, context))) {
      continue;
    }
    break;
  }

  if (!axis) {
    axis = CSSIdentifierValue::Create(CSSValueID::kBlock);
  }
  if (inset_list && !inset) {
    inset = MakeGarbageCollected<CSSValuePair>(
        CSSIdentifierValue::Create(CSSValueID::kAuto),
        CSSIdentifierValue::Create(CSSValueID::kAuto),
        CSSValuePair::kDropIdenticalValues);
  }

  DCHECK(name_list);
  DCHECK(axis_list);
  name_list->Append(*name);
  axis_list->Append(*axis);
  if (inset) {
    DCHECK(inset_list);
    inset_list->Append(*inset);
  }

  return true;
}

bool ParseTimelineShorthand(CSSPropertyID shorthand_id,
                            const StylePropertyShorthand& shorthand,
                            bool important,
                            CSSParserTokenRange& range,
                            const CSSParserContext& context,
                            const CSSParserLocalContext&,
                            HeapVector<CSSPropertyValue, 64>& properties) {
  using css_parsing_utils::AddProperty;
  using css_parsing_utils::ConsumeCommaIncludingWhitespace;
  using css_parsing_utils::IsImplicitProperty;

  CSSValueList* name_list = CSSValueList::CreateCommaSeparated();
  CSSValueList* axis_list = CSSValueList::CreateCommaSeparated();
  CSSValueList* inset_list =
      shorthand.length() == 3u ? CSSValueList::CreateCommaSeparated() : nullptr;

  do {
    if (!ConsumeTimelineItemInto(range, context, name_list, axis_list,
                                 inset_list)) {
      return false;
    }
  } while (ConsumeCommaIncludingWhitespace(range));

  DCHECK(name_list->length());
  DCHECK(axis_list->length());
  DCHECK(!inset_list || inset_list->length());
  DCHECK_EQ(name_list->length(), axis_list->length());
  DCHECK_EQ(inset_list ? name_list->length() : 0,
            inset_list ? inset_list->length() : 0);

  DCHECK_GE(shorthand.length(), 2u);
  DCHECK_LE(shorthand.length(), 3u);
  AddProperty(shorthand.properties()[0]->PropertyID(), shorthand_id, *name_list,
              important, IsImplicitProperty::kNotImplicit, properties);
  AddProperty(shorthand.properties()[1]->PropertyID(), shorthand_id, *axis_list,
              important, IsImplicitProperty::kNotImplicit, properties);
  if (inset_list) {
    DCHECK_EQ(shorthand.length(), 3u);
    AddProperty(shorthand.properties()[2]->PropertyID(), shorthand_id,
                *inset_list, important, IsImplicitProperty::kNotImplicit,
                properties);
  }

  return range.AtEnd();
}

static CSSValue* CSSValueForTimelineShorthand(
    const HeapVector<Member<const ScopedCSSName>>& name_vector,
    const Vector<TimelineAxis>& axis_vector,
    const Vector<TimelineInset>* inset_vector,
    const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();

  if (name_vector.size() != axis_vector.size()) {
    return list;
  }
  if (inset_vector && name_vector.size() != inset_vector->size()) {
    return list;
  }
  if (name_vector.empty()) {
    list->Append(*ComputedStyleUtils::SingleValueForTimelineShorthand(
        /* name */ nullptr, TimelineAxis::kBlock, /* inset */ absl::nullopt,
        style));
    return list;
  }
  for (wtf_size_t i = 0; i < name_vector.size(); ++i) {
    list->Append(*ComputedStyleUtils::SingleValueForTimelineShorthand(
        name_vector[i].Get(), axis_vector[i],
        inset_vector ? absl::optional<TimelineInset>((*inset_vector)[i])
                     : absl::optional<TimelineInset>(),
        style));
  }

  return list;
}

}  // namespace

bool ScrollStart::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValue* block_value = css_parsing_utils::ConsumeScrollStart(range, context);
  if (!block_value) {
    return false;
  }
  CSSValue* inline_value =
      css_parsing_utils::ConsumeScrollStart(range, context);
  if (!inline_value) {
    inline_value = CSSIdentifierValue::Create(CSSValueID::kStart);
  }
  AddProperty(scrollStartShorthand().properties()[0]->PropertyID(),
              scrollStartShorthand().id(), *block_value, important,
              css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  AddProperty(scrollStartShorthand().properties()[1]->PropertyID(),
              scrollStartShorthand().id(), *inline_value, important,
              css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return range.AtEnd();
}

const CSSValue* ScrollStart::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const CSSValue* block_value =
      scrollStartShorthand().properties()[0]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style);
  const CSSValue* inline_value =
      scrollStartShorthand().properties()[1]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style);
  if (const auto* ident_value = DynamicTo<CSSIdentifierValue>(inline_value);
      !ident_value || ident_value->GetValueID() != CSSValueID::kStart) {
    return MakeGarbageCollected<CSSValuePair>(
        block_value, inline_value, CSSValuePair::kDropIdenticalValues);
  }
  return block_value;
}

bool ScrollStartTarget::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValue* block_value = css_parsing_utils::ConsumeScrollStartTarget(range);
  if (!block_value) {
    return false;
  }
  CSSValue* inline_value = css_parsing_utils::ConsumeScrollStartTarget(range);
  if (!inline_value) {
    inline_value = CSSIdentifierValue::Create(CSSValueID::kNone);
  }
  AddProperty(scrollStartTargetShorthand().properties()[0]->PropertyID(),
              scrollStartTargetShorthand().id(), *block_value, important,
              css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  AddProperty(scrollStartTargetShorthand().properties()[1]->PropertyID(),
              scrollStartTargetShorthand().id(), *inline_value, important,
              css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return range.AtEnd();
}

const CSSValue* ScrollStartTarget::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const CSSValue* block_value =
      scrollStartTargetShorthand().properties()[0]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style);
  const CSSValue* inline_value =
      scrollStartTargetShorthand().properties()[1]->CSSValueFromComputedStyle(
          style, layout_object, allow_visited_style);
  if (To<CSSIdentifierValue>(*inline_value).GetValueID() != CSSValueID::kNone) {
    return MakeGarbageCollected<CSSValuePair>(
        block_value, inline_value, CSSValuePair::kDropIdenticalValues);
  }
  return block_value;
}

bool ScrollTimeline::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return ParseTimelineShorthand(CSSPropertyID::kScrollTimeline,
                                scrollTimelineShorthand(), important, range,
                                context, local_context, properties);
}

const CSSValue* ScrollTimeline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const HeapVector<Member<const ScopedCSSName>>& name_vector =
      style.ScrollTimelineName() ? style.ScrollTimelineName()->GetNames()
                                 : HeapVector<Member<const ScopedCSSName>>{};
  const Vector<TimelineAxis>& axis_vector = style.ScrollTimelineAxis();
  return CSSValueForTimelineShorthand(name_vector, axis_vector,
                                      /* inset_vector */ nullptr, style);
}

bool TextDecoration::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  // Use RuntimeEnabledFeature-aware shorthandForProperty() method until
  // text-decoration-thickness ships, see style_property_shorthand.cc.tmpl.
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      shorthandForProperty(CSSPropertyID::kTextDecoration), important, context,
      range, properties);
}

const CSSValue* TextDecoration::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  // Use RuntimeEnabledFeature-aware shorthandForProperty() method until
  // text-decoration-thickness ships, see style_property_shorthand.cc.tmpl.
  const StylePropertyShorthand& shorthand =
      shorthandForProperty(CSSPropertyID::kTextDecoration);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (unsigned i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value =
        shorthand.properties()[i]->CSSValueFromComputedStyle(
            style, layout_object, allow_visited_style);
    // Do not include initial value 'auto' for thickness.
    // TODO(https://crbug.com/1093826): general shorthand serialization issues
    // remain, in particular for text-decoration.
    if (shorthand.properties()[i]->PropertyID() ==
        CSSPropertyID::kTextDecorationThickness) {
      if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
        CSSValueID value_id = identifier_value->GetValueID();
        if (value_id == CSSValueID::kAuto) {
          continue;
        }
      }
    }
    DCHECK(value);
    list->Append(*value);
  }
  return list;
}

namespace {

CSSValue* ConsumeTransitionValue(CSSPropertyID property,
                                 CSSParserTokenRange& range,
                                 const CSSParserContext& context,
                                 bool use_legacy_parsing) {
  switch (property) {
    case CSSPropertyID::kTransitionDelay:
      return css_parsing_utils::ConsumeTime(
          range, context, CSSPrimitiveValue::ValueRange::kAll);
    case CSSPropertyID::kTransitionDuration:
      return css_parsing_utils::ConsumeTime(
          range, context, CSSPrimitiveValue::ValueRange::kNonNegative);
    case CSSPropertyID::kTransitionProperty:
      return css_parsing_utils::ConsumeTransitionProperty(range, context);
    case CSSPropertyID::kTransitionTimingFunction:
      return css_parsing_utils::ConsumeAnimationTimingFunction(range, context);
    case CSSPropertyID::kTransitionBehavior:
      if (css_parsing_utils::IsValidTransitionBehavior(range.Peek().Id())) {
        return CSSIdentifierValue::Create(
            range.ConsumeIncludingWhitespace().Id());
      }
      return nullptr;
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace

bool Transition::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  const StylePropertyShorthand shorthand = transitionShorthandForParsing();
  const unsigned longhand_count = shorthand.length();

  // Only relevant for 'animation'.
  auto is_reset_only_function = [](CSSPropertyID) { return false; };

  HeapVector<Member<CSSValueList>, css_parsing_utils::kMaxNumAnimationLonghands>
      longhands(longhand_count);
  if (!css_parsing_utils::ConsumeAnimationShorthand(
          shorthand, longhands, ConsumeTransitionValue, is_reset_only_function,
          range, context, local_context.UseAliasParsing())) {
    return false;
  }

  for (unsigned i = 0; i < longhand_count; ++i) {
    if (shorthand.properties()[i]->IDEquals(
            CSSPropertyID::kTransitionProperty) &&
        !css_parsing_utils::IsValidPropertyList(*longhands[i])) {
      return false;
    }
  }

  for (unsigned i = 0; i < longhand_count; ++i) {
    css_parsing_utils::AddProperty(
        shorthand.properties()[i]->PropertyID(), shorthand.id(), *longhands[i],
        important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
        properties);
  }

  return range.AtEnd();
}

const CSSValue* Transition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const CSSTransitionData* transition_data = style.Transitions();
  if (transition_data) {
    CSSValueList* transitions_list = CSSValueList::CreateCommaSeparated();
    for (wtf_size_t i = 0; i < transition_data->PropertyList().size(); ++i) {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(*ComputedStyleUtils::CreateTransitionPropertyValue(
          transition_data->PropertyList()[i]));
      list->Append(*CSSNumericLiteralValue::Create(
          CSSTimingData::GetRepeated(transition_data->DurationList(), i)
              .value(),
          CSSPrimitiveValue::UnitType::kSeconds));
      list->Append(*ComputedStyleUtils::ValueForAnimationTimingFunction(
          CSSTimingData::GetRepeated(transition_data->TimingFunctionList(),
                                     i)));
      list->Append(*ComputedStyleUtils::ValueForAnimationDelayStart(
          CSSTimingData::GetRepeated(transition_data->DelayStartList(), i)));
      if (CSSTimingData::GetRepeated(transition_data->BehaviorList(), i) !=
          CSSTransitionData::InitialBehavior()) {
        list->Append(*ComputedStyleUtils::CreateTransitionBehaviorValue(
            transition_data->BehaviorList()[i]));
      }
      transitions_list->Append(*list);
    }
    return transitions_list;
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  // transition-property default value.
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kAll));
  list->Append(*CSSNumericLiteralValue::Create(
      CSSTransitionData::InitialDuration().value(),
      CSSPrimitiveValue::UnitType::kSeconds));
  list->Append(*ComputedStyleUtils::ValueForAnimationTimingFunction(
      CSSTransitionData::InitialTimingFunction()));
  list->Append(*ComputedStyleUtils::ValueForAnimationDelayStart(
      CSSTransitionData::InitialDelayStart()));
  return list;
}

bool ViewTimeline::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return ParseTimelineShorthand(CSSPropertyID::kViewTimeline,
                                viewTimelineShorthand(), important, range,
                                context, local_context, properties);
}

const CSSValue* ViewTimeline::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const HeapVector<Member<const ScopedCSSName>>& name_vector =
      style.ViewTimelineName() ? style.ViewTimelineName()->GetNames()
                               : HeapVector<Member<const ScopedCSSName>>{};
  const Vector<TimelineAxis>& axis_vector = style.ViewTimelineAxis();
  return CSSValueForTimelineShorthand(name_vector, axis_vector,
                                      /* inset_vector */ nullptr, style);
}

bool AlternativeViewTimelineWithInset::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return ParseTimelineShorthand(
      CSSPropertyID::kAlternativeViewTimelineWithInset,
      alternativeViewTimelineWithInsetShorthand(), important, range, context,
      local_context, properties);
}

const CSSValue*
AlternativeViewTimelineWithInset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  const HeapVector<Member<const ScopedCSSName>>& name_vector =
      style.ViewTimelineName() ? style.ViewTimelineName()->GetNames()
                               : HeapVector<Member<const ScopedCSSName>>{};
  const Vector<TimelineAxis>& axis_vector = style.ViewTimelineAxis();
  const Vector<TimelineInset>& inset_vector = style.ViewTimelineInset();
  return CSSValueForTimelineShorthand(name_vector, axis_vector, &inset_vector,
                                      style);
}

bool WebkitColumnBreakAfter::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValueID value;
  if (!css_parsing_utils::ConsumeFromColumnBreakBetween(range, value)) {
    return false;
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kBreakAfter, CSSPropertyID::kWebkitColumnBreakAfter,
      *CSSIdentifierValue::Create(value), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* WebkitColumnBreakAfter::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForWebkitColumnBreakBetween(
      style.BreakAfter());
}

bool WebkitColumnBreakBefore::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValueID value;
  if (!css_parsing_utils::ConsumeFromColumnBreakBetween(range, value)) {
    return false;
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kBreakBefore, CSSPropertyID::kWebkitColumnBreakBefore,
      *CSSIdentifierValue::Create(value), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* WebkitColumnBreakBefore::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForWebkitColumnBreakBetween(
      style.BreakBefore());
}

bool WebkitColumnBreakInside::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValueID value;
  if (!css_parsing_utils::ConsumeFromColumnOrPageBreakInside(range, value)) {
    return false;
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kBreakInside, CSSPropertyID::kWebkitColumnBreakInside,
      *CSSIdentifierValue::Create(value), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* WebkitColumnBreakInside::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForWebkitColumnBreakInside(
      style.BreakInside());
}

bool WebkitMaskBoxImage::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CSSValue* source = nullptr;
  CSSValue* slice = nullptr;
  CSSValue* width = nullptr;
  CSSValue* outset = nullptr;
  CSSValue* repeat = nullptr;

  if (!css_parsing_utils::ConsumeBorderImageComponents(
          range, context, source, slice, width, outset, repeat,
          css_parsing_utils::DefaultFill::kFill)) {
    return false;
  }

  css_parsing_utils::AddProperty(
      CSSPropertyID::kWebkitMaskBoxImageSource,
      CSSPropertyID::kWebkitMaskBoxImage,
      source ? *source : *CSSInitialValue::Create(), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kWebkitMaskBoxImageSlice,
      CSSPropertyID::kWebkitMaskBoxImage,
      slice ? *slice : *CSSInitialValue::Create(), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kWebkitMaskBoxImageWidth,
      CSSPropertyID::kWebkitMaskBoxImage,
      width ? *width : *CSSInitialValue::Create(), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kWebkitMaskBoxImageOutset,
      CSSPropertyID::kWebkitMaskBoxImage,
      outset ? *outset : *CSSInitialValue::Create(), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  css_parsing_utils::AddProperty(
      CSSPropertyID::kWebkitMaskBoxImageRepeat,
      CSSPropertyID::kWebkitMaskBoxImage,
      repeat ? *repeat : *CSSInitialValue::Create(), important,
      css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);

  return true;
}

const CSSValue* WebkitMaskBoxImage::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImage(style.MaskBoxImage(), style,
                                                    allow_visited_style);
}

bool WebkitMask::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CHECK(!RuntimeEnabledFeatures::CSSMaskingInteropEnabled());
  return css_parsing_utils::ParseBackgroundOrMask(important, range, context,
                                                  local_context, properties);
}

bool AlternativeMask::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  CHECK(RuntimeEnabledFeatures::CSSMaskingInteropEnabled());
  return css_parsing_utils::ParseBackgroundOrMask(important, range, context,
                                                  local_context, properties);
}

const CSSValue* AlternativeMask::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForMaskShorthand(
      alternativeMaskShorthand(), style, layout_object, allow_visited_style);
}

bool MaskPosition::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return ParseBackgroundOrMaskPosition(
      maskPositionShorthand(), important, range, context,
      local_context.UseAliasParsing()
          ? WebFeature::kThreeValuedPositionBackground
          : absl::optional<WebFeature>(),
      properties);
}

const CSSValue* MaskPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::BackgroundPositionOrWebkitMaskPosition(
      *this, style, &style.MaskLayers());
}

bool WebkitMaskPosition::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return ParseBackgroundOrMaskPosition(
      webkitMaskPositionShorthand(), important, range, context,
      WebFeature::kThreeValuedPositionBackground, properties);
}

const CSSValue* WebkitMaskPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::BackgroundPositionOrWebkitMaskPosition(
      *this, style, &style.MaskLayers());
}

bool TextEmphasis::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      textEmphasisShorthand(), important, context, range, properties);
}

const CSSValue* TextEmphasis::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      textEmphasisShorthand(), style, layout_object, allow_visited_style);
}

bool TextSpacing::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  const CSSParserTokenRange original_range = range;
  CSSValue* autospace = nullptr;
  CSSValue* spacing_trim = nullptr;

  // The `text-spacing` shorthand doesn't lean directly on the longhand's
  // grammar, instead uses the `autospace` and `spacing-trim` productions.
  // https://drafts.csswg.org/css-text-4/#text-spacing-property
  //
  // Try `normal | none` first.
  if (const CSSIdentifierValue* ident =
          css_parsing_utils::ConsumeIdent<CSSValueID::kNone,
                                          CSSValueID::kNormal>(range);
      ident && range.AtEnd()) {
    switch (ident->GetValueID()) {
      case CSSValueID::kNone:
        autospace = CSSIdentifierValue::Create(CSSValueID::kNoAutospace);
        spacing_trim = CSSIdentifierValue::Create(CSSValueID::kSpaceAll);
        break;
      case CSSValueID::kNormal:
        autospace = CSSIdentifierValue::Create(CSSValueID::kNormal);
        spacing_trim = CSSIdentifierValue::Create(CSSValueID::kSpaceFirst);
        break;
      default:
        NOTREACHED_NORETURN();
    }
  }

  // Try `<autospace> || <spacing-trim>`.
  if (!autospace) {
    range = original_range;

    while (!range.AtEnd()) {
      if (!autospace &&
          (autospace = css_parsing_utils::ConsumeAutospace(range))) {
        continue;
      }
      if (!spacing_trim &&
          (spacing_trim = css_parsing_utils::ConsumeSpacingTrim(range))) {
        continue;
      }
      return false;
    }
    if (!autospace && !spacing_trim) {
      return false;
    }
    if (!autospace) {
      autospace = CSSIdentifierValue::Create(CSSValueID::kNormal);
    }
    if (!spacing_trim) {
      spacing_trim = CSSIdentifierValue::Create(CSSValueID::kSpaceFirst);
    }
  }

  CHECK(autospace);
  AddProperty(CSSPropertyID::kTextAutospace, CSSPropertyID::kTextSpacing,
              *autospace, important,
              css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  CHECK(spacing_trim);
  AddProperty(CSSPropertyID::kTextSpacingTrim, CSSPropertyID::kTextSpacing,
              *spacing_trim, important,
              css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* TextSpacing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const ETextAutospace autospace = style.TextAutospace();
  const TextSpacingTrim spacing_trim =
      style.GetFontDescription().GetTextSpacingTrim();
  if (autospace == ComputedStyleInitialValues::InitialTextAutospace() &&
      spacing_trim == FontBuilder::InitialTextSpacingTrim()) {
    return CSSIdentifierValue::Create(CSSValueID::kNormal);
  }
  if (autospace == ETextAutospace::kNoAutospace &&
      spacing_trim == TextSpacingTrim::kSpaceAll) {
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  }

  const CSSValue* autospace_value =
      autospace == ComputedStyleInitialValues::InitialTextAutospace()
          ? nullptr
          : CSSIdentifierValue::Create(autospace);
  const CSSValue* spacing_trim_value =
      spacing_trim == FontBuilder::InitialTextSpacingTrim()
          ? nullptr
          : CSSIdentifierValue::Create(spacing_trim);
  if (!autospace_value) {
    CHECK(spacing_trim_value);
    return spacing_trim_value;
  }
  if (!spacing_trim_value) {
    return autospace_value;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*autospace_value);
  list->Append(*spacing_trim_value);
  return list;
}

bool WebkitTextStroke::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      webkitTextStrokeShorthand(), important, context, range, properties);
}

bool WhiteSpace::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 64>& properties) const {
  const CSSParserTokenRange original_range = range;

  // Try to parse as a pre-defined keyword. The `white-space` has pre-defined
  // keywords in addition to the multi-values shorthand, for the backward
  // compatibility with when it was a longhand.
  if (const CSSIdentifierValue* value = css_parsing_utils::ConsumeIdent<
          CSSValueID::kBreakSpaces, CSSValueID::kNormal, CSSValueID::kNowrap,
          CSSValueID::kPre, CSSValueID::kPreLine, CSSValueID::kPreWrap>(
          range)) {
    // Parse as a pre-defined keyword only if it is at the end. Some keywords
    // can be both a pre-defined keyword or a longhand value.
    if (range.AtEnd()) {
      const EWhiteSpace whitespace =
          CssValueIDToPlatformEnum<EWhiteSpace>(value->GetValueID());
      DCHECK(IsValidWhiteSpace(whitespace));
      AddProperty(
          CSSPropertyID::kWhiteSpaceCollapse, CSSPropertyID::kWhiteSpace,
          *CSSIdentifierValue::Create(ToWhiteSpaceCollapse(whitespace)),
          important, css_parsing_utils::IsImplicitProperty::kNotImplicit,
          properties);
      AddProperty(
          CSSPropertyID::kTextWrap, CSSPropertyID::kWhiteSpace,
          *CSSIdentifierValue::Create(ToTextWrap(whitespace)), important,
          css_parsing_utils::IsImplicitProperty::kNotImplicit, properties);
      return true;
    }

    // If `range` is not at end, the keyword is for longhands. Restore `range`.
    range = original_range;
  }

  // Consume multi-value syntax if the first identifier is not pre-defined.
  return css_parsing_utils::ConsumeShorthandGreedilyViaLonghands(
      whiteSpaceShorthand(), important, context, range, properties);
}

const CSSValue* WhiteSpace::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const EWhiteSpace whitespace = style.WhiteSpace();
  if (IsValidWhiteSpace(whitespace)) {
    const CSSValueID value = PlatformEnumToCSSValueID(whitespace);
    DCHECK_NE(value, CSSValueID::kNone);
    return CSSIdentifierValue::Create(value);
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  const WhiteSpaceCollapse collapse = style.GetWhiteSpaceCollapse();
  if (collapse != ComputedStyleInitialValues::InitialWhiteSpaceCollapse()) {
    list->Append(*CSSIdentifierValue::Create(collapse));
  }
  const TextWrap wrap = style.GetTextWrap();
  if (wrap != ComputedStyleInitialValues::InitialTextWrap()) {
    list->Append(*CSSIdentifierValue::Create(wrap));
  }
  // When all longhands are initial values, it should be `normal`, covered by
  // `IsValidWhiteSpace()` above.
  DCHECK(list->length());
  return list;
}

}  // namespace css_shorthand
}  // namespace blink
