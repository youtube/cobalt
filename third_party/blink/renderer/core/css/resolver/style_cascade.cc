// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"

#include <bit>
#include <optional>

#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"
#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/css_attr_type.h"
#include "third_party/blink/renderer/core/css/css_cyclic_variable_value.h"
#include "third_party/blink/renderer/core/css/css_flip_revert_value.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_invalid_variable_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/if_test.h"
#include "third_party/blink/renderer/core/css/kleene_value.h"
#include "third_party/blink/renderer/core/css/media_eval_utils.h"
#include "third_party/blink/renderer/core/css/parser/css_if_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/property_bitsets.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_expansion-inl.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_expansion.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_interpolations.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule_function_declarations.h"
#include "third_party/blink/renderer/core/css/try_value_flips.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

AtomicString ConsumeVariableName(CSSParserTokenStream& stream) {
  stream.ConsumeWhitespace();
  CSSParserToken ident_token = stream.ConsumeIncludingWhitespaceRaw();
  DCHECK_EQ(ident_token.GetType(), kIdentToken);
  return ident_token.Value().ToAtomicString();
}

bool ConsumeComma(CSSParserTokenStream& stream) {
  if (stream.Peek().GetType() == kCommaToken) {
    stream.ConsumeRaw();
    return true;
  }
  return false;
}

template <CSSParserTokenType... Types>
StringView ConsumeUntilPeekedTypeIs(CSSParserTokenStream& stream) {
  wtf_size_t value_start_offset = stream.LookAheadOffset();
  stream.SkipUntilPeekedTypeIs<Types...>();
  wtf_size_t value_end_offset = stream.LookAheadOffset();
  return stream.StringRangeAt(value_start_offset,
                              value_end_offset - value_start_offset);
}

const CSSValue* Parse(const CSSProperty& property,
                      CSSParserTokenStream& stream,
                      const CSSParserContext* context) {
  return CSSPropertyParser::ParseSingleValue(property.PropertyID(), stream,
                                             context);
}

const CSSValue* ValueAt(const MatchResult& result, uint32_t position) {
  wtf_size_t matched_properties_index = DecodeMatchedPropertiesIndex(position);
  wtf_size_t declaration_index = DecodeDeclarationIndex(position);
  const MatchedPropertiesVector& vector = result.GetMatchedProperties();
  const CSSPropertyValueSet* set = vector[matched_properties_index].properties;
  return &set->PropertyAt(declaration_index).Value();
}

const TreeScope& TreeScopeAt(const MatchResult& result, uint32_t position) {
  wtf_size_t matched_properties_index = DecodeMatchedPropertiesIndex(position);
  const MatchedProperties& properties =
      result.GetMatchedProperties()[matched_properties_index];
  DCHECK_EQ(properties.data_.origin, CascadeOrigin::kAuthor);
  return result.ScopeFromTreeOrder(properties.data_.tree_order);
}

const CSSValue* EnsureScopedValue(const Document& document,
                                  const MatchResult& match_result,
                                  CascadePriority priority,
                                  const CSSValue* value) {
  CascadeOrigin origin = priority.GetOrigin();
  const TreeScope* tree_scope{nullptr};
  if (origin == CascadeOrigin::kAuthor) {
    tree_scope = &TreeScopeAt(match_result, priority.GetPosition());
  } else if (origin == CascadeOrigin::kAuthorPresentationalHint) {
    tree_scope = &document;
  }
  return &value->EnsureScopedValue(tree_scope);
}

PropertyHandle ToPropertyHandle(const CSSProperty& property,
                                CascadePriority priority) {
  uint32_t position = priority.GetPosition();
  CSSPropertyID id = DecodeInterpolationPropertyID(position);
  if (id == CSSPropertyID::kVariable) {
    DCHECK(IsA<CustomProperty>(property));
    return PropertyHandle(property.GetPropertyNameAtomicString());
  }
  return PropertyHandle(CSSProperty::Get(id),
                        DecodeIsPresentationAttribute(position));
}

// https://drafts.csswg.org/css-cascade-4/#default
CascadeOrigin TargetOriginForRevert(CascadeOrigin origin) {
  switch (origin) {
    case CascadeOrigin::kNone:
    case CascadeOrigin::kTransition:
      NOTREACHED();
    case CascadeOrigin::kUserAgent:
      return CascadeOrigin::kNone;
    case CascadeOrigin::kUser:
      return CascadeOrigin::kUserAgent;
    case CascadeOrigin::kAuthorPresentationalHint:
    case CascadeOrigin::kAuthor:
    case CascadeOrigin::kAnimation:
      return CascadeOrigin::kUser;
  }
}

CSSPropertyID UnvisitedID(CSSPropertyID id) {
  if (id == CSSPropertyID::kVariable) {
    return id;
  }
  const CSSProperty& property = CSSProperty::Get(id);
  if (!property.IsVisited()) {
    return id;
  }
  return property.GetUnvisitedProperty()->PropertyID();
}

bool IsInterpolation(CascadePriority priority) {
  switch (priority.GetOrigin()) {
    case CascadeOrigin::kAnimation:
    case CascadeOrigin::kTransition:
      return true;
    case CascadeOrigin::kNone:
    case CascadeOrigin::kUserAgent:
    case CascadeOrigin::kUser:
    case CascadeOrigin::kAuthorPresentationalHint:
    case CascadeOrigin::kAuthor:
      return false;
  }
}

std::optional<const CSSValue*> FindOrNullopt(
    const HeapHashMap<String, Member<const CSSValue>>& map,
    const String& key) {
  auto it = map.find(key);
  if (it == map.end()) {
    return std::nullopt;
  }
  return it->value.Get();
}

bool EvaluateContainerQuery(Element& element,
                            PseudoId pseudo_id,
                            const ContainerQuery& query,
                            TreeScope& tree_scope,
                            Element* nearest_size_container,
                            MatchResult& match_result) {
  const ContainerSelector& selector = query.Selector();
  if (!selector.SelectsAnyContainer()) {
    return false;
  }
  // TODO(crbug.com/394500600): Calling SetDependencyFlags here works,
  // but it's arguably a bit late when considering that MatchResult
  // is supposed to be the output of ElementRuleCollector.
  // Consider refactoring.
  ContainerQueryEvaluator::SetDependencyFlags(query, match_result);

  Element* starting_element = ContainerQueryEvaluator::DetermineStartingElement(
      element, pseudo_id, selector, nearest_size_container);
  Element* container = ContainerQueryEvaluator::FindContainer(
      starting_element, selector, &tree_scope);
  if (!container) {
    return false;
  }
  ContainerQueryEvaluator& evaluator =
      container->EnsureContainerQueryEvaluator();
  using Change = ContainerQueryEvaluator::Change;
  Change change = starting_element == container ? Change::kNearestContainer
                                                : Change::kDescendantContainers;
  return evaluator.EvalAndAdd(query, change, match_result);
}

}  // namespace

MatchResult& StyleCascade::MutableMatchResult() {
  DCHECK(!generation_) << "Apply has already been called";
  needs_match_result_analyze_ = true;
  return match_result_;
}

void StyleCascade::AddInterpolations(const ActiveInterpolationsMap* map,
                                     CascadeOrigin origin) {
  DCHECK(map);
  needs_interpolations_analyze_ = true;
  interpolations_.Add(map, origin);
}

void StyleCascade::Apply(CascadeFilter filter) {
  AnalyzeIfNeeded();
  state_.UpdateLengthConversionData();

  CascadeResolver resolver(filter, ++generation_);

  ApplyCascadeAffecting(resolver);

  ApplyHighPriority(resolver);
  state_.UpdateFont();

  if (map_.NativeBitset().Has(CSSPropertyID::kLineHeight)) {
    LookupAndApply(GetCSSPropertyLineHeight(), resolver);
  }
  state_.UpdateLineHeight();

  ApplyWideOverlapping(resolver);

  ApplyMatchResult(resolver);
  ApplyInterpolations(resolver);

  // These three flags are only used if HasAppearance() is set
  // (they are used for knowing whether appearance: auto is to be overridden),
  // but we compute them nevertheless, to avoid suddenly having to compute them
  // after-the-fact if inline style is updated incrementally.
  if (resolver.AuthorFlags() & CSSProperty::kBackground) {
    state_.StyleBuilder().SetHasAuthorBackground();
  }
  if (resolver.AuthorFlags() & CSSProperty::kBorder) {
    state_.StyleBuilder().SetHasAuthorBorder();
  }
  if (resolver.AuthorFlags() & CSSProperty::kBorderRadius) {
    state_.StyleBuilder().SetHasAuthorBorderRadius();
  }

  if ((state_.InsideLink() != EInsideLink::kInsideVisitedLink &&
       (resolver.AuthorFlags() & CSSProperty::kHighlightColors)) ||
      (state_.InsideLink() == EInsideLink::kInsideVisitedLink &&
       (resolver.AuthorFlags() & CSSProperty::kVisitedHighlightColors))) {
    state_.StyleBuilder().SetHasAuthorHighlightColors();
  }

  if (resolver.Flags() & CSSProperty::kAnimation) {
    state_.StyleBuilder().SetCanAffectAnimations();
  }
  if (resolver.RejectedFlags() & CSSProperty::kLegacyOverlapping) {
    state_.SetRejectedLegacyOverlapping();
  }

  // TOOD(crbug.com/1334570):
  //
  // Count applied H1 font-size from html.css UA stylesheet where H1 is inside
  // a sectioning element matching selectors like:
  //
  // :-webkit-any(article,aside,nav,section) h1 { ... }
  //
  if (state_.GetElement().HasTagName(html_names::kH1Tag)) {
    if (CascadePriority* priority =
            map_.Find(GetCSSPropertyFontSize().GetCSSPropertyName())) {
      if (priority->GetOrigin() != CascadeOrigin::kUserAgent) {
        return;
      }
      const CSSValue* value = ValueAt(match_result_, priority->GetPosition());
      if (const auto* numeric = DynamicTo<CSSNumericLiteralValue>(value)) {
        DCHECK(numeric->GetType() == CSSNumericLiteralValue::UnitType::kEms);
        if (numeric->DoubleValue() != 2.0) {
          CountUse(WebFeature::kH1UserAgentFontSizeInSectionApplied);
        }
      }
    }
  }

  ApplyUnresolvedEnv();
}

std::unique_ptr<CSSBitset> StyleCascade::GetImportantSet() {
  AnalyzeIfNeeded();
  if (!map_.HasImportant()) {
    return nullptr;
  }
  auto set = std::make_unique<CSSBitset>();
  for (CSSPropertyID id : map_.NativeBitset()) {
    // We use the unvisited ID because visited/unvisited colors are currently
    // interpolated together.
    // TODO(crbug.com/1062217): Interpolate visited colors separately
    set->Or(UnvisitedID(id), map_.At(CSSPropertyName(id)).IsImportant());
  }
  return set;
}

void StyleCascade::Reset() {
  map_.Reset();
  match_result_.Reset();
  interpolations_.Reset();
  generation_ = 0;
  depends_on_cascade_affecting_property_ = false;
}

const CSSValue* StyleCascade::Resolve(const CSSPropertyName& name,
                                      const CSSValue& value,
                                      CascadeOrigin origin,
                                      CascadeResolver& resolver) {
  CSSPropertyRef ref(name, state_.GetDocument());

  const CSSValue* resolved = Resolve(ResolveSurrogate(ref.GetProperty()), value,
                                     CascadePriority(origin), origin, resolver);

  DCHECK(resolved);

  // TODO(crbug.com/1185745): Cycles in animations get special handling by our
  // implementation. This is not per spec, but the correct behavior is not
  // defined at the moment.
  if (resolved->IsCyclicVariableValue()) {
    return nullptr;
  }

  // TODO(crbug.com/1185745): We should probably not return 'unset' for
  // properties where CustomProperty::SupportsGuaranteedInvalid return true.
  if (resolved->IsInvalidVariableValue()) {
    return cssvalue::CSSUnsetValue::Create();
  }

  return resolved;
}

HeapHashMap<CSSPropertyName, Member<const CSSValue>>
StyleCascade::GetCascadedValues() const {
  DCHECK(!needs_match_result_analyze_);
  DCHECK(!needs_interpolations_analyze_);
  DCHECK_GE(generation_, 0);

  HeapHashMap<CSSPropertyName, Member<const CSSValue>> result;

  for (CSSPropertyID id : map_.NativeBitset()) {
    CSSPropertyName name(id);
    CascadePriority priority = map_.At(name);
    if (IsInterpolation(priority)) {
      continue;
    }
    if (!priority.HasOrigin()) {
      // Declarations added for explicit defaults (AddExplicitDefaults)
      // should not be observable.
      continue;
    }
    const CSSValue* cascaded = ValueAt(match_result_, priority.GetPosition());
    DCHECK(cascaded);
    result.Set(name, cascaded);
  }

  for (const auto& name : map_.GetCustomMap().Keys()) {
    CascadePriority priority = map_.At(CSSPropertyName(name));
    DCHECK(priority.HasOrigin());
    if (IsInterpolation(priority)) {
      continue;
    }
    const CSSValue* cascaded = ValueAt(match_result_, priority.GetPosition());
    DCHECK(cascaded);
    result.Set(CSSPropertyName(name), cascaded);
  }

  return result;
}

const CSSValue* StyleCascade::Resolve(StyleResolverState& state,
                                      const CSSPropertyName& name,
                                      const CSSValue& value) {
  STACK_UNINITIALIZED StyleCascade cascade(state);

  // Since the cascade map is empty, the CascadeResolver isn't important,
  // as there can be no cycles in an empty map. We just instantiate it to
  // satisfy the API.
  CascadeResolver resolver(CascadeFilter(), /* generation */ 0);

  // The origin is relevant for 'revert', but since the cascade map
  // is empty, there will be nothing to revert to regardless of the origin
  // We use kNone, because kAuthor (etc) imply that the `value` originates
  // from a location on the `MatchResult`, which is not the case.
  CascadeOrigin origin = CascadeOrigin::kNone;

  return cascade.Resolve(name, value, origin, resolver);
}

void StyleCascade::AnalyzeIfNeeded() {
  if (needs_match_result_analyze_) {
    AnalyzeMatchResult();
    needs_match_result_analyze_ = false;
  }
  if (needs_interpolations_analyze_) {
    AnalyzeInterpolations();
    needs_interpolations_analyze_ = false;
  }
}

void StyleCascade::AnalyzeMatchResult() {
  AddExplicitDefaults();

  int index = 0;
  for (const MatchedProperties& properties :
       match_result_.GetMatchedProperties()) {
    ExpandCascade(
        properties, GetDocument(), index++,
        [this](CascadePriority cascade_priority,
               const AtomicString& custom_property_name) {
          map_.Add(custom_property_name, cascade_priority);
        },
        [this](CascadePriority cascade_priority, CSSPropertyID property_id) {
          if (kSurrogateProperties.Has(property_id)) {
            const CSSProperty& property =
                ResolveSurrogate(CSSProperty::Get(property_id));
            map_.Add(property.PropertyID(), cascade_priority);
          } else {
            map_.Add(property_id, cascade_priority);
          }
        });
  }
}

void StyleCascade::AnalyzeInterpolations() {
  const auto& entries = interpolations_.GetEntries();
  for (wtf_size_t i = 0; i < entries.size(); ++i) {
    for (const auto& active_interpolation : *entries[i].map) {
      auto name = active_interpolation.key.GetCSSPropertyName();
      uint32_t position = EncodeInterpolationPosition(
          name.Id(), i, active_interpolation.key.IsPresentationAttribute());
      CascadePriority priority(entries[i].origin,
                               /* important */ false,
                               /* tree_order */ 0,
                               /* is_inline_style */ false,
                               /* is_try_style */ false,
                               /* is_try_tactics_style */ false,
                               /* layer_order */ 0, position);

      CSSPropertyRef ref(name, GetDocument());
      DCHECK(ref.IsValid());

      if (name.IsCustomProperty()) {
        map_.Add(name.ToAtomicString(), priority);
      } else {
        const CSSProperty& property = ResolveSurrogate(ref.GetProperty());
        map_.Add(property.PropertyID(), priority);

        // Since an interpolation for an unvisited property also causes an
        // interpolation of the visited property, add the visited property to
        // the map as well.
        // TODO(crbug.com/1062217): Interpolate visited colors separately
        if (const CSSProperty* visited = property.GetVisitedProperty()) {
          map_.Add(visited->PropertyID(), priority);
        }
      }
    }
  }
}

// The implicit defaulting behavior of inherited properties is to take
// the value of the parent style [1]. However, we never reach
// Longhand::ApplyInherit for implicit defaults, which is needed to adjust
// Lengths with premultiplied zoom. Therefore, all inherited properties
// are instead explicitly defaulted [2] when the effective zoom has changed
// versus the parent zoom.
//
// [1] https://drafts.csswg.org/css-cascade/#defaulting
// [2] https://drafts.csswg.org/css-cascade/#defaulting-keywords
void StyleCascade::AddExplicitDefaults() {
  if (state_.GetDocument().StandardizedBrowserZoomEnabled() &&
      effective_zoom_changed_) {
    // These inherited properties can contain lengths:
    //
    //   -webkit-border-horizontal-spacing
    //   -webkit-border-vertical-spacing
    //   -webkit-text-stroke-width
    //   letter-spacing
    //   line-height
    //   list-style-image *
    //   stroke-dasharray
    //   stroke-dashoffset
    //   stroke-width **
    //   text-indent
    //   text-shadow
    //   text-underline-offset
    //   word-spacing
    //
    // * list-style-image need not be recomputed on zoom change because list
    // image marker is sized to 1em and font-size is already correctly zoomed.
    //
    // ** stroke-width gets special handling elsewhere.
    map_.Add(CSSPropertyID::kLetterSpacing,
             CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kLineHeight, CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kStrokeDasharray,
             CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kStrokeDashoffset,
             CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kTextIndent, CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kTextShadow, CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kTextUnderlineOffset,
             CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kWebkitTextStrokeWidth,
             CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kWebkitBorderHorizontalSpacing,
             CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kWebkitBorderVerticalSpacing,
             CascadePriority(CascadeOrigin::kNone));
    map_.Add(CSSPropertyID::kWordSpacing,
             CascadePriority(CascadeOrigin::kNone));
  }
}

void StyleCascade::Reanalyze() {
  map_.Reset();
  generation_ = 0;
  depends_on_cascade_affecting_property_ = false;

  needs_match_result_analyze_ = true;
  needs_interpolations_analyze_ = true;
  AnalyzeIfNeeded();
}

void StyleCascade::ApplyCascadeAffecting(CascadeResolver& resolver) {
  // During the initial call to Analyze, we speculatively assume that the
  // direction/writing-mode inherited from the parent will be the final
  // direction/writing-mode. If either property ends up with another value,
  // our assumption was incorrect, and we have to Reanalyze with the correct
  // values on ComputedStyle.
  auto direction = state_.StyleBuilder().Direction();
  auto writing_mode = state_.StyleBuilder().GetWritingMode();
  // Similarly, we assume that the effective zoom of this element
  // is the same as the parent's effective zoom. If it isn't,
  // we re-cascade with explicit defaults inserted at CascadeOrigin::kNone.
  //
  // See also StyleCascade::AddExplicitDefaults.
  float effective_zoom = state_.StyleBuilder().EffectiveZoom();

  if (map_.NativeBitset().Has(CSSPropertyID::kDirection)) {
    LookupAndApply(GetCSSPropertyDirection(), resolver);
  }
  if (map_.NativeBitset().Has(CSSPropertyID::kWritingMode)) {
    LookupAndApply(GetCSSPropertyWritingMode(), resolver);
  }
  if (map_.NativeBitset().Has(CSSPropertyID::kZoom)) {
    LookupAndApply(GetCSSPropertyZoom(), resolver);
  }

  bool reanalyze = false;

  if (depends_on_cascade_affecting_property_) {
    if (direction != state_.StyleBuilder().Direction() ||
        writing_mode != state_.StyleBuilder().GetWritingMode()) {
      reanalyze = true;
    }
  }
  if (effective_zoom != state_.StyleBuilder().EffectiveZoom()) {
    effective_zoom_changed_ = true;
    reanalyze = true;
  }

  if (reanalyze) {
    Reanalyze();
  }
}

void StyleCascade::ApplyHighPriority(CascadeResolver& resolver) {
  uint64_t bits = map_.HighPriorityBits();

  while (bits) {
    int i = std::countr_zero(bits);
    bits &= bits - 1;  // Clear the lowest bit.
    LookupAndApply(CSSProperty::Get(ConvertToCSSPropertyID(i)), resolver);
  }
}

void StyleCascade::ApplyWideOverlapping(CascadeResolver& resolver) {
  // Overlapping properties are handled as follows:
  //
  // 1. Apply the "wide" longhand which represents the entire computed value
  //    first. This is not always the non-legacy property,
  //    e.g.-webkit-border-image is one such longhand.
  // 2. For the other overlapping longhands (each of which represent a *part*
  //    of that computed value), *skip* applying that longhand if the wide
  //    longhand has a higher priority.
  //
  // This allows us to always apply the "wide" longhand in a fixed order versus
  // the other overlapping longhands, but still produce the same result as if
  // everything was applied in the order the properties were specified.

  // Skip `property` if its priority is lower than the incoming priority.
  // Skipping basically means pretending it's already applied by setting the
  // generation.
  auto maybe_skip = [this, &resolver](const CSSProperty& property,
                                      CascadePriority priority) {
    if (CascadePriority* p = map_.Find(property.GetCSSPropertyName())) {
      if (*p < priority) {
        *p = CascadePriority(*p, resolver.generation_);
      }
    }
  };

  const CSSProperty& webkit_border_image = GetCSSPropertyWebkitBorderImage();
  if (!resolver.filter_.Rejects(webkit_border_image)) {
    if (const CascadePriority* priority =
            map_.Find(webkit_border_image.GetCSSPropertyName())) {
      LookupAndApply(webkit_border_image, resolver);

      const auto& shorthand = borderImageShorthand();
      for (const CSSProperty* const longhand : shorthand.properties()) {
        maybe_skip(*longhand, *priority);
      }
    }
  }

  const CSSProperty& perspective_origin = GetCSSPropertyPerspectiveOrigin();
  if (!resolver.filter_.Rejects(perspective_origin)) {
    if (const CascadePriority* priority =
            map_.Find(perspective_origin.GetCSSPropertyName())) {
      LookupAndApply(perspective_origin, resolver);
      maybe_skip(GetCSSPropertyWebkitPerspectiveOriginX(), *priority);
      maybe_skip(GetCSSPropertyWebkitPerspectiveOriginY(), *priority);
    }
  }

  const CSSProperty& transform_origin = GetCSSPropertyTransformOrigin();
  if (!resolver.filter_.Rejects(transform_origin)) {
    if (const CascadePriority* priority =
            map_.Find(transform_origin.GetCSSPropertyName())) {
      LookupAndApply(transform_origin, resolver);
      maybe_skip(GetCSSPropertyWebkitTransformOriginX(), *priority);
      maybe_skip(GetCSSPropertyWebkitTransformOriginY(), *priority);
      maybe_skip(GetCSSPropertyWebkitTransformOriginZ(), *priority);
    }
  }

  // vertical-align will become a shorthand in the future - in order to
  // mitigate the forward compat risk, skip the baseline-source longhand.
  const CSSProperty& vertical_align = GetCSSPropertyVerticalAlign();
  if (!resolver.filter_.Rejects(vertical_align)) {
    if (const CascadePriority* priority =
            map_.Find(vertical_align.GetCSSPropertyName())) {
      LookupAndApply(vertical_align, resolver);
      maybe_skip(GetCSSPropertyBaselineSource(), *priority);
    }
  }

  // Note that -webkit-box-decoration-break isn't really more (or less)
  // "wide" than the non-prefixed counterpart, but they still share
  // a ComputedStyle location, and therefore need to be handled here.
  const CSSProperty& webkit_box_decoration_break =
      GetCSSPropertyWebkitBoxDecorationBreak();
  if (!resolver.filter_.Rejects(webkit_box_decoration_break)) {
    if (const CascadePriority* priority =
            map_.Find(webkit_box_decoration_break.GetCSSPropertyName())) {
      LookupAndApply(webkit_box_decoration_break, resolver);
      maybe_skip(GetCSSPropertyBoxDecorationBreak(), *priority);
    }
  }
}

// Go through all properties that were found during the analyze phase
// (e.g. in AnalyzeMatchResult()) and actually apply them. We need to do this
// in a second phase so that we know which ones actually won the cascade
// before we start applying, as some properties can affect others.
void StyleCascade::ApplyMatchResult(CascadeResolver& resolver) {
  // All the high-priority properties were dealt with in ApplyHighPriority(),
  // so we don't need to look at them again. (That would be a no-op due to
  // the generation check below, but it's cheaper just to mask them out
  // entirely.)
  for (auto it = map_.NativeBitset().BeginAfterHighPriority();
       it != map_.NativeBitset().end(); ++it) {
    CSSPropertyID id = *it;
    CascadePriority* p = map_.FindKnownToExist(id);
    const CascadePriority priority = *p;
    if (priority.GetGeneration() >= resolver.generation_) {
      // Already applied this generation.
      // Also checked in LookupAndApplyDeclaration,
      // but done here to get a fast exit.
      continue;
    }
    if (IsInterpolation(priority)) {
      continue;
    }

    const CSSProperty& property = CSSProperty::Get(id);
    if (resolver.Rejects(property)) {
      continue;
    }
    LookupAndApplyDeclaration(property, p, resolver);
  }

  for (auto& [name, priority_list] : map_.GetCustomMap()) {
    CascadePriority* p = &map_.Top(priority_list);
    CascadePriority priority = *p;
    if (priority.GetGeneration() >= resolver.generation_) {
      continue;
    }
    if (IsInterpolation(priority)) {
      continue;
    }

    CustomProperty property(name, GetDocument());
    if (resolver.Rejects(property)) {
      continue;
    }
    LookupAndApplyDeclaration(property, p, resolver);
  }
}

void StyleCascade::ApplyInterpolations(CascadeResolver& resolver) {
  const auto& entries = interpolations_.GetEntries();
  for (wtf_size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    ApplyInterpolationMap(*entry.map, entry.origin, i, resolver);
  }
}

void StyleCascade::ApplyInterpolationMap(const ActiveInterpolationsMap& map,
                                         CascadeOrigin origin,
                                         size_t index,
                                         CascadeResolver& resolver) {
  for (const auto& entry : map) {
    auto name = entry.key.GetCSSPropertyName();
    uint32_t position = EncodeInterpolationPosition(
        name.Id(), index, entry.key.IsPresentationAttribute());
    CascadePriority priority(origin,
                             /* important */ false,
                             /* tree_order */ 0,
                             /* is_inline_style */ false,
                             /* is_try_style */ false,
                             /* is_try_tactics_style */ false,
                             /* layer_order */ 0, position);
    priority = CascadePriority(priority, resolver.generation_);

    CSSPropertyRef ref(name, GetDocument());
    if (resolver.Rejects(ref.GetProperty())) {
      continue;
    }

    const CSSProperty& property = ResolveSurrogate(ref.GetProperty());

    CascadePriority* p = map_.Find(property.GetCSSPropertyName());
    if (!p || *p >= priority) {
      continue;
    }
    *p = priority;

    ApplyInterpolation(property, priority, *entry.value, resolver);
  }
}

void StyleCascade::ApplyInterpolation(
    const CSSProperty& property,
    CascadePriority priority,
    const ActiveInterpolations& interpolations,
    CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  CSSInterpolationTypesMap map(state_.GetDocument().GetPropertyRegistry(),
                               state_.GetDocument());
  CSSInterpolationEnvironment environment(map, state_, this, &resolver);

  const Interpolation& interpolation = *interpolations.front();
  if (IsA<InvalidatableInterpolation>(interpolation)) {
    InvalidatableInterpolation::ApplyStack(interpolations, environment);
  } else {
    To<TransitionInterpolation>(interpolation).Apply(environment);
  }

  // Applying a color property interpolation will also unconditionally apply
  // the -internal-visited- counterpart (see CSSColorInterpolationType::
  // ApplyStandardPropertyValue). To make sure !important rules in :visited
  // selectors win over animations, we re-apply the -internal-visited property
  // if its priority is higher.
  //
  // TODO(crbug.com/1062217): Interpolate visited colors separately
  if (const CSSProperty* visited = property.GetVisitedProperty()) {
    CascadePriority* visited_priority =
        map_.Find(visited->GetCSSPropertyName());
    if (visited_priority && priority < *visited_priority) {
      DCHECK(visited_priority->IsImportant());
      // Resetting generation to zero makes it possible to apply the
      // visited property again.
      *visited_priority = CascadePriority(*visited_priority, 0);
      LookupAndApply(*visited, resolver);
    }
  }
}

void StyleCascade::LookupAndApply(const CSSPropertyName& name,
                                  CascadeResolver& resolver) {
  CSSPropertyRef ref(name, state_.GetDocument());
  DCHECK(ref.IsValid());
  LookupAndApply(ref.GetProperty(), resolver);
}

void StyleCascade::LookupAndApply(const CSSProperty& property,
                                  CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  CSSPropertyName name = property.GetCSSPropertyName();
  DCHECK(!resolver.IsLocked(property));

  CascadePriority* priority = map_.Find(name);
  if (!priority) {
    return;
  }

  if (resolver.Rejects(property)) {
    return;
  }

  LookupAndApplyValue(property, priority, resolver);
}

void StyleCascade::LookupAndApplyValue(const CSSProperty& property,
                                       CascadePriority* priority,
                                       CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  if (priority->GetOrigin() < CascadeOrigin::kAnimation) {
    LookupAndApplyDeclaration(property, priority, resolver);
  } else if (priority->GetOrigin() >= CascadeOrigin::kAnimation) {
    LookupAndApplyInterpolation(property, priority, resolver);
  }
}

void StyleCascade::LookupAndApplyDeclaration(const CSSProperty& property,
                                             CascadePriority* priority,
                                             CascadeResolver& resolver) {
  if (priority->GetGeneration() >= resolver.generation_) {
    // Already applied this generation.
    return;
  }
  *priority = CascadePriority(*priority, resolver.generation_);
  DCHECK(!property.IsSurrogate());
  DCHECK(priority->GetOrigin() < CascadeOrigin::kAnimation);
  CascadeOrigin origin = priority->GetOrigin();
  // Values at CascadeOrigin::kNone are used for explicit defaulting,
  // see StyleCascade::AddExplicitDefaults.
  const CSSValue* value = (origin == CascadeOrigin::kNone)
                              ? cssvalue::CSSUnsetValue::Create()
                              : ValueAt(match_result_, priority->GetPosition());
  DCHECK(value);
  value = Resolve(property, *value, *priority, origin, resolver);
  DCHECK(IsA<CustomProperty>(property) || !value->IsUnparsedDeclaration());
  DCHECK(!value->IsPendingSubstitutionValue());
  value = EnsureScopedValue(GetDocument(), match_result_, *priority, value);
  StyleBuilder::ApplyPhysicalProperty(property, state_, *value);
}

void StyleCascade::LookupAndApplyInterpolation(const CSSProperty& property,
                                               CascadePriority* priority,
                                               CascadeResolver& resolver) {
  if (priority->GetGeneration() >= resolver.generation_) {
    // Already applied this generation.
    return;
  }
  *priority = CascadePriority(*priority, resolver.generation_);

  DCHECK(!property.IsSurrogate());

  // Interpolations for -internal-visited properties are applied via the
  // interpolation for the main (unvisited) property, so we don't need to
  // apply it twice.
  // TODO(crbug.com/1062217): Interpolate visited colors separately
  if (property.IsVisited()) {
    return;
  }
  DCHECK(priority->GetOrigin() >= CascadeOrigin::kAnimation);
  wtf_size_t index = DecodeInterpolationIndex(priority->GetPosition());
  DCHECK_LE(index, interpolations_.GetEntries().size());
  const ActiveInterpolationsMap& map = *interpolations_.GetEntries()[index].map;
  PropertyHandle handle = ToPropertyHandle(property, *priority);
  const auto& entry = map.find(handle);
  CHECK_NE(entry, map.end(), base::NotFatalUntil::M130);
  ApplyInterpolation(property, *priority, *entry->value, resolver);
}

bool StyleCascade::IsRootElement() const {
  return &state_.GetElement() == state_.GetDocument().documentElement();
}

StyleCascade::TokenSequence::TokenSequence(const CSSVariableData* data)
    : is_animation_tainted_(data->IsAnimationTainted()),
      has_font_units_(data->HasFontUnits()),
      has_root_font_units_(data->HasRootFontUnits()),
      has_line_height_units_(data->HasLineHeightUnits()) {}

bool StyleCascade::TokenSequence::AppendFallback(const TokenSequence& sequence,
                                                 bool is_attr_tainted,
                                                 wtf_size_t byte_limit) {
  // https://drafts.csswg.org/css-variables/#long-variables
  if (original_text_.length() + sequence.original_text_.length() > byte_limit) {
    return false;
  }
  size_t start = original_text_.length();

  StringView other_text = sequence.original_text_;
  other_text =
      CSSVariableParser::StripTrailingWhitespaceAndComments(other_text);

  CSSTokenizer tokenizer(other_text);
  CSSParserToken first_token = tokenizer.TokenizeSingleWithComments();

  if (NeedsInsertedComment(last_token_, first_token)) {
    original_text_.Append("/**/");
  }
  original_text_.Append(other_text);
  last_token_ = last_non_whitespace_token_ =
      sequence.last_non_whitespace_token_;

  is_animation_tainted_ |= sequence.is_animation_tainted_;
  has_font_units_ |= sequence.has_font_units_;
  has_root_font_units_ |= sequence.has_root_font_units_;
  has_line_height_units_ |= sequence.has_line_height_units_;

  size_t end = original_text_.length();
  if (is_attr_tainted) {
    attr_taint_ranges_.emplace_back(std::make_pair(start, end));
  }
  return true;
}

static bool IsNonWhitespaceToken(const CSSParserToken& token) {
  return token.GetType() != kWhitespaceToken &&
         token.GetType() != kCommentToken;
}

bool StyleCascade::TokenSequence::Append(StringView str,
                                         bool is_attr_tainted,
                                         wtf_size_t byte_limit) {
  // https://drafts.csswg.org/css-variables/#long-variables
  if (original_text_.length() + str.length() > byte_limit) {
    return false;
  }
  size_t start = original_text_.length();
  CSSTokenizer tokenizer(str);
  const CSSParserToken first_token = tokenizer.TokenizeSingleWithComments();
  if (first_token.GetType() != kEOFToken) {
    CSSVariableData::ExtractFeatures(first_token, has_font_units_,
                                     has_root_font_units_,
                                     has_line_height_units_);
    if (NeedsInsertedComment(last_token_, first_token)) {
      original_text_.Append("/**/");
    }
    last_token_ = first_token.CopyWithoutValue();
    if (IsNonWhitespaceToken(first_token)) {
      last_non_whitespace_token_ = first_token;
    }
    while (true) {
      const CSSParserToken token = tokenizer.TokenizeSingleWithComments();
      if (token.GetType() == kEOFToken) {
        break;
      } else {
        CSSVariableData::ExtractFeatures(token, has_font_units_,
                                         has_root_font_units_,
                                         has_line_height_units_);
        last_token_ = token.CopyWithoutValue();
        if (IsNonWhitespaceToken(token)) {
          last_non_whitespace_token_ = token;
        }
      }
    }
  }
  original_text_.Append(str);

  size_t end = original_text_.length();
  if (is_attr_tainted) {
    attr_taint_ranges_.emplace_back(std::make_pair(start, end));
  }
  return true;
}

bool StyleCascade::TokenSequence::Append(const CSSValue* value,
                                         bool is_attr_tainted,
                                         wtf_size_t byte_limit) {
  return Append(value->CssText(), is_attr_tainted, byte_limit);
}

bool StyleCascade::TokenSequence::Append(CSSVariableData* data,
                                         bool is_attr_tainted,
                                         wtf_size_t byte_limit) {
  if (!Append(data->OriginalText(), is_attr_tainted, byte_limit)) {
    return false;
  }
  is_animation_tainted_ |= data->IsAnimationTainted();
  return true;
}

void StyleCascade::TokenSequence::Append(const CSSParserToken& token,
                                         bool is_attr_tainted,
                                         StringView original_text) {
  CSSVariableData::ExtractFeatures(token, has_font_units_, has_root_font_units_,
                                   has_line_height_units_);
  size_t start = original_text_.length();
  if (NeedsInsertedComment(last_token_, token)) {
    original_text_.Append("/**/");
  }
  last_token_ = token.CopyWithoutValue();
  if (IsNonWhitespaceToken(token)) {
    last_non_whitespace_token_ = token;
  }
  original_text_.Append(original_text);
  size_t end = original_text_.length();
  if (is_attr_tainted) {
    attr_taint_ranges_.emplace_back(std::make_pair(start, end));
  }
}

bool StyleCascade::TokenSequence::Append(TokenSequence& sequence,
                                         bool is_attr_tainted,
                                         wtf_size_t byte_limit) {
  if (!Append(sequence.OriginalText(),
              is_attr_tainted || !sequence.GetAttrTaintedRanges()->empty(),
              byte_limit)) {
    return false;
  }
  is_animation_tainted_ |= sequence.is_animation_tainted_;
  return true;
}

CSSVariableData* StyleCascade::TokenSequence::BuildVariableData() {
  return CSSVariableData::Create(
      original_text_, is_animation_tainted_, !attr_taint_ranges_.empty(),
      /*needs_variable_resolution=*/false, has_font_units_,
      has_root_font_units_, has_line_height_units_);
}

const CSSValue* StyleCascade::Resolve(const CSSProperty& property,
                                      const CSSValue& value,
                                      CascadePriority priority,
                                      CascadeOrigin& origin,
                                      CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  const CSSValue* result = ResolveSubstitutions(property, value, resolver);
  DCHECK(result);

  if (result->IsRevertValue()) {
    return ResolveRevert(property, *result, origin, resolver);
  }
  if (result->IsRevertLayerValue() || TreatAsRevertLayer(priority)) {
    return ResolveRevertLayer(property, priority, origin, resolver);
  }
  if (const auto* v = DynamicTo<CSSFlipRevertValue>(result)) {
    return ResolveFlipRevert(property, *v, priority, origin, resolver);
  }
  if (const auto* v = DynamicTo<CSSMathFunctionValue>(result)) {
    return ResolveMathFunction(property, *v, priority);
  }

  resolver.CollectFlags(property, origin);

  return result;
}

const CSSValue* StyleCascade::ResolveSubstitutions(const CSSProperty& property,
                                                   const CSSValue& value,
                                                   CascadeResolver& resolver) {
  if (const auto* v = DynamicTo<CSSUnparsedDeclarationValue>(value)) {
    if (property.GetCSSPropertyName().IsCustomProperty()) {
      return ResolveCustomProperty(property, *v, resolver);
    } else {
      return ResolveVariableReference(property, *v, resolver);
    }
  }
  if (const auto* v = DynamicTo<cssvalue::CSSPendingSubstitutionValue>(value)) {
    return ResolvePendingSubstitution(property, *v, resolver);
  }
  return &value;
}

const CSSValue* StyleCascade::ResolveCustomProperty(
    const CSSProperty& property,
    const CSSUnparsedDeclarationValue& decl,
    CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());

  DCHECK(!resolver.IsLocked(property));
  CascadeResolver::AutoLock lock(property, resolver);

  CSSVariableData* data = decl.VariableDataValue();

  if (data->NeedsVariableResolution()) {
    data = ResolveVariableData(data, *GetParserContext(decl),
                               /*function_context=*/nullptr, resolver);
  }

  if (HasFontSizeDependency(To<CustomProperty>(property), data)) {
    resolver.DetectCycle(GetCSSPropertyFontSize());
  }

  if (HasLineHeightDependency(To<CustomProperty>(property), data)) {
    resolver.DetectCycle(GetCSSPropertyLineHeight());
  }

  if (resolver.InCycle()) {
    return CSSCyclicVariableValue::Create();
  }

  if (!data) {
    return CSSInvalidVariableValue::Create();
  }

  if (data == decl.VariableDataValue()) {
    return &decl;
  }

  // If a declaration, once all var() functions are substituted in, contains
  // only a CSS-wide keyword (and possibly whitespace), its value is determined
  // as if that keyword were its specified value all along.
  //
  // https://drafts.csswg.org/css-variables/#substitute-a-var
  {
    CSSParserTokenStream stream(data->OriginalText());
    stream.ConsumeWhitespace();
    CSSValue* value = css_parsing_utils::ConsumeCSSWideKeyword(stream);
    if (value && stream.AtEnd()) {
      return value;
    }
  }

  return MakeGarbageCollected<CSSUnparsedDeclarationValue>(
      data, decl.ParserContext());
}

const CSSValue* StyleCascade::ResolveVariableReference(
    const CSSProperty& property,
    const CSSUnparsedDeclarationValue& value,
    CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());
  DCHECK(!resolver.IsLocked(property));
  CascadeResolver::AutoLock lock(property, resolver);

  const CSSVariableData* data = value.VariableDataValue();
  const CSSParserContext* context = GetParserContext(value);

  MarkHasVariableReference(property);

  DCHECK(data);
  DCHECK(context);

  TokenSequence sequence;

  CSSParserTokenStream stream(data->OriginalText());
  if (ResolveTokensInto(stream, resolver, *context,
                        /* function_context */ nullptr,
                        /* stop_type */ kEOFToken, sequence)) {
    // TODO(sesse): It would be nice if we had some way of combining
    // ResolveTokensInto() and the re-tokenization. This is basically
    // what we pay by using the streaming parser everywhere; we tokenize
    // everything involving variable references twice.
    CSSParserTokenStream stream2(sequence.OriginalText(),
                                 sequence.GetAttrTaintedRanges());
    if (const auto* parsed = Parse(property, stream2, context)) {
      return parsed;
    }
  }

  return cssvalue::CSSUnsetValue::Create();
}

const CSSValue* StyleCascade::ResolvePendingSubstitution(
    const CSSProperty& property,
    const cssvalue::CSSPendingSubstitutionValue& value,
    CascadeResolver& resolver) {
  DCHECK(!property.IsSurrogate());
  DCHECK(!resolver.IsLocked(property));
  CascadeResolver::AutoLock lock(property, resolver);

  DCHECK_NE(property.PropertyID(), CSSPropertyID::kVariable);

  MarkHasVariableReference(property);

  // If the previous call to ResolvePendingSubstitution parsed 'value', then
  // we don't need to do it again.
  bool is_cached = resolver.shorthand_cache_.value == &value;

  if (!is_cached) {
    CSSUnparsedDeclarationValue* shorthand_value = value.ShorthandValue();
    const auto* shorthand_data = shorthand_value->VariableDataValue();
    CSSPropertyID shorthand_property_id = value.ShorthandPropertyId();

    TokenSequence sequence;

    CSSParserTokenStream stream(shorthand_data->OriginalText());
    if (!ResolveTokensInto(stream, resolver,
                           *GetParserContext(*shorthand_value),
                           /* function_context */ nullptr,
                           /* stop_type */ kEOFToken, sequence)) {
      return cssvalue::CSSUnsetValue::Create();
    }

    HeapVector<CSSPropertyValue, 64> parsed_properties;

    // NOTE: We don't actually need the original text to be comment-stripped,
    // since we're not storing it in a custom property anywhere.
    CSSParserTokenStream stream2(sequence.OriginalText());
    if (!CSSPropertyParser::ParseValue(
            shorthand_property_id, /*allow_important_annotation=*/false,
            stream2, shorthand_value->ParserContext(), parsed_properties,
            StyleRule::RuleType::kStyle)) {
      return cssvalue::CSSUnsetValue::Create();
    }

    resolver.shorthand_cache_.value = &value;
    resolver.shorthand_cache_.parsed_properties = std::move(parsed_properties);
  }

  const auto& parsed_properties = resolver.shorthand_cache_.parsed_properties;

  // For -internal-visited-properties with CSSPendingSubstitutionValues,
  // the inner 'shorthand_property_id' will expand to a set of longhands
  // containing the unvisited equivalent. Hence, when parsing the
  // CSSPendingSubstitutionValue, we look for the unvisited property in
  // parsed_properties.
  const CSSProperty* unvisited_property =
      property.IsVisited() ? property.GetUnvisitedProperty() : &property;

  unsigned parsed_properties_count = parsed_properties.size();
  for (unsigned i = 0; i < parsed_properties_count; ++i) {
    const CSSProperty& longhand =
        CSSProperty::Get(parsed_properties[i].PropertyID());

    // When using var() in a css-logical shorthand (e.g. margin-inline),
    // the longhands here will also be logical.
    if (unvisited_property == &ResolveSurrogate(longhand)) {
      return &parsed_properties[i].Value();
    }
  }

  NOTREACHED();
}

const CSSValue* StyleCascade::ResolveRevert(const CSSProperty& property,
                                            const CSSValue& value,
                                            CascadeOrigin& origin,
                                            CascadeResolver& resolver) {
  MaybeUseCountRevert(value);

  CascadeOrigin target_origin = TargetOriginForRevert(origin);

  switch (target_origin) {
    case CascadeOrigin::kTransition:
    case CascadeOrigin::kNone:
      return cssvalue::CSSUnsetValue::Create();
    case CascadeOrigin::kUserAgent:
    case CascadeOrigin::kUser:
    case CascadeOrigin::kAuthorPresentationalHint:
    case CascadeOrigin::kAuthor:
    case CascadeOrigin::kAnimation: {
      const CascadePriority* p =
          map_.Find(property.GetCSSPropertyName(), target_origin);
      if (!p || !p->HasOrigin()) {
        origin = CascadeOrigin::kNone;
        return cssvalue::CSSUnsetValue::Create();
      }
      origin = p->GetOrigin();
      return Resolve(property, *ValueAt(match_result_, p->GetPosition()), *p,
                     origin, resolver);
    }
  }
}

const CSSValue* StyleCascade::ResolveRevertLayer(const CSSProperty& property,
                                                 CascadePriority priority,
                                                 CascadeOrigin& origin,
                                                 CascadeResolver& resolver) {
  const CascadePriority* p = map_.FindRevertLayer(
      property.GetCSSPropertyName(), priority.ForLayerComparison());
  if (!p) {
    origin = CascadeOrigin::kNone;
    return cssvalue::CSSUnsetValue::Create();
  }
  origin = p->GetOrigin();
  return Resolve(property, *ValueAt(match_result_, p->GetPosition()), *p,
                 origin, resolver);
}

const CSSValue* StyleCascade::ResolveFlipRevert(const CSSProperty& property,
                                                const CSSFlipRevertValue& value,
                                                CascadePriority priority,
                                                CascadeOrigin& origin,
                                                CascadeResolver& resolver) {
  const CSSProperty& to_property =
      ResolveSurrogate(CSSProperty::Get(value.PropertyID()));
  const CSSValue* unflipped =
      ResolveRevertLayer(to_property, priority, origin, resolver);
  // Note: the value is transformed *from* the property we're reverting *to*.
  const CSSValue* flipped = TryValueFlips::FlipValue(
      /* from_property */ to_property.PropertyID(), unflipped,
      value.Transform(), state_.StyleBuilder().GetWritingDirection());
  return Resolve(property, *flipped, priority, origin, resolver);
}

// Math functions can become invalid at computed-value time. Currently, this
// is only possible for invalid anchor*() functions.
//
// https://drafts.csswg.org/css-anchor-position-1/#anchor-valid
// https://drafts.csswg.org/css-anchor-position-1/#anchor-size-valid
const CSSValue* StyleCascade::ResolveMathFunction(
    const CSSProperty& property,
    const CSSMathFunctionValue& math_value,
    CascadePriority priority) {
  if (!math_value.HasAnchorFunctions()) {
    return &math_value;
  }

  const CSSLengthResolver& length_resolver = state_.CssToLengthConversionData();

  // Calling HasInvalidAnchorFunctions evaluates the anchor*() functions
  // inside the CSSMathFunctionValue. Evaluating anchor*() requires that we
  // have the correct AnchorEvaluator::Mode, so we need to set that just like
  // we do for during e.g. Left::ApplyValue, Right::ApplyValue, etc.
  AnchorScope anchor_scope(property.PropertyID(),
                           length_resolver.GetAnchorEvaluator());
  // HasInvalidAnchorFunctions actually evaluates any anchor*() queries
  // within the CSSMathFunctionValue, and this requires the TreeScope to
  // be populated.
  const auto* scoped_math_value = To<CSSMathFunctionValue>(
      EnsureScopedValue(GetDocument(), match_result_, priority, &math_value));
  if (scoped_math_value->HasInvalidAnchorFunctions(length_resolver)) {
    return cssvalue::CSSUnsetValue::Create();
  }
  return scoped_math_value;
}

CSSVariableData* StyleCascade::ResolveVariableData(
    CSSVariableData* data,
    const CSSParserContext& context,
    FunctionContext* function_context,
    CascadeResolver& resolver) {
  DCHECK(data && data->NeedsVariableResolution());

  TokenSequence sequence(data);

  CSSParserTokenStream stream(data->OriginalText());
  if (!ResolveTokensInto(stream, resolver, context, function_context,
                         /*stop_type=*/kEOFToken, sequence)) {
    return nullptr;
  }

  return sequence.BuildVariableData();
}

bool StyleCascade::ResolveTokensInto(CSSParserTokenStream& stream,
                                     CascadeResolver& resolver,
                                     const CSSParserContext& context,
                                     FunctionContext* function_context,
                                     CSSParserTokenType stop_type,
                                     TokenSequence& out) {
  bool success = true;
  int nesting_level = 0;
  while (true) {
    const CSSParserToken& token = stream.Peek();
    if (token.IsEOF()) {
      break;
    } else if (token.GetType() == stop_type && nesting_level == 0) {
      break;
    } else if (token.FunctionId() == CSSValueID::kVar) {
      CSSParserTokenStream::BlockGuard guard(stream);
      success &=
          ResolveVarInto(stream, resolver, context, function_context, out);
    } else if (token.FunctionId() == CSSValueID::kEnv) {
      CSSParserTokenStream::BlockGuard guard(stream);
      success &= ResolveEnvInto(stream, resolver, context, out);
    } else if (token.FunctionId() == CSSValueID::kAttr &&
               RuntimeEnabledFeatures::CSSAdvancedAttrFunctionEnabled()) {
      CSSParserTokenStream::BlockGuard guard(stream);
      state_.StyleBuilder().SetHasAttrFunction();
      success &=
          ResolveAttrInto(stream, resolver, context, function_context, out);
    } else if (token.FunctionId() ==
               CSSValueID::kInternalAutoBase) {
      CSSParserTokenStream::BlockGuard guard(stream);
      success &=
          ResolveAutoBaseInto(stream, resolver, context, out);
    } else if (token.FunctionId() == CSSValueID::kIf &&
               RuntimeEnabledFeatures::CSSInlineIfForStyleQueriesEnabled()) {
      CSSParserTokenStream::BlockGuard guard(stream);
      success &=
          ResolveIfInto(stream, resolver, context, function_context, out);
    } else if (token.GetType() == kFunctionToken &&
               CSSVariableParser::IsValidVariableName(token.Value()) &&
               RuntimeEnabledFeatures::CSSFunctionsEnabled()) {
      // User-defined CSS function.
      CSSParserTokenStream::BlockGuard guard(stream);
      success &= ResolveFunctionInto(token.Value(), stream, resolver, context,
                                     function_context, out);
    } else {
      if (token.GetBlockType() == CSSParserToken::kBlockStart) {
        ++nesting_level;
      } else if (token.GetBlockType() == CSSParserToken::kBlockEnd) {
        if (nesting_level == 0) {
          // Attempting to go outside our block.
          break;
        }
        --nesting_level;
      }
      wtf_size_t start = stream.Offset();
      stream.ConsumeRaw();
      wtf_size_t end = stream.Offset();

      // NOTE: This will include any comment tokens that ConsumeRaw()
      // skipped over; i.e., any comment will be attributed to the
      // token after it and any trailing comments will be skipped.
      // This is fine, because trailing comments (sans whitespace)
      // should be skipped anyway.
      out.Append(token, stream.IsAttrTainted(start, end),
                 stream.StringRangeAt(start, end - start));
    }
  }
  return success;
}

bool StyleCascade::ResolveVarInto(CSSParserTokenStream& stream,
                                  CascadeResolver& resolver,
                                  const CSSParserContext& context,
                                  FunctionContext* function_context,
                                  TokenSequence& out) {
  AtomicString var_name = ConsumeVariableName(stream);
  DCHECK(stream.AtEnd() || (stream.Peek().GetType() == kCommaToken));

  // If we have a fallback, we must process it to look for cycles,
  // even if we are not going to use the fallback.
  //
  // https://drafts.csswg.org/css-variables/#cycles
  TokenSequence fallback;
  bool has_fallback = false;
  // Note: has_comma may be `true` even for fallbacks which contain
  // invalid var(). This is needed for syntax validation of fallbacks for
  // registered custom properties.
  // TODO(crbug.com/372475301): Remove this, if possible.
  bool has_comma = false;
  if (ConsumeComma(stream)) {
    has_comma = true;
    stream.ConsumeWhitespace();
    has_fallback =
        ResolveTokensInto(stream, resolver, context, function_context,
                          /* stop_type */ kEOFToken, fallback);
    // Even if the above call to ResolveTokensInto caused a cycle
    // (resolver.InCycle()==true), we must proceed to look for cycles in the
    // non-fallback branch. For example, suppose we are currently resolving
    // the ', var(--z)' part of the following:
    //
    //  --x: var(--y, var(--z));
    //  --y: var(--x);
    //  --z: var(--x);
    //
    // The properties --x and --z would be detected as cyclic as a result,
    // but we also need to discover the cycle between --x and --y.
  }

  // Within a function context (i.e. when resolving values within the body of
  // an @function rule), var() must first look for local variables
  // and arguments.
  //
  // https://drafts.csswg.org/css-mixins-1/#locally-substitute-a-var
  if (function_context) {
    // Locals shadow arguments, which shadow custom properties
    // from the element.

    // Ensure that any local variable with a matching name is applied
    // (i.e. exists on function_context->locals).
    // TODO(crbug.com/325504770): This may create cycles.
    LookupAndApplyLocalVariable(var_name, resolver, context, *function_context);
    for (FunctionContext* frame = function_context; frame;
         frame = frame->parent) {
      if (std::optional<const CSSValue*> local_variable =
              FindOrNullopt(frame->locals, var_name)) {
        return ResolveArgumentOrLocalInto(
            local_variable.value(), stream, resolver, context,
            (has_fallback ? &fallback : nullptr), out);
      }
      // Note that there is no "lookup and apply" step for arguments; one
      // argument cannot reference another using var() or similar.
      if (std::optional<const CSSValue*> argument =
              FindOrNullopt(frame->arguments, var_name)) {
        return ResolveArgumentOrLocalInto(
            argument.value(), stream, resolver, context,
            (has_fallback ? &fallback : nullptr), out);
      }
    }
  }

  CustomProperty property(var_name, state_.GetDocument());

  // Any custom property referenced (by anything, even just once) in the
  // document can currently not be animated on the compositor. Hence we mark
  // properties that have been referenced.
  DCHECK(resolver.CurrentProperty());
  MarkIsReferenced(*resolver.CurrentProperty(), property);

  if (!resolver.DetectCycle(property)) {
    // We are about to substitute var(property). In order to do that, we must
    // know the computed value of 'property', hence we Apply it.
    //
    // We can however not do this if we're in a cycle. If a cycle is detected
    // here, it means we are already resolving 'property', and have discovered
    // a reference to 'property' during that resolution.
    LookupAndApply(property, resolver);
  }
  // Note that this check catches cycles detected by the DetectCycle call
  // immediately above, but also any cycles detected during processing of the
  // fallback near the start of this function.
  if (resolver.InCycle()) {
    return false;
  }

  CSSVariableData* data = GetVariableData(property);

  // If substitution is not allowed, treat the value as
  // invalid-at-computed-value-time.
  //
  // https://drafts.csswg.org/css-variables/#animation-tainted
  if (!resolver.AllowSubstitution(data)) {
    data = nullptr;
  }

  // The fallback must match the syntax of the referenced custom
  // property, even if the fallback isn't used.
  //
  // TODO(crbug.com/372475301): Remove this, if possible.
  //
  // https://drafts.css-houdini.org/css-properties-values-api-1/#fallbacks-in-var-references
  if (has_comma && !ValidateFallback(property, fallback.OriginalText())) {
    CountUse(WebFeature::kVarFallbackValidation);
    return false;
  }

  if (!data) {
    // No substitution value found; attempt fallback.
    if (has_fallback) {
      return out.AppendFallback(fallback,
                                !fallback.GetAttrTaintedRanges()->empty(),
                                CSSVariableData::kMaxVariableBytes);
    }
    return false;
  }

  return out.Append(data, data->IsAttrTainted(),
                    CSSVariableData::kMaxVariableBytes);
}

bool StyleCascade::ResolveFunctionInto(StringView function_name,
                                       CSSParserTokenStream& stream,
                                       CascadeResolver& resolver,
                                       const CSSParserContext& context,
                                       FunctionContext* function_context,
                                       TokenSequence& out) {
  state_.StyleBuilder().SetAffectedByCSSFunction();

  AtomicString function_name_atomic(function_name);
  using Function = CascadeResolver::Function;
  if (resolver.DetectCycle(Function(function_name_atomic))) {
    return false;
  }
  CascadeResolver::AutoLock lock(Function(function_name_atomic), resolver);

  // TODO(sesse): Deal with tree-scoped references.
  StyleRuleFunction* function = nullptr;
  if (GetDocument().GetScopedStyleResolver()) {
    function =
        GetDocument().GetScopedStyleResolver()->FunctionForName(function_name);
  }
  if (!function) {
    return false;
  }

  // Parse and resolve function arguments.
  HeapHashMap<String, Member<const CSSValue>> function_arguments;

  bool first_parameter = true;
  for (const StyleRuleFunction::Parameter& parameter :
       function->GetParameters()) {
    stream.ConsumeWhitespace();

    StringView argument_string;

    if (!stream.AtEnd() &&
        (first_parameter || stream.Peek().GetType() == kCommaToken)) {
      first_parameter = false;
      if (stream.Peek().GetType() == kCommaToken) {
        stream.ConsumeIncludingWhitespace();
      }
      // Handle {}-wrapper.
      // https://drafts.csswg.org/css-values-5/#component-function-commas
      if (stream.Peek().GetType() == kLeftBraceToken) {
        CSSParserTokenStream::BlockGuard guard(stream);
        stream.ConsumeWhitespace();
        DCHECK(!stream.AtEnd());
        argument_string = ConsumeUntilPeekedTypeIs<>(stream);
      } else {
        argument_string = ConsumeUntilPeekedTypeIs<kCommaToken>(stream);
      }
      DCHECK(!argument_string.empty());  // Handled parse-time.
    } else if (parameter.default_value) {
      argument_string = parameter.default_value->OriginalText();
    } else {
      // Argument was missing, with no default.
      return false;
    }

    DCHECK(!argument_string.empty());

    // We need to resolve the argument in the context of this function,
    // so that we can do type coercion on the resolved value before the call.
    // In particular, we want any var() within the argument to be resolved
    // in our context; e.g., --foo(var(--a)) should be our a, not foo's a
    // (if that even exists).
    //
    // Note that if this expression comes from directly a function call,
    // as in the example above (and if the return and argument types are the
    // same), we will effectively do type parsing of exactly the same data
    // twice. This is wasteful, and it's possible that we should do something
    // about it if it proves to be a common case.
    const CSSValue* argument_value = ResolveFunctionExpression(
        argument_string, parameter.type, resolver, context, function_context);
    if (argument_value == nullptr) {
      return false;
    }

    function_arguments.insert(parameter.name, argument_value);
  }

  if (!stream.AtEnd()) {
    // This could mean that we have more arguments than we have parameters,
    // which isn't allowed.
    return false;
  }

  const CSSUnparsedDeclarationValue* unresolved_result = nullptr;
  HeapHashMap<String, Member<const CSSValue>> unresolved_locals;

  // Flattens the function body, consisting of any number of
  // CSSFunctionDeclarations and conditional rules, into the final
  // (unresolved) values for 'result'/locals.
  FlattenFunctionBody(*function, unresolved_result, unresolved_locals);

  if (!unresolved_result) {
    return false;
  }

  FunctionContext local_function_context{
      .arguments = function_arguments,
      .locals = {},  // Populated by ApplyLocalVariables.
      .unresolved_locals = unresolved_locals,
      .parent = function_context};

  ApplyLocalVariables(resolver, context, local_function_context);

  // Applying local variables may place this function in a cycle.
  if (resolver.InCycle()) {
    return false;
  }

  const CSSValue* ret_value = ResolveFunctionExpression(
      unresolved_result->VariableDataValue()->OriginalText(),
      function->GetReturnType(), resolver, context, &local_function_context);
  if (ret_value == nullptr) {
    return false;
  }
  // TODO(crbug.com/325504770): Urggg
  String ret_string = ret_value->CssText();
  CSSParserTokenStream ret_value_stream(ret_string);
  return ResolveTokensInto(ret_value_stream, resolver, context,
                           /* function_context */ nullptr,
                           /* stop_type */ kEOFToken, out);
}

bool StyleCascade::ResolveArgumentOrLocalInto(const CSSValue* value,
                                              CSSParserTokenStream& stream,
                                              CascadeResolver& resolver,
                                              const CSSParserContext& context,
                                              const TokenSequence* fallback,
                                              TokenSequence& out) {
  // Note: `value` may be nullptr when a locals variable became invalid
  // due to e.g. failed substitutions.
  bool success = false;
  if (value) {
    // TODO(crbug.com/393924687): There is nothing to resolve at this point.
    // Just append the CSSVariableData directly.
    String value_str = value->CssText();
    CSSParserTokenStream value_stream(value_str);
    success = ResolveTokensInto(value_stream, resolver, context,
                                /*function_context=*/nullptr,
                                /*stop_type=*/kEOFToken, out);
  }
  if (!success && fallback) {
    success = out.AppendFallback(*fallback,
                                 !fallback->GetAttrTaintedRanges()->empty(),
                                 CSSVariableData::kMaxVariableBytes);
  }
  return success;
}

// Resolves an expression within a function; in practice, either a function
// argument or its return value. In practice, this is about taking a string
// and coercing it into the given type -- and then the caller will convert it
// right back to a string again. This is pretty suboptimal, but it's the way
// registered properties also work, and crucially, without such a resolve step
// (which needs a type), we would not be able to collapse calc() expressions
// and similar, which could cause massive blowup as the values are passed
// through a large tree of function calls.
const CSSValue* StyleCascade::ResolveFunctionExpression(
    StringView expr,
    const CSSSyntaxDefinition& type,
    CascadeResolver& resolver,
    const CSSParserContext& context,
    FunctionContext* function_context) {
  TokenSequence resolved_expr;

  CSSParserTokenStream argument_stream(expr);
  if (!ResolveTokensInto(argument_stream, resolver, context, function_context,
                         /* stop_type */ kEOFToken, resolved_expr)) {
    return nullptr;
  }

  const CSSValue* value = type.Parse(resolved_expr.OriginalText(), context,
                                     /*is_animation_tainted=*/false);
  if (!value) {
    return nullptr;
  }

  // TODO(crbug.com/325504770): We need to return a CSSUnparsedDeclarationValue
  // (or CSSVariableData) from this function. We're currently losing
  // tainting information held by TokenSequence.

  // Resolve the value as if it were a registered property, to get rid of
  // extraneous calc(), resolve lengths and so on.
  return &StyleBuilderConverter::ConvertRegisteredPropertyValue(state_, *value,
                                                                &context);
}

void StyleCascade::ApplyLocalVariables(CascadeResolver& resolver,
                                       const CSSParserContext& context,
                                       FunctionContext& function_context) {
  for (const auto& [name, value] : function_context.unresolved_locals) {
    if (function_context.locals.find(name) != function_context.locals.end()) {
      // Already applied. This can happen because a call to ResolveLocalVariable
      // may trigger application of other local variables via var().
    }
    const CSSValue* resolved = ResolveLocalVariable(
        AtomicString(name), *value, resolver, context, function_context);
    // Note: The following call may insert an explicit nullptr;
    // this is intentional.
    function_context.locals.insert(name, resolved);
  }
}

void StyleCascade::LookupAndApplyLocalVariable(
    const String& name,
    CascadeResolver& resolver,
    const CSSParserContext& context,
    FunctionContext& function_context) {
  auto resolved_it = function_context.locals.find(name);
  if (resolved_it != function_context.locals.end()) {
    // Already applied.
    return;
  }

  auto unresolved_it = function_context.unresolved_locals.find(name);
  if (unresolved_it == function_context.unresolved_locals.end()) {
    // Does not exist.
    return;
  }

  const CSSValue* resolved =
      ResolveLocalVariable(AtomicString(name), *unresolved_it->value, resolver,
                           context, function_context);
  // Note: we may insert an explicit nullptr here; this is intentional.
  function_context.locals.insert(name, resolved);
}

const CSSValue* StyleCascade::ResolveLocalVariable(
    const AtomicString& name,
    const CSSValue& unresolved,
    CascadeResolver& resolver,
    const CSSParserContext& context,
    FunctionContext& function_context) {
  using LocalVariable = CascadeResolver::LocalVariable;
  if (resolver.DetectCycle(LocalVariable(name))) {
    return nullptr;
  }
  CascadeResolver::AutoLock lock(LocalVariable(name), resolver);
  CSSVariableData* data =
      To<CSSUnparsedDeclarationValue>(unresolved).VariableDataValue();
  if (data->NeedsVariableResolution()) {
    data = ResolveVariableData(data, context, &function_context, resolver);
  }
  if (!data) {
    return nullptr;
  }
  // TODO: Work with CSSVariableData directly.
  return MakeGarbageCollected<CSSUnparsedDeclarationValue>(data, &context);
}

void StyleCascade::FlattenFunctionBody(
    StyleRuleGroup& group,
    const CSSUnparsedDeclarationValue*& result,
    HeapHashMap<String, Member<const CSSValue>>& locals) {
  for (const Member<StyleRuleBase>& child : group.ChildRules()) {
    if (auto* function_declarations =
            DynamicTo<StyleRuleFunctionDeclarations>(child.Get())) {
      const CSSPropertyValueSet& propety_value_set =
          function_declarations->Properties();
      for (const CSSPropertyValue& property_value :
           propety_value_set.Properties()) {
        if (property_value.PropertyID() == CSSPropertyID::kVariable) {
          const auto& unresolved_local =
              To<CSSUnparsedDeclarationValue>(property_value.Value());
          locals.Set(property_value.CustomPropertyName(), &unresolved_local);
        }
      }
      if (auto* r = DynamicTo<CSSUnparsedDeclarationValue>(
              propety_value_set.GetPropertyCSSValue(CSSPropertyID::kResult))) {
        result = r;
      }
    } else if (auto* supports_rule =
                   DynamicTo<StyleRuleSupports>(child.Get())) {
      if (supports_rule->ConditionIsSupported()) {
        FlattenFunctionBody(*supports_rule, result, locals);
      }
    } else if (auto* media_rule = DynamicTo<StyleRuleMedia>(child.Get())) {
      state_.StyleBuilder().SetAffectedByFunctionalMedia();
      if (GetDocument().GetStyleEngine().EvaluateFunctionalMediaQuery(
              *media_rule->MediaQueries())) {
        FlattenFunctionBody(*media_rule, result, locals);
      }
    } else if (auto* container_rule =
                   DynamicTo<StyleRuleContainer>(child.Get())) {
      // TODO(crbug.com/394353319): Rename this flag to accommodate any
      // CQ-dependent value.
      state_.StyleBuilder().SetHasContainerRelativeUnits();
      // TODO(crbug.com/394500599): This is the wrong tree-scope; we should use
      // the tree scope of the current declaration.
      TreeScope& tree_scope = state_.GetElement().GetTreeScope();
      if (EvaluateContainerQuery(state_.GetElement(), state_.GetPseudoId(),
                                 container_rule->GetContainerQuery(),
                                 tree_scope, state_.NearestSizeContainer(),
                                 match_result_)) {
        FlattenFunctionBody(*container_rule, result, locals);
      }
    }
  }
}

bool StyleCascade::ResolveEnvInto(CSSParserTokenStream& stream,
                                  CascadeResolver& resolver,
                                  const CSSParserContext& context,
                                  TokenSequence& out) {
  state_.StyleBuilder().SetHasEnv();
  AtomicString variable_name = ConsumeVariableName(stream);

  if (variable_name == "safe-area-inset-bottom") {
    state_.StyleBuilder().SetHasEnvSafeAreaInsetBottom();
    state_.GetDocument()
        .GetStyleEngine()
        .SetNeedsToUpdateComplexSafeAreaConstraints();
  }

  DCHECK(stream.AtEnd() || (stream.Peek().GetType() == kCommaToken) ||
         (stream.Peek().GetType() == kNumberToken));

  WTF::Vector<unsigned> indices;
  if (!stream.AtEnd() && stream.Peek().GetType() != kCommaToken) {
    do {
      const CSSParserToken& token = stream.ConsumeIncludingWhitespaceRaw();
      DCHECK(token.GetNumericValueType() == kIntegerValueType);
      DCHECK(token.NumericValue() >= 0.);
      indices.push_back(static_cast<unsigned>(token.NumericValue()));
    } while (stream.Peek().GetType() == kNumberToken);
  }

  DCHECK(stream.AtEnd() || (stream.Peek().GetType() == kCommaToken));

  CSSVariableData* data =
      GetEnvironmentVariable(variable_name, std::move(indices));

  if (!data) {
    if (ConsumeComma(stream)) {
      return ResolveTokensInto(stream, resolver, context,
                               /* function_context */ nullptr,
                               /* stop_type */ kEOFToken, out);
    }
    return false;
  }

  return out.Append(data, data->IsAttrTainted());
}

bool StyleCascade::ResolveAttrInto(CSSParserTokenStream& stream,
                                   CascadeResolver& resolver,
                                   const CSSParserContext& context,
                                   FunctionContext* function_context,
                                   TokenSequence& out) {
  AtomicString local_name = ConsumeVariableName(stream);
  using Attribute = CascadeResolver::Attribute;
  if (resolver.DetectCycle(Attribute(local_name))) {
    return false;
  }
  CascadeResolver::AutoLock lock(Attribute(local_name), resolver);
  std::optional<CSSAttrType> attr_type = CSSAttrType::Consume(stream);
  if (!attr_type.has_value()) {
    attr_type = CSSAttrType::GetDefaultValue();
  }
  DCHECK(stream.AtEnd() || stream.Peek().GetType() == kCommaToken);

  Element& element = state_.GetUltimateOriginatingElementOrSelf();

  // TODO(crbug.com/387281256): Support namespaces.
  const String& attribute_value = element.getAttributeNS(
      /*namespace_uri=*/g_null_atom, element.LowercaseIfNecessary(local_name));

  String substituted_attribute_value = attribute_value;
  if (!attribute_value.IsNull() && !attr_type->IsString()) {
    TokenSequence substituted_attribute_token_sequence;
    CSSParserTokenStream attribute_value_stream(attribute_value);
    if (!ResolveTokensInto(
            attribute_value_stream, resolver, context, function_context,
            /* stop_type */ kEOFToken, substituted_attribute_token_sequence)) {
      return false;
    }
    substituted_attribute_value =
        substituted_attribute_token_sequence.OriginalText();
  }

  const CSSValue* substitution_value =
      (!attribute_value || !substituted_attribute_value)
          ? nullptr
          : attr_type->Parse(substituted_attribute_value, context);

  if (ConsumeComma(stream)) {
    stream.ConsumeWhitespace();

    TokenSequence fallback;
    if (!ResolveTokensInto(stream, resolver, context, function_context,
                           /* stop_type */ kEOFToken, fallback)) {
      return false;
    }
    if (!substitution_value) {
      return out.AppendFallback(fallback, /* is_attr_tainted */ true,
                                CSSVariableData::kMaxVariableBytes);
    }
  }

  if (attr_type->IsString() && !substitution_value) {
    // If the <attr-type> argument is omitted, the fallback defaults to the
    // empty string if omitted.
    // https://drafts.csswg.org/css-values-5/#attr-notation
    out.Append(CSSParserToken(kStringToken, g_empty_atom),
               /* is_attr_tainted */ true, g_empty_atom);
    return true;
  }

  if (substitution_value) {
    out.Append(substitution_value, /* is_attr_tainted */ true,
               CSSVariableData::kMaxVariableBytes);
    return true;
  }

  return false;
}

bool StyleCascade::ResolveAutoBaseInto(
    CSSParserTokenStream& stream,
    CascadeResolver& resolver,
    const CSSParserContext& context,
    TokenSequence& out) {
  const CSSProperty& appearance = GetCSSPropertyAppearance();
  if (resolver.DetectCycle(appearance)) {
    return false;
  }
  LookupAndApply(appearance, resolver);

  // Note that the InBaseSelectAppearance() flag is set by StyleAdjuster,
  // which hasn't happened yet. Therefore we also need to check
  // HasBaseSelectAppearance() here.
  bool has_base_appearance = state_.StyleBuilder().HasBaseSelectAppearance() ||
                             state_.StyleBuilder().InBaseSelectAppearance();

  if (has_base_appearance) {
    // We want to the second argument.
    stream.SkipUntilPeekedTypeIs<kCommaToken>();
    CHECK(!stream.AtEnd());
    stream.ConsumeIncludingWhitespace();  // kCommaToken
  }

  return ResolveTokensInto(stream, resolver, context,
                           /* function_context */ nullptr,
                           /* stop_type */ kCommaToken, out);
}

bool StyleCascade::EvalIfInitial(CSSVariableData* value,
                                 const CustomProperty& property) {
  if (!property.IsRegistered()) {
    return !value;
  }
  const StyleInitialData* initial_data = state_.StyleBuilder().InitialData();
  DCHECK(initial_data);
  CSSVariableData* initial_variable_data =
      initial_data->GetVariableData(property.GetPropertyNameAtomicString());
  return value->EqualsIgnoringAttrTainting(*initial_variable_data);
}

bool StyleCascade::EvalIfInherit(CSSVariableData* value,
                                 const CustomProperty& property) {
  if (!state_.ParentStyle()) {
    return EvalIfInitial(value, property);
  }

  bool is_inherited_property = property.IsInherited();

  CSSVariableData* parent_data = state_.ParentStyle()->GetVariableData(
      property.GetPropertyNameAtomicString(), is_inherited_property);

  return value->EqualsIgnoringAttrTainting(*parent_data);
}

bool StyleCascade::EvalIfKeyword(const CSSValue& keyword_value,
                                 CSSVariableData* value,
                                 const CustomProperty& property) {
  if (keyword_value.IsInitialValue()) {
    return EvalIfInitial(value, property);
  }

  if (keyword_value.IsInheritedValue()) {
    return EvalIfInherit(value, property);
  }

  if (keyword_value.IsUnsetValue()) {
    if (state_.IsInheritedForUnset(property)) {
      return EvalIfInherit(value, property);
    } else {
      return EvalIfInitial(value, property);
    }
  }

  // revert and revert-layer keywords
  return false;
}

KleeneValue StyleCascade::EvalIfStyleFeature(
    const MediaQueryFeatureExpNode& feature,
    CascadeResolver& resolver,
    const CSSParserContext& context,
    FunctionContext* function_context,
    bool& is_attr_tainted) {
  const MediaQueryExpBounds& bounds = feature.Bounds();

  // Style features do not support the range syntax.
  DCHECK(!bounds.IsRange());
  DCHECK(bounds.right.op == MediaQueryOperator::kNone);

  AtomicString property_name(feature.Name());
  CustomProperty property(property_name, GetDocument());

  CSSVariableData* computed_data = nullptr;
  CSSParserTokenStream property_name_stream(property_name);
  TokenSequence computed_data_sequence;
  // To avoid duplicating lookup logic, we pretend that we're resolving
  // a var() with `property_name`. This will resolve to the appropriate
  // custom property, local variable, or function argument. We also get
  // cycle handling for free.
  if (ResolveVarInto(property_name_stream, resolver, context, function_context,
                     computed_data_sequence)) {
    computed_data = computed_data_sequence.BuildVariableData();
  }

  if (resolver.InCycle()) {
    return KleeneValue::kFalse;
  }

  if (computed_data && computed_data->IsAttrTainted()) {
    is_attr_tainted = true;
  }

  if (!bounds.right.value.IsValid()) {
    return computed_data ? KleeneValue::kTrue : KleeneValue::kFalse;
  }

  const CSSValue& query_specified = bounds.right.value.GetCSSValue();

  if (query_specified.IsCSSWideKeyword()) {
    return EvalIfKeyword(query_specified, computed_data, property)
               ? KleeneValue::kTrue
               : KleeneValue::kFalse;
  }

  if (!computed_data) {
    return KleeneValue::kFalse;
  }

  const auto& decl_value = To<CSSUnparsedDeclarationValue>(query_specified);

  CSSParserTokenStream decl_value_stream(
      decl_value.VariableDataValue()->OriginalText());
  TokenSequence substituted_token_sequence;
  if (!ResolveTokensInto(decl_value_stream, resolver, context, function_context,
                         /* stop_type */ kEOFToken,
                         substituted_token_sequence)) {
    return KleeneValue::kFalse;
  }

  CSSVariableData* computed_query_data =
      substituted_token_sequence.BuildVariableData();

  if (property.IsRegistered()) {
    const CSSValue* parsed_value =
        property.Parse(substituted_token_sequence.OriginalText(), context,
                       CSSParserLocalContext());
    if (!parsed_value) {
      return KleeneValue::kFalse;
    }
    const CSSValue& computed_query_value =
        StyleBuilderConverter::ConvertRegisteredPropertyValue(
            state_, *parsed_value, decl_value.ParserContext());
    computed_query_data =
        StyleBuilderConverter::ConvertRegisteredPropertyVariableData(
            computed_query_value, /* is_animation_tainted */ false,
            computed_query_data->IsAttrTainted());
  }

  if (computed_query_data->IsAttrTainted()) {
    is_attr_tainted = true;
  }

  if (computed_data->EqualsIgnoringAttrTainting(*computed_query_data)) {
    return KleeneValue::kTrue;
  }

  return KleeneValue::kFalse;
}

bool StyleCascade::EvalIfCondition(CSSParserTokenStream& stream,
                                   CascadeResolver& resolver,
                                   const CSSParserContext& context,
                                   FunctionContext* function_context,
                                   bool& is_attr_tainted) {
  if (stream.Peek().Id() == CSSValueID::kElse) {
    stream.ConsumeIncludingWhitespace();
    DCHECK_EQ(stream.Peek().GetType(), kColonToken);
    stream.ConsumeIncludingWhitespace();
    return true;
  }

  CSSIfParser parser(context);

  std::optional<IfTest> if_test = parser.ConsumeIfTest(stream);
  DCHECK(if_test.has_value());

  // style()
  if (const MediaQueryExpNode* style_test = if_test->GetStyleTest()) {
    stream.ConsumeWhitespace();
    DCHECK_EQ(stream.Peek().GetType(), kColonToken);
    stream.ConsumeIncludingWhitespace();

    return MediaEval(*style_test, [this, &resolver, &context, &function_context,
                                   &is_attr_tainted](
                                      const MediaQueryFeatureExpNode& feature) {
             return EvalIfStyleFeature(feature, resolver, context,
                                       function_context, is_attr_tainted);
           }) == KleeneValue::kTrue;
  }

  // TODO(crbug.com/346977961): Support media() and supports() queries in if()
  // condition.
  return false;
}

bool StyleCascade::ResolveIfInto(CSSParserTokenStream& stream,
                                 CascadeResolver& resolver,
                                 const CSSParserContext& context,
                                 FunctionContext* function_context,
                                 TokenSequence& out) {
  stream.ConsumeWhitespace();
  bool is_attr_tainted = false;
  bool eval_result = EvalIfCondition(stream, resolver, context,
                                     function_context, is_attr_tainted);
  while (!eval_result) {
    stream.SkipUntilPeekedTypeIs<kSemicolonToken>();
    if (stream.AtEnd()) {
      // None of the conditions matched, so should be IACVT.
      return false;
    }
    stream.ConsumeIncludingWhitespace();  // kSemicolonToken
    if (stream.AtEnd()) {
      // None of the conditions matched, so should be IACVT.
      return false;
    }
    eval_result = EvalIfCondition(stream, resolver, context, function_context,
                                  is_attr_tainted);
  }
  TokenSequence if_result;
  if (!ResolveTokensInto(stream, resolver, context, function_context,
                         /* stop_type */ kSemicolonToken, if_result)) {
    return false;
  }

  return out.Append(if_result, is_attr_tainted);
}

CSSVariableData* StyleCascade::GetVariableData(
    const CustomProperty& property) const {
  const AtomicString& name = property.GetPropertyNameAtomicString();
  const bool is_inherited = property.IsInherited();
  return state_.StyleBuilder().GetVariableData(name, is_inherited);
}

CSSVariableData* StyleCascade::GetEnvironmentVariable(
    const AtomicString& name,
    WTF::Vector<unsigned> indices) const {
  // If we are in a User Agent Shadow DOM then we should not record metrics.
  ContainerNode& scope_root = state_.GetElement().GetTreeScope().RootNode();
  auto* shadow_root = DynamicTo<ShadowRoot>(&scope_root);
  bool is_ua_scope = shadow_root && shadow_root->IsUserAgent();

  return state_.GetDocument()
      .GetStyleEngine()
      .EnsureEnvironmentVariables()
      .ResolveVariable(name, std::move(indices), !is_ua_scope);
}

const CSSParserContext* StyleCascade::GetParserContext(
    const CSSUnparsedDeclarationValue& value) {
  // TODO(crbug.com/985028): CSSUnparsedDeclarationValue should always have a
  // CSSParserContext. (CSSUnparsedValue violates this).
  if (value.ParserContext()) {
    return value.ParserContext();
  }
  return StrictCSSParserContext(
      state_.GetDocument().GetExecutionContext()->GetSecureContextMode());
}

bool StyleCascade::HasFontSizeDependency(const CustomProperty& property,
                                         CSSVariableData* data) const {
  if (!property.IsRegistered() || !data) {
    return false;
  }
  if (data->HasFontUnits() || data->HasLineHeightUnits()) {
    return true;
  }
  if (data->HasRootFontUnits() && IsRootElement()) {
    return true;
  }
  return false;
}

bool StyleCascade::HasLineHeightDependency(const CustomProperty& property,
                                           CSSVariableData* data) const {
  if (!property.IsRegistered() || !data) {
    return false;
  }
  if (data->HasLineHeightUnits()) {
    return true;
  }
  return false;
}

bool StyleCascade::ValidateFallback(const CustomProperty& property,
                                    StringView value) const {
  if (!property.IsRegistered()) {
    return true;
  }
  auto context_mode =
      state_.GetDocument().GetExecutionContext()->GetSecureContextMode();
  auto* context = StrictCSSParserContext(context_mode);
  auto local_context = CSSParserLocalContext();
  return property.Parse(value, *context, local_context);
}

void StyleCascade::MarkIsReferenced(const CSSProperty& referencer,
                                    const CustomProperty& referenced) {
  if (!referenced.IsRegistered()) {
    return;
  }
  const AtomicString& name = referenced.GetPropertyNameAtomicString();
  state_.GetDocument().EnsurePropertyRegistry().MarkReferenced(name);
}

void StyleCascade::MarkHasVariableReference(const CSSProperty& property) {
  state_.StyleBuilder().SetHasVariableReference();
}

void StyleCascade::ApplyUnresolvedEnv() {
  // Currently the only field that depends on parsing unresolved env().
  ApplyIsBottomRelativeToSafeAreaInset();
}

void StyleCascade::ApplyIsBottomRelativeToSafeAreaInset() {
  if (!state_.StyleBuilder().HasEnvSafeAreaInsetBottom() ||
      !map_.NativeBitset().Has(CSSPropertyID::kBottom)) {
    return;
  }

  const CascadePriority* p = map_.FindKnownToExist(CSSPropertyID::kBottom);
  if (p->GetOrigin() >= CascadeOrigin::kAnimation) {
    // Effect values from animations/transition do not contain env().
    return;
  }

  const CSSValue* value = ValueAt(match_result_, p->GetPosition());
  const auto* unparsed = DynamicTo<CSSUnparsedDeclarationValue>(value);
  if (!unparsed) {
    return;  // Does not contain env().
  }

  // IsSafeAreaInsetBottom assumes the fallback is not taken.
  DCHECK(GetEnvironmentVariable(AtomicString("safe-area-inset-bottom"),
                                /*indices=*/WTF::Vector<unsigned>()));

  if (CSSParserFastPaths::IsSafeAreaInsetBottom(
          unparsed->VariableDataValue()->OriginalText())) {
    state_.StyleBuilder().SetIsBottomRelativeToSafeAreaInset(true);

    UseCounter::Count(
        state_.GetDocument(),
        WebFeature::kCSSEnvironmentVariable_SafeAreaInsetBottom_FastPath);
  }
}

bool StyleCascade::TreatAsRevertLayer(CascadePriority priority) const {
  return priority.IsTryStyle() && !ComputedStyle::HasOutOfFlowPosition(
                                      state_.StyleBuilder().GetPosition());
}

const Document& StyleCascade::GetDocument() const {
  return state_.GetDocument();
}

const CSSProperty& StyleCascade::ResolveSurrogate(const CSSProperty& property) {
  if (!property.IsSurrogate()) {
    return property;
  }
  // This marks the cascade as dependent on cascade-affecting properties
  // even for simple surrogates like -webkit-writing-mode, but there isn't
  // currently a flag to distinguish such surrogates from e.g. css-logical
  // properties.
  depends_on_cascade_affecting_property_ = true;
  const CSSProperty* original =
      property.SurrogateFor(state_.StyleBuilder().GetWritingDirection());
  DCHECK(original);
  return *original;
}

void StyleCascade::CountUse(WebFeature feature) {
  GetDocument().CountUse(feature);
}

void StyleCascade::MaybeUseCountRevert(const CSSValue& value) {
  if (value.IsRevertValue()) {
    CountUse(WebFeature::kCSSKeywordRevert);
  }
}

}  // namespace blink
