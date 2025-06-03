/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COMPUTED_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COMPUTED_STYLE_H_

#include <algorithm>
#include <memory>

#include "base/check_op.h"
#include "base/gtest_prod_util.h"
#include "base/memory/values_equivalent.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/color_scheme_flags.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/css_bitset.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/resolver/matched_properties_cache.h"
#include "third_party/blink/renderer/core/css/style_auto_color.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/layout/geometry/box_sides.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_type.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/style/border_value.h"
#include "third_party/blink/renderer/core/style/computed_style_base.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/computed_style_initial_values.h"
#include "third_party/blink/renderer/core/style/cursor_list.h"
#include "third_party/blink/renderer/core/style/display_style.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/core/style/font_size_style.h"
#include "third_party/blink/renderer/core/style/style_cached_data.h"
#include "third_party/blink/renderer/core/style/style_highlight_data.h"
#include "third_party/blink/renderer/core/style/style_scrollbar_color.h"
#include "third_party/blink/renderer/core/style/transform_origin.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/geometry/length_box.h"
#include "third_party/blink/renderer/platform/geometry/length_point.h"
#include "third_party/blink/renderer/platform/geometry/length_size.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"
#include "third_party/blink/renderer/platform/text/writing_mode_utils.h"
#include "third_party/blink/renderer/platform/transforms/transform_operations.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/leak_annotations.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace gfx {
class Transform;
}

namespace blink {

using std::max;

class AppliedTextDecoration;
class BorderEdge;
class ContentData;
class CounterDirectives;
class CSSAnimationData;
class FloodColor;
class CSSTransitionData;
class CSSVariableData;
class Font;
class Hyphenation;
class LayoutBox;
class LayoutTheme;
class Longhand;
class NinePieceImage;
class ShadowList;
class ShapeValue;
class StyleAdjuster;
class StyleContentAlignmentData;
class StyleDifference;
class StyleImage;
class StyleInheritedVariables;
class StyleInitialData;
class StyleRay;
class StyleResolver;
class StyleResolverState;
class StyleSelfAlignmentData;

namespace css_longhand {

class AccentColor;
class BackgroundColor;
class BorderBottomColor;
class BorderLeftColor;
class BorderRightColor;
class BorderTopColor;
class CaretColor;
class Color;
class ColumnRuleColor;
class Fill;
class FloodColor;
class InternalForcedBackgroundColor;
class InternalForcedBorderColor;
class InternalForcedColor;
class InternalForcedOutlineColor;
class InternalForcedVisitedColor;
class InternalVisitedBackgroundColor;
class InternalVisitedBorderBottomColor;
class InternalVisitedBorderLeftColor;
class InternalVisitedBorderRightColor;
class InternalVisitedBorderTopColor;
class InternalVisitedCaretColor;
class InternalVisitedColor;
class InternalVisitedColumnRuleColor;
class InternalVisitedFill;
class InternalVisitedOutlineColor;
class InternalVisitedStroke;
class InternalVisitedTextDecorationColor;
class InternalVisitedTextEmphasisColor;
class InternalVisitedTextFillColor;
class InternalVisitedTextStrokeColor;
class LightingColor;
class OutlineColor;
class StopColor;
class Stroke;
class TextDecorationColor;
class TextEmphasisColor;
class WebkitTapHighlightColor;
class WebkitTextFillColor;
class WebkitTextStrokeColor;

}  // namespace css_longhand

// ComputedStyle stores the computed value [1] for every CSS property on an
// element and provides the interface between the style engine and the rest of
// Blink. It acts as a container where the computed value of every CSS property
// can be stored and retrieved:
//
//   auto style = ComputedStyle::CreateInitialStyleSingleton();
//   style->SetDisplay(EDisplay::kNone); //'display' keyword property
//   style->Display();
//
// In addition to storing the computed value of every CSS property,
// ComputedStyle also contains various internal style information. Examples
// include cached_pseudo_element_styles_ (for storing pseudo element styles) and
// has_simple_underline_ (cached indicator flag of text-decoration). These are
// stored on ComputedStyle for two reasons:
//
//  1) They share the same lifetime as ComputedStyle, so it is convenient to
//  store them in the same object rather than a separate object that have to be
//  passed around as well.
//
//  2) Many of these data members can be packed as bit fields, so we use less
//  memory by packing them in this object with other bit fields.
//
// STORAGE:
//
// ComputedStyle is optimized for memory and performance. The data is not
// actually stored directly in ComputedStyle, but rather in a generated parent
// class ComputedStyleBase. This separation of concerns allows us to optimise
// the memory layout without affecting users of ComputedStyle. ComputedStyle
// inherits from ComputedStyleBase. For more about the memory layout, there is
// documentation in ComputedStyleBase and make_computed_style_base.py.
//
// INTERFACE:
//
// For most CSS properties, ComputedStyle provides a consistent interface which
// includes a getter, setter, and resetter (that resets the computed value to
// its initial value). Exceptions include vertical-align, which has a separate
// set of accessors for its length and its keyword components. Apart from
// accessors, ComputedStyle also has a wealth of helper functions.
//
// Because ComputedStyleBase defines simple accessors to every CSS property,
// ComputedStyle inherits these and so they are not redeclared in this file.
// This means that the interface to ComputedStyle is split between this file and
// ComputedStyleBase.h.
//
// [1] https://developer.mozilla.org/en-US/docs/Web/CSS/computed_value
//
// NOTE:
//
// Currently, some properties are stored in ComputedStyle and some in
// ComputedStyleBase. Eventually, the storage of all properties (except SVG
// ones) will be in ComputedStyleBase.
//
// Since this class is huge, do not mark all of it CORE_EXPORT.  Instead,
// export only the methods you need below.
class ComputedStyle final : public ComputedStyleBase {
  // Needed to allow access to private/protected getters of fields to allow diff
  // generation
  friend class ComputedStyleBase;
  friend class ComputedStyleBuilder;
  // Used by CSS animations. We can't allow them to animate based off visited
  // colors.
  friend class CSSPropertyEquality;

  // Accesses GetColor().
  friend class ComputedStyleUtils;
  // Color properties need access to private color utils.
  friend class css_longhand::AccentColor;
  friend class css_longhand::BackgroundColor;
  friend class css_longhand::BorderBottomColor;
  friend class css_longhand::BorderLeftColor;
  friend class css_longhand::BorderRightColor;
  friend class css_longhand::BorderTopColor;
  friend class css_longhand::CaretColor;
  friend class css_longhand::Clear;
  friend class css_longhand::Color;
  friend class css_longhand::ColumnRuleColor;
  friend class css_longhand::Fill;
  friend class css_longhand::Float;
  friend class css_longhand::FloodColor;
  friend class css_longhand::InternalForcedBackgroundColor;
  friend class css_longhand::InternalForcedBorderColor;
  friend class css_longhand::InternalForcedColor;
  friend class css_longhand::InternalForcedOutlineColor;
  friend class css_longhand::InternalForcedVisitedColor;
  friend class css_longhand::InternalVisitedBackgroundColor;
  friend class css_longhand::InternalVisitedBorderBottomColor;
  friend class css_longhand::InternalVisitedBorderLeftColor;
  friend class css_longhand::InternalVisitedBorderRightColor;
  friend class css_longhand::InternalVisitedBorderTopColor;
  friend class css_longhand::InternalVisitedCaretColor;
  friend class css_longhand::InternalVisitedColor;
  friend class css_longhand::InternalVisitedColumnRuleColor;
  friend class css_longhand::InternalVisitedFill;
  friend class css_longhand::InternalVisitedOutlineColor;
  friend class css_longhand::InternalVisitedStroke;
  friend class css_longhand::InternalVisitedTextDecorationColor;
  friend class css_longhand::InternalVisitedTextEmphasisColor;
  friend class css_longhand::InternalVisitedTextFillColor;
  friend class css_longhand::InternalVisitedTextStrokeColor;
  friend class css_longhand::LightingColor;
  friend class css_longhand::OutlineColor;
  friend class css_longhand::Resize;
  friend class css_longhand::StopColor;
  friend class css_longhand::Stroke;
  friend class css_longhand::TextDecorationColor;
  friend class css_longhand::TextEmphasisColor;
  friend class css_longhand::WebkitTapHighlightColor;
  friend class css_longhand::WebkitTextFillColor;
  friend class css_longhand::WebkitTextStrokeColor;
  // Access to private Appearance() and HasAppearance().
  friend class LayoutTheme;
  friend class StyleAdjuster;
  friend class StyleCascade;
  // Editing has to only reveal unvisited info.
  friend class EditingStyle;
  // Saves Border/Background information for later comparison.
  friend class CachedUAStyle;
  // Accesses visited and unvisited colors.
  friend class ColorPropertyFunctions;
  // Accesses inset and sizing property values.
  friend class LengthPropertyFunctions;
  // Edits the background for media controls and accesses UserModify().
  friend class StyleAdjuster;
  // Access to GetCurrentColor().
  friend class StyleResolver;
  // Access to UserModify().
  friend class MatchedPropertiesCache;

  // Allows unit tests to access protected members.
  friend class StyleResolverTest;

  using ComputedStyleBase::Clear;
  using ComputedStyleBase::Floating;
  using ComputedStyleBase::Resize;

 protected:
  mutable Member<StyleCachedData> cached_data_;

  StyleCachedData& EnsureCachedData() const;

  bool HasCachedPseudoElementStyles() const;
  CORE_EXPORT PseudoElementStyleCache* GetPseudoElementStyleCache() const;
  PseudoElementStyleCache& EnsurePseudoElementStyleCache() const;

  Vector<AtomicString>* GetVariableNamesCache() const;
  Vector<AtomicString>& EnsureVariableNamesCache() const;

  PositionFallbackStyleCache& EnsurePositionFallbackStyleCache(
      unsigned ensure_size) const;

  CORE_EXPORT base::RefCountedData<Vector<AppliedTextDecoration, 1>>*
  EnsureAppliedTextDecorationsCache() const;

 private:
  // TODO(sashab): Move these private members to the bottom of ComputedStyle.
  ALWAYS_INLINE ComputedStyle();
  ALWAYS_INLINE ComputedStyle(const ComputedStyle& initial_style);
  ALWAYS_INLINE explicit ComputedStyle(const ComputedStyleBuilder&);

 public:
  using PassKey = base::PassKey<ComputedStyle>;
  using BuilderPassKey = base::PassKey<ComputedStyleBuilder>;

  ALWAYS_INLINE ComputedStyle(BuilderPassKey,
                              const ComputedStyle& initial_style);
  ALWAYS_INLINE ComputedStyle(BuilderPassKey, const ComputedStyleBuilder&);
  ALWAYS_INLINE explicit ComputedStyle(PassKey);

  void TraceAfterDispatch(Visitor* visitor) const {
    visitor->Trace(cached_data_);
    ComputedStyleBase::TraceAfterDispatch(visitor);
  }

  // Create the per-document/context singleton that is used for shallow-copying
  // into new instances.
  CORE_EXPORT static const ComputedStyle* CreateInitialStyleSingleton();

  static const ComputedStyle* NullifyEnsured(const ComputedStyle* style) {
    if (!style) {
      return nullptr;
    }
    if (style->IsEnsuredOutsideFlatTree()) {
      return nullptr;
    }
    if (style->IsEnsuredInDisplayNone()) {
      return nullptr;
    }
    return style;
  }

  static bool IsNullOrEnsured(const ComputedStyle* style) {
    return !NullifyEnsured(style);
  }

  // Find out how two ComputedStyles differ. Used for figuring out if style
  // recalc needs to propagate style changes down the tree. The constants are
  // listed in increasing severity. E.g. kInherited also means we need to update
  // pseudo elements (kPseudoElementStyle).
  enum class Difference {
    // The ComputedStyle objects have the same computed style. The might have
    // some different extra flags which means we still need to replace the old
    // with the new instance.
    kEqual,
    // Non-inherited properties differ which means we need to apply visual
    // difference changes to the layout tree through LayoutObject::SetStyle().
    kNonInherited,
    // Pseudo element style is different which means we have to update pseudo
    // element existence and computed style.
    kPseudoElementStyle,
    // Inherited properties are different which means we need to recalc style
    // for children. Only independent properties changed which means we can
    // inherit by cloning the exiting ComputedStyle for children an set modified
    // properties directly without re-matching rules.
    kIndependentInherited,
    // Inherited properties are different which means we need to recalc style
    // for children.
    kInherited,
    // Properties which can affect descendants changed. This can happen the
    // following ways:
    //
    // Display type changes for flex/grid/custom layout affects computed style
    // adjustments for descendants. For instance flex/grid items are blockified
    // at computed style time and such items can be arbitrarily deep down the
    // flat tree in the presence of display:contents.
    //
    // The container-name property affects which container is queried by
    // rules matching descedant elements.
    kDescendantAffecting,
  };
  CORE_EXPORT static Difference ComputeDifference(
      const ComputedStyle* old_style,
      const ComputedStyle* new_style);

  // Returns true if the ComputedStyle change requires a LayoutObject re-attach.
  static bool NeedsReattachLayoutTree(const Element& element,
                                      const ComputedStyle* old_style,
                                      const ComputedStyle* new_style);

  ContentPosition ResolvedJustifyContentPosition(
      const StyleContentAlignmentData& normal_value_behavior) const;
  ContentDistributionType ResolvedJustifyContentDistribution(
      const StyleContentAlignmentData& normal_value_behavior) const;
  ContentPosition ResolvedAlignContentPosition(
      const StyleContentAlignmentData& normal_value_behavior) const;
  ContentDistributionType ResolvedAlignContentDistribution(
      const StyleContentAlignmentData& normal_value_behavior) const;
  StyleSelfAlignmentData ResolvedAlignSelf(
      ItemPosition normal_value_behaviour,
      const ComputedStyle* parent_style = nullptr) const;
  StyleContentAlignmentData ResolvedAlignContent(
      const StyleContentAlignmentData& normal_behaviour) const;
  StyleSelfAlignmentData ResolvedJustifySelf(
      ItemPosition normal_value_behaviour,
      const ComputedStyle* parent_style = nullptr) const;
  StyleContentAlignmentData ResolvedJustifyContent(
      const StyleContentAlignmentData& normal_behaviour) const;

  CORE_EXPORT StyleDifference
  VisualInvalidationDiff(const Document&, const ComputedStyle&) const;

  CORE_EXPORT const ComputedStyle* GetCachedPseudoElementStyle(
      PseudoId,
      const AtomicString& pseudo_argument = g_null_atom) const;
  const ComputedStyle* AddCachedPseudoElementStyle(const ComputedStyle*,
                                                   PseudoId,
                                                   const AtomicString&) const;
  const ComputedStyle* ReplaceCachedPseudoElementStyle(
      const ComputedStyle* pseudo_style,
      PseudoId pseudo_id,
      const AtomicString& pseudo_argument) const;
  void ClearCachedPseudoElementStyles() const;

  const ComputedStyle* GetCachedPositionFallbackStyle(unsigned index) const;
  const ComputedStyle* AddCachedPositionFallbackStyle(const ComputedStyle*,
                                                      unsigned index) const;

  // If this ComputedStyle is affected by animation/transitions, then the
  // unanimated "base" style can be retrieved with this function.
  //
  // If this function returns nullptr, then this ComputedStyle is not
  // affected by animations, and *is* the base style.
  CORE_EXPORT const ComputedStyle* GetBaseComputedStyle() const;

  // Indicates which properties are !important in the base style.
  CORE_EXPORT const CSSBitset* GetBaseImportantSet() const;

  CORE_EXPORT const ComputedStyle* GetBaseComputedStyleOrThis() const {
    if (auto* base = GetBaseComputedStyle()) {
      return base;
    }
    return this;
  }

  HashSet<AtomicString>* CustomHighlightNames() const {
    return CustomHighlightNamesInternal().get();
  }

  /**
   * ComputedStyle properties
   *
   * Each property stored in ComputedStyle is made up of fields. Fields have
   * getters and setters. A field is preferably a basic data type or enum,
   * but can be any type. A set of fields should be preceded by the property
   * the field is stored for.
   *
   * Field method naming should be done like so:
   *   // name-of-property
   *   int nameOfProperty() const;
   *   void SetNameOfProperty(int);
   * If the property has multiple fields, add the field name to the end of the
   * method name.
   *
   * Avoid nested types by splitting up fields where possible, e.g.:
   *  int getBorderTopWidth();
   *  int getBorderBottomWidth();
   *  int getBorderLeftWidth();
   *  int getBorderRightWidth();
   * is preferable to:
   *  BorderWidths getBorderWidths();
   *
   * Utility functions should go in a separate section at the end of the
   * class, and be kept to a minimum.
   */

  // anchor-default
  bool AnchorDefaultDataEquivalent(const ComputedStyle& o) const {
    return base::ValuesEquivalent(AnchorDefault(), o.AnchorDefault());
  }

  // anchor-name
  bool AnchorNameDataEquivalent(const ComputedStyle& o) const {
    return base::ValuesEquivalent(AnchorName(), o.AnchorName());
  }

  // For containing blocks, use |HasNonInitialBackdropFilter()| which includes
  // will-change: backdrop-filter.
  static bool HasBackdropFilter(const FilterOperations& backdrop_filter) {
    return !backdrop_filter.Operations().empty();
  }
  bool HasBackdropFilter() const { return HasBackdropFilter(BackdropFilter()); }

  // filter (aka -webkit-filter)
  // For containing blocks, use |HasNonInitialFilter()| which includes
  // will-change: filter.
  bool HasFilter() const { return !Filter().Operations().empty(); }

  // background-image
  bool HasBackgroundImage() const {
    return BackgroundInternal().AnyLayerHasImage();
  }
  bool HasFixedAttachmentBackgroundImage() const {
    return BackgroundInternal().AnyLayerHasFixedAttachmentImage();
  }

  // background-clip
  EFillBox BackgroundClip() const {
    return static_cast<EFillBox>(BackgroundInternal().Clip());
  }

  // Returns true if the Element should stick to the viewport bottom as the URL
  // bar hides.
  bool IsFixedToBottom() const {
    return !UsedBottom().IsAuto() && UsedTop().IsAuto();
  }

  // Border properties.
  // border-image-slice
  const LengthBox& BorderImageSlices() const {
    return BorderImage().ImageSlices();
  }

  // border-image-source
  StyleImage* BorderImageSource() const { return BorderImage().GetImage(); }

  // border-image-width
  const BorderImageLengthBox& BorderImageWidth() const {
    return BorderImage().BorderSlices();
  }

  // border-image-outset
  const BorderImageLengthBox& BorderImageOutset() const {
    return BorderImage().Outset();
  }

  static LayoutUnit BorderWidth(EBorderStyle style, LayoutUnit width) {
    if (style == EBorderStyle::kNone || style == EBorderStyle::kHidden) {
      return LayoutUnit();
    }
    return width;
  }

  // Border width properties.
  LayoutUnit BorderTopWidth() const {
    return BorderWidth(BorderTopStyle(), BorderTopWidthInternal());
  }
  LayoutUnit BorderBottomWidth() const {
    return BorderWidth(BorderBottomStyle(), BorderBottomWidthInternal());
  }
  LayoutUnit BorderLeftWidth() const {
    return BorderWidth(BorderLeftStyle(), BorderLeftWidthInternal());
  }
  LayoutUnit BorderRightWidth() const {
    return BorderWidth(BorderRightStyle(), BorderRightWidthInternal());
  }

  // box-shadow (aka -webkit-box-shadow)
  bool BoxShadowDataEquivalent(const ComputedStyle& other) const {
    return base::ValuesEquivalent(BoxShadow(), other.BoxShadow());
  }

  // clip-path
  ClipPathOperation* ClipPath() const {
    // This method is accessed frequently during SVG Hit Testing, but the
    // cumulative cost of calling |ClipPathInternal| can be fairly high due to
    // multiple rare data pointer indirections. |HasClipPath| was added as a way
    // to reduce the cost of these expensive indirections by placing a bit
    // in more easily accessible memory.
    return HasClipPath() ? ClipPathInternal().Get() : nullptr;
  }

  // column-rule-width
  uint16_t ColumnRuleWidth() const {
    if (ColumnRuleStyle() == EBorderStyle::kNone ||
        ColumnRuleStyle() == EBorderStyle::kHidden) {
      return 0;
    }
    return ColumnRuleWidthInternal().ToUnsigned();
  }

  // content
  ContentData* GetContentData() const { return ContentInternal().Get(); }

  // -webkit-line-clamp
  bool HasLineClamp() const { return LineClamp() > 0; }

  bool OpacityChangedStackingContext(const ComputedStyle& other) const {
    // We only need do layout for opacity changes if adding or losing opacity
    // could trigger a change
    // in us being a stacking context.
    if (IsStackingContextWithoutContainment() ==
            other.IsStackingContextWithoutContainment() ||
        HasOpacity() == other.HasOpacity()) {
      // FIXME: We would like to use SimplifiedLayout here, but we can't quite
      // do that yet.  We need to make sure SimplifiedLayout can operate
      // correctly on LayoutInlines (we will need to add a
      // selfNeedsSimplifiedLayout bit in order to not get confused and taint
      // every line).  In addition we need to solve the floating object issue
      // when layers come and go. Right now a full layout is necessary to keep
      // floating object lists sane.
      return true;
    }
    return false;
  }

  // Outline properties.

  bool OutlineVisuallyEqual(const ComputedStyle& other) const {
    if (OutlineStyle() == EBorderStyle::kNone &&
        other.OutlineStyle() == EBorderStyle::kNone) {
      return true;
    }
    return OutlineWidthInternal() == other.OutlineWidthInternal() &&
           OutlineColor() == other.OutlineColor() &&
           OutlineStyle() == other.OutlineStyle() &&
           OutlineOffset() == other.OutlineOffset() &&
           OutlineStyleIsAuto() == other.OutlineStyleIsAuto();
  }

  // outline-width
  LayoutUnit OutlineWidth() const {
    if (OutlineStyle() == EBorderStyle::kNone) {
      return LayoutUnit();
    }
    return OutlineWidthInternal();
  }

  // For history and compatibility reasons, we draw outline:auto (for focus
  // rings) and normal style outline differently.
  // Focus rings enclose block visual overflows (of line boxes and descendants),
  // while normal outlines don't.
  NGOutlineType OutlineRectsShouldIncludeBlockVisualOverflow() const {
    return OutlineStyleIsAuto()
               ? NGOutlineType::kIncludeBlockVisualOverflow
               : NGOutlineType::kDontIncludeBlockVisualOverflow;
  }

  // position-fallback

  // https://drafts.csswg.org/css-anchor-position-1/#position-fallback-list
  bool MayHavePositionFallbackList() const {
    return HasOutOfFlowPosition() && PositionFallback();
  }

  // Scroll properties.

  PhysicalToLogicalGetter<const Length&, ComputedStyle>
  PhysicalScrollPaddingToLogicalGetter() const {
    return PhysicalToLogicalGetter<const Length&, ComputedStyle>(
        GetWritingDirection(), *this, &ComputedStyleBase::ScrollPaddingTop,
        &ComputedStyleBase::ScrollPaddingRight,
        &ComputedStyleBase::ScrollPaddingBottom,
        &ComputedStyleBase::ScrollPaddingLeft);
  }

  // scroll-padding-block-start
  const Length& ScrollPaddingBlockStart() const {
    return PhysicalScrollPaddingToLogicalGetter().BlockStart();
  }

  // scroll-padding-block-end
  const Length& ScrollPaddingBlockEnd() const {
    return PhysicalScrollPaddingToLogicalGetter().BlockEnd();
  }

  // scroll-padding-inline-start
  const Length& ScrollPaddingInlineStart() const {
    return PhysicalScrollPaddingToLogicalGetter().InlineStart();
  }

  // scroll-padding-inline-end
  const Length& ScrollPaddingInlineEnd() const {
    return PhysicalScrollPaddingToLogicalGetter().InlineEnd();
  }

  // scrollbar-gutter
  inline bool IsScrollbarGutterAuto() const {
    return ScrollbarGutter() == kScrollbarGutterAuto;
  }
  inline bool IsScrollbarGutterStable() const {
    return ScrollbarGutter() & kScrollbarGutterStable;
  }
  inline bool IsScrollbarGutterBothEdges() const {
    return ScrollbarGutter() & kScrollbarGutterBothEdges;
  }

  bool UsesStandardScrollbarStyle() const {
    return ScrollbarWidth() != EScrollbarWidth::kAuto ||
           ScrollbarColor().has_value();
  }

  // Ignore non-standard ::-webkit-scrollbar when standard properties are in
  // use.
  bool HasCustomScrollbarStyle() const {
    return HasPseudoElementStyle(kPseudoIdScrollbar) &&
           !UsesStandardScrollbarStyle();
  }

  // shape-outside (aka -webkit-shape-outside)
  ShapeValue* ShapeOutside() const { return ShapeOutsideInternal().Get(); }
  bool ShapeOutsideDataEquivalent(const ComputedStyle& other) const {
    return base::ValuesEquivalent(ShapeOutside(), other.ShapeOutside());
  }

  // vertical-align
  EVerticalAlign VerticalAlign() const {
    return static_cast<EVerticalAlign>(VerticalAlignInternal());
  }

  // This returns the z-index if it applies (i.e. positioned element or grid or
  // flex children), and 0 otherwise. Note that for most situations,
  // `EffectiveZIndex()` is what the code should use to determine how to stack
  // the element. `ZIndex()` is still available and returns the value as
  // specified in style (used for e.g. style comparisons and computed style
  // reporting)
  int EffectiveZIndex() const { return EffectiveZIndexZero() ? 0 : ZIndex(); }

  // -webkit-clip-path
  bool ClipPathDataEquivalent(const ComputedStyle& other) const {
    return base::ValuesEquivalent(ClipPath(), other.ClipPath());
  }

  // Mask properties.
  // -webkit-mask-box-image-outset
  bool HasMaskBoxImageOutsets() const {
    return MaskBoxImageInternal().HasImage() && MaskBoxImageOutset().NonZero();
  }
  PhysicalBoxStrut MaskBoxImageOutsets() const {
    return ImageOutsets(MaskBoxImageInternal());
  }
  const BorderImageLengthBox& MaskBoxImageOutset() const {
    return MaskBoxImageInternal().Outset();
  }

  // -webkit-mask-box-image-slice
  const LengthBox& MaskBoxImageSlices() const {
    return MaskBoxImageInternal().ImageSlices();
  }

  // -webkit-mask-box-image-source
  StyleImage* MaskBoxImageSource() const {
    return MaskBoxImageInternal().GetImage();
  }

  // -webkit-mask-box-image-width
  const BorderImageLengthBox& MaskBoxImageWidth() const {
    return MaskBoxImageInternal().BorderSlices();
  }

  // Inherited properties.

  // line-height
  Length LineHeight() const;

  // List style properties.

  // list-style-type
  const AtomicString& ListStyleStringValue() const;
  bool ListStyleTypeDataEquivalent(const ComputedStyle& other) const {
    return base::ValuesEquivalent(ListStyleType(), other.ListStyleType());
  }

  // list-style-position

  // Returns true if ::marker should be rendered inline.
  // In some cases, it should be inline even if `list-style-position` property
  // value is `outside`.
  bool MarkerShouldBeInside(const Element& parent) const;

  // quotes
  bool QuotesDataEquivalent(const ComputedStyle&) const;

  // text-shadow
  bool TextShadowDataEquivalent(const ComputedStyle&) const;

  // Text emphasis properties.
  TextEmphasisMark GetTextEmphasisMark() const;
  const AtomicString& TextEmphasisMarkString() const;
  LineLogicalSide GetTextEmphasisLineLogicalSide() const;

  CORE_EXPORT FontSizeStyle GetFontSizeStyle() const {
    return FontSizeStyle(GetFont(), LineHeightInternal(), EffectiveZoom());
  }

  // Font properties.
  CORE_EXPORT const FontDescription& GetFontDescription() const {
    return GetFont().GetFontDescription();
  }
  bool HasFontRelativeUnits() const {
    return HasEmUnits() || HasRootFontRelativeUnits() ||
           HasGlyphRelativeUnits();
  }

  // If true, the ComputedStyle must be recalculated when fonts are updated.
  bool DependsOnFontMetrics() const {
    return HasGlyphRelativeUnits() || HasFontSizeAdjust() ||
           CustomStyleCallbackDependsOnFont();
  }
  bool CachedPseudoElementStylesDependOnFontMetrics() const;
  bool HighlightPseudoElementStylesDependOnFontMetrics() const;

  // font-size
  int FontSize() const { return GetFontDescription().ComputedPixelSize(); }
  CORE_EXPORT float SpecifiedFontSize() const {
    return GetFontDescription().SpecifiedSize();
  }
  CORE_EXPORT float ComputedFontSize() const {
    return GetFontDescription().ComputedSize();
  }
  static inline LayoutUnit ComputedFontSizeAsFixed(const Font& font) {
    return LayoutUnit::FromFloatRound(font.GetFontDescription().ComputedSize());
  }
  LayoutUnit ComputedFontSizeAsFixed() const {
    return ComputedFontSizeAsFixed(GetFont());
  }

  // font-size-adjust
  blink::FontSizeAdjust FontSizeAdjust() const {
    return GetFontDescription().SizeAdjust();
  }
  bool HasFontSizeAdjust() const {
    return GetFontDescription().HasSizeAdjust();
  }

  // font-weight
  CORE_EXPORT FontSelectionValue GetFontWeight() const {
    return GetFontDescription().Weight();
  }

  // font-stretch
  FontSelectionValue GetFontStretch() const {
    return GetFontDescription().Stretch();
  }

  // font-style
  FontSelectionValue GetFontStyle() const {
    return GetFontDescription().Style();
  }

  // font-palette
  blink::FontPalette* FontPalette() const {
    return GetFontDescription().GetFontPalette();
  }

  // Child is aligned to the parent by matching the parent’s dominant baseline
  // to the same baseline in the child.
  CORE_EXPORT FontBaseline GetFontBaseline() const;

  CORE_EXPORT FontHeight GetFontHeight(FontBaseline baseline) const;
  CORE_EXPORT FontHeight GetFontHeight() const {
    return GetFontHeight(GetFontBaseline());
  }

  // -webkit-locale
  const AtomicString& Locale() const {
    return LayoutLocale::LocaleString(GetFontDescription().Locale());
  }

  // letter-spacing
  float LetterSpacing() const { return GetFontDescription().LetterSpacing(); }

  // word-spacing
  float WordSpacing() const { return GetFontDescription().WordSpacing(); }

  // fill helpers
  bool HasFill() const { return !FillPaint().IsNone(); }
  bool IsFillColorCurrentColor() const {
    return FillPaint().HasCurrentColor() ||
           InternalVisitedFillPaint().HasCurrentColor();
  }

  // marker-* helpers
  bool HasMarkers() const {
    return MarkerStartResource() || MarkerMidResource() || MarkerEndResource();
  }

  // stroke helpers
  bool HasStroke() const { return !StrokePaint().IsNone(); }
  bool HasVisibleStroke() const {
    return HasStroke() && !StrokeWidth().IsZero();
  }
  bool IsStrokeColorCurrentColor() const {
    return StrokePaint().HasCurrentColor() ||
           InternalVisitedStrokePaint().HasCurrentColor();
  }
  bool HasDashArray() const { return !StrokeDashArray()->data.empty(); }
  bool StrokeDashArrayDataEquivalent(const ComputedStyle&) const;

  // accent-color
  // An empty optional means the accent-color is 'auto'
  absl::optional<blink::Color> AccentColorResolved() const;

  // scrollbar-color
  // An empty optional means the scrollbar-color is 'auto'
  absl::optional<blink::Color> ScrollbarThumbColorResolved() const;
  absl::optional<blink::Color> ScrollbarTrackColorResolved() const;

  // Comparison operators
  // FIXME: Replace callers of operator== wth a named method instead, e.g.
  // inheritedEquals().
  CORE_EXPORT bool operator==(const ComputedStyle& other) const;
  bool operator!=(const ComputedStyle& other) const {
    return !(*this == other);
  }

  bool InheritedEqual(const ComputedStyle&) const;
  bool NonInheritedEqual(const ComputedStyle&) const;
  inline bool IndependentInheritedEqual(const ComputedStyle&) const;
  inline bool NonIndependentInheritedEqual(const ComputedStyle&) const;
  bool InheritedDataShared(const ComputedStyle&) const;

  bool HasChildDependentFlags() const { return ChildHasExplicitInheritance(); }
  void CopyChildDependentFlagsFrom(const ComputedStyle&) const;

  // Counters.
  const CounterDirectiveMap* GetCounterDirectives() const;
  const CounterDirectives GetCounterDirectives(
      const AtomicString& identifier) const;
  bool CounterDirectivesEqual(const ComputedStyle& other) const {
    // If the counter directives change, trigger a relayout to re-calculate
    // counter values and rebuild the counter node tree.
    return base::ValuesEquivalent(CounterDirectivesInternal().get(),
                                  other.CounterDirectivesInternal().get());
  }

  bool IsDeprecatedWebkitBox() const {
    return Display() == EDisplay::kWebkitBox ||
           Display() == EDisplay::kWebkitInlineBox;
  }
  bool IsDeprecatedFlexboxUsingFlexLayout() const {
    return IsDeprecatedWebkitBox() &&
           !IsDeprecatedWebkitBoxWithVerticalLineClamp();
  }
  bool IsDeprecatedWebkitBoxWithVerticalLineClamp() const {
    return IsDeprecatedWebkitBox() && BoxOrient() == EBoxOrient::kVertical &&
           HasLineClamp();
  }

  // Variables.
  bool HasVariables() const;
  CORE_EXPORT wtf_size_t GetVariableNamesCount() const;
  CORE_EXPORT const Vector<AtomicString>& GetVariableNames() const;
  CORE_EXPORT const StyleInheritedVariables* InheritedVariables() const;
  CORE_EXPORT const StyleNonInheritedVariables* NonInheritedVariables() const;

  // Handles both inherited and non-inherited variables
  CORE_EXPORT CSSVariableData* GetVariableData(const AtomicString&) const;
  CSSVariableData* GetVariableData(const AtomicString&,
                                   bool is_inherited_property) const;

  const CSSValue* GetVariableValue(const AtomicString&) const;
  const CSSValue* GetVariableValue(const AtomicString&,
                                   bool is_inherited_property) const;

  // Animations.
  const CSSAnimationData* Animations() const {
    return AnimationsInternal().get();
  }

  // Transitions.
  const CSSTransitionData* Transitions() const {
    return TransitionsInternal().get();
  }

  // Column utility functions.
  bool SpecifiesColumns() const {
    return !HasAutoColumnCount() || !HasAutoColumnWidth();
  }
  bool ColumnRuleIsTransparent() const {
    return ColumnRuleColor()
        .Resolve(GetCurrentColor(), UsedColorScheme())
        .IsFullyTransparent();
  }
  bool ColumnRuleEquivalent(const ComputedStyle& other_style) const;
  bool HasColumnRule() const {
    if (LIKELY(!SpecifiesColumns())) {
      return false;
    }
    return ColumnRuleWidth() && !ColumnRuleIsTransparent() &&
           BorderStyleIsVisible(ColumnRuleStyle());
  }

  // Flex utility functions.
  bool ResolvedIsColumnFlexDirection() const {
    if (IsDeprecatedWebkitBox()) {
      return BoxOrient() == EBoxOrient::kVertical;
    }
    return FlexDirection() == EFlexDirection::kColumn ||
           FlexDirection() == EFlexDirection::kColumnReverse;
  }
  bool ResolvedIsColumnReverseFlexDirection() const {
    if (IsDeprecatedWebkitBox()) {
      return BoxOrient() == EBoxOrient::kVertical &&
             (RuntimeEnabledFeatures::NonInheritedWebkitBoxDirectionEnabled()
                  ? BoxDirectionAlternative() ==
                        EBoxDirectionAlternative::kReverse
                  : BoxDirection() == EBoxDirection::kReverse);
    }
    return FlexDirection() == EFlexDirection::kColumnReverse;
  }
  bool ResolvedIsRowFlexDirection() const {
    if (IsDeprecatedWebkitBox()) {
      return BoxOrient() == EBoxOrient::kHorizontal;
    }
    return FlexDirection() == EFlexDirection::kRow ||
           FlexDirection() == EFlexDirection::kRowReverse;
  }
  bool ResolvedIsRowReverseFlexDirection() const {
    if (IsDeprecatedWebkitBox()) {
      return BoxOrient() == EBoxOrient::kHorizontal &&
             (RuntimeEnabledFeatures::NonInheritedWebkitBoxDirectionEnabled()
                  ? BoxDirectionAlternative() ==
                        EBoxDirectionAlternative::kReverse
                  : BoxDirection() == EBoxDirection::kReverse);
    }
    return FlexDirection() == EFlexDirection::kRowReverse;
  }
  bool HasBoxReflect() const { return BoxReflect(); }
  bool ReflectionDataEquivalent(const ComputedStyle& other) const {
    return base::ValuesEquivalent(BoxReflect(), other.BoxReflect());
  }
  float ResolvedFlexGrow(const ComputedStyle& box_style) const {
    if (box_style.IsDeprecatedWebkitBox()) {
      return BoxFlex() > 0 ? BoxFlex() : 0.0f;
    }
    return FlexGrow();
  }
  float ResolvedFlexShrink(const ComputedStyle& box_style) const {
    if (box_style.IsDeprecatedWebkitBox()) {
      return BoxFlex() > 0 ? BoxFlex() : 0.0f;
    }
    return FlexShrink();
  }

  // Mask utility functions.
  bool HasMask() const {
    return MaskInternal().AnyLayerHasImage() ||
           MaskBoxImageInternal().HasImage();
  }
  const FillLayer& MaskLayers() const { return MaskInternal(); }
  const NinePieceImage& MaskBoxImage() const { return MaskBoxImageInternal(); }
  bool MaskBoxImageSlicesFill() const { return MaskBoxImageInternal().Fill(); }

  // Text-combine utility functions.
  bool HasTextCombine() const { return TextCombine() != ETextCombine::kNone; }

  // Grid utility functions.
  GridAutoFlow GetGridAutoFlow() const { return GridAutoFlowInternal(); }
  bool IsGridAutoFlowDirectionRow() const {
    return (GridAutoFlowInternal() &
            static_cast<int>(kInternalAutoFlowDirectionRow)) ==
           kInternalAutoFlowDirectionRow;
  }
  bool IsGridAutoFlowDirectionColumn() const {
    return (GridAutoFlowInternal() &
            static_cast<int>(kInternalAutoFlowDirectionColumn)) ==
           kInternalAutoFlowDirectionColumn;
  }
  bool IsGridAutoFlowAlgorithmSparse() const {
    return (GridAutoFlowInternal() &
            static_cast<int>(kInternalAutoFlowAlgorithmSparse)) ==
           kInternalAutoFlowAlgorithmSparse;
  }
  bool IsGridAutoFlowAlgorithmDense() const {
    return (GridAutoFlowInternal() &
            static_cast<int>(kInternalAutoFlowAlgorithmDense)) ==
           kInternalAutoFlowAlgorithmDense;
  }

  // Writing mode utility functions.
  WritingDirectionMode GetWritingDirection() const {
    return {GetWritingMode(), Direction()};
  }
  bool IsHorizontalWritingMode() const {
    return blink::IsHorizontalWritingMode(GetWritingMode());
  }
  bool IsFlippedLinesWritingMode() const {
    return blink::IsFlippedLinesWritingMode(GetWritingMode());
  }
  bool IsFlippedBlocksWritingMode() const {
    return blink::IsFlippedBlocksWritingMode(GetWritingMode());
  }

  // Will-change utility functions.
  bool HasWillChangeCompositingHint() const;
  bool HasWillChangeOpacityHint() const {
    return WillChangeProperties().Contains(CSSPropertyID::kOpacity) ||
           WillChangeProperties().Contains(CSSPropertyID::kAliasWebkitOpacity);
  }
  // Do we have a will-change hint for transform, perspective, or
  // transform-style?
  // TODO(dbaron): It's not clear that perspective and transform-style belong
  // here any more than they belong for scale, rotate, translate, or offset-*.
  bool HasWillChangeTransformHint() const;
  bool HasWillChangeScaleHint() const {
    return WillChangeProperties().Contains(CSSPropertyID::kScale);
  }
  bool HasWillChangeRotateHint() const {
    return WillChangeProperties().Contains(CSSPropertyID::kRotate);
  }
  bool HasWillChangeTranslateHint() const {
    return WillChangeProperties().Contains(CSSPropertyID::kTranslate);
  }
  bool HasWillChangeOffsetHint() const {
    return WillChangeProperties().Contains(CSSPropertyID::kOffsetPath) ||
           WillChangeProperties().Contains(CSSPropertyID::kOffsetPosition);
  }
  // The union of the above five functions (but faster).
  bool HasWillChangeHintForAnyTransformProperty() const;

  bool HasWillChangeFilterHint() const {
    return WillChangeProperties().Contains(CSSPropertyID::kFilter) ||
           WillChangeProperties().Contains(CSSPropertyID::kAliasWebkitFilter);
  }
  bool HasWillChangeBackdropFilterHint() const {
    return WillChangeProperties().Contains(CSSPropertyID::kBackdropFilter);
  }

  // Hyphen utility functions.
  Hyphenation* GetHyphenation() const;
  Hyphenation* GetHyphenationWithLimits() const;
  const AtomicString& HyphenString() const;

  // text-align utility functions.
  using ComputedStyleBase::GetTextAlign;
  ETextAlign GetTextAlign(bool is_last_line) const;

  // text-indent utility functions.
  bool ShouldUseTextIndent(bool is_first_line) const;

  // text-transform utility functions.
  void ApplyTextTransform(String*, UChar previous_character = ' ') const;

  // Line-height utility functions.
  const Length& SpecifiedLineHeight() const { return LineHeightInternal(); }
  static float ComputedLineHeight(const Length& line_height, const Font&);
  float ComputedLineHeight() const;
  CORE_EXPORT LayoutUnit ComputedLineHeightAsFixed() const;
  LayoutUnit ComputedLineHeightAsFixed(const Font& font) const;

  const Length& AdjustLengthForAnchorQueries(
      const Length& original_value,
      const Length& fallback_value) const {
    if (LIKELY(!original_value.HasAnchorQueries())) {
      return original_value;
    }
    return HasOutOfFlowPosition() ? original_value : fallback_value;
  }

  // Inset utility functions.
  const Length& UsedLeft() const {
    return AdjustLengthForAnchorQueries(Left(), Length::Auto());
  }
  const Length& UsedRight() const {
    return AdjustLengthForAnchorQueries(Right(), Length::Auto());
  }
  const Length& UsedTop() const {
    return AdjustLengthForAnchorQueries(Top(), Length::Auto());
  }
  const Length& UsedBottom() const {
    return AdjustLengthForAnchorQueries(Bottom(), Length::Auto());
  }
  bool HasAutoAnchorPositioning() const {
    return UsedLeft().HasAutoAnchorPositioning() ||
           UsedRight().HasAutoAnchorPositioning() ||
           UsedTop().HasAutoAnchorPositioning() ||
           UsedBottom().HasAutoAnchorPositioning();
  }

  // Width/height utility functions.
  const Length& UsedWidth() const {
    return AdjustLengthForAnchorQueries(Width(), Length::Auto());
  }
  const Length& UsedHeight() const {
    return AdjustLengthForAnchorQueries(Height(), Length::Auto());
  }
  const Length& UsedMinWidth() const {
    return AdjustLengthForAnchorQueries(MinWidth(), Length::Auto());
  }
  const Length& UsedMinHeight() const {
    return AdjustLengthForAnchorQueries(MinHeight(), Length::Auto());
  }
  const Length& UsedMaxWidth() const {
    return AdjustLengthForAnchorQueries(MaxWidth(), Length::None());
  }
  const Length& UsedMaxHeight() const {
    return AdjustLengthForAnchorQueries(MaxHeight(), Length::None());
  }
  const Length& LogicalWidth() const {
    return IsHorizontalWritingMode() ? UsedWidth() : UsedHeight();
  }
  const Length& LogicalHeight() const {
    return IsHorizontalWritingMode() ? UsedHeight() : UsedWidth();
  }
  const Length& LogicalMaxWidth() const {
    return IsHorizontalWritingMode() ? UsedMaxWidth() : UsedMaxHeight();
  }
  const Length& LogicalMaxHeight() const {
    return IsHorizontalWritingMode() ? UsedMaxHeight() : UsedMaxWidth();
  }
  const Length& LogicalMinWidth() const {
    return IsHorizontalWritingMode() ? UsedMinWidth() : UsedMinHeight();
  }
  const Length& LogicalMinHeight() const {
    return IsHorizontalWritingMode() ? UsedMinHeight() : UsedMinWidth();
  }

  // Margin utility functions.
  bool HasMarginBlockStartQuirk() const {
    return MayHaveMargin() && MarginBlockStart().Quirk();
  }
  bool HasMarginBlockEndQuirk() const {
    return MayHaveMargin() && MarginBlockEnd().Quirk();
  }
  const Length& MarginBlockStart() const {
    return MarginBlockStartUsing(*this);
  }
  const Length& MarginBlockEnd() const { return MarginBlockEndUsing(*this); }
  const Length& MarginInlineStart() const {
    return MarginInlineStartUsing(*this);
  }
  const Length& MarginInlineEnd() const { return MarginInlineEndUsing(*this); }
  const Length& MarginInlineStartUsing(const ComputedStyle& other) const {
    return PhysicalMarginToLogical(other).InlineStart();
  }
  const Length& MarginInlineEndUsing(const ComputedStyle& other) const {
    return PhysicalMarginToLogical(other).InlineEnd();
  }
  const Length& MarginBlockStartUsing(const ComputedStyle& other) const {
    return PhysicalMarginToLogical(other).BlockStart();
  }
  const Length& MarginBlockEndUsing(const ComputedStyle& other) const {
    return PhysicalMarginToLogical(other).BlockEnd();
  }
  bool MarginEqual(const ComputedStyle& other) const {
    return MarginTop() == other.MarginTop() &&
           MarginLeft() == other.MarginLeft() &&
           MarginRight() == other.MarginRight() &&
           MarginBottom() == other.MarginBottom();
  }

  // Padding utility functions.
  const Length& PaddingBlockStart() const {
    return PhysicalPaddingToLogical().BlockStart();
  }
  const Length& PaddingBlockEnd() const {
    return PhysicalPaddingToLogical().BlockEnd();
  }
  const Length& PaddingInlineStart() const {
    return PhysicalPaddingToLogical().InlineStart();
  }
  const Length& PaddingInlineEnd() const {
    return PhysicalPaddingToLogical().InlineEnd();
  }
  bool PaddingEqual(const ComputedStyle& other) const {
    return PaddingTop() == other.PaddingTop() &&
           PaddingLeft() == other.PaddingLeft() &&
           PaddingRight() == other.PaddingRight() &&
           PaddingBottom() == other.PaddingBottom();
  }
  bool PaddingEqual(const LengthBox& other) const {
    return PaddingTop() == other.Top() && PaddingLeft() == other.Left() &&
           PaddingRight() == other.Right() && PaddingBottom() == other.Bottom();
  }

  // Border utility functions
  PhysicalBoxStrut ImageOutsets(const NinePieceImage&) const;
  bool HasBorderImageOutsets() const {
    return BorderImage().HasImage() && BorderImage().Outset().NonZero();
  }
  PhysicalBoxStrut BorderImageOutsets() const {
    return ImageOutsets(BorderImage());
  }
  bool BorderImageSlicesFill() const { return BorderImage().Fill(); }

  const BorderValue BorderLeft() const {
    return BorderValue(BorderLeftStyle(), BorderLeftColor(),
                       BorderLeftWidthInternal());
  }
  const BorderValue BorderRight() const {
    return BorderValue(BorderRightStyle(), BorderRightColor(),
                       BorderRightWidthInternal());
  }
  const BorderValue BorderTop() const {
    return BorderValue(BorderTopStyle(), BorderTopColor(),
                       BorderTopWidthInternal());
  }
  const BorderValue BorderBottom() const {
    return BorderValue(BorderBottomStyle(), BorderBottomColor(),
                       BorderBottomWidthInternal());
  }

  bool BorderSizeEquals(const ComputedStyle& o) const {
    return BorderLeftWidth() == o.BorderLeftWidth() &&
           BorderTopWidth() == o.BorderTopWidth() &&
           BorderRightWidth() == o.BorderRightWidth() &&
           BorderBottomWidth() == o.BorderBottomWidth();
  }

  BorderValue BorderBlockStartUsing(const ComputedStyle& other) const {
    return PhysicalBorderToLogical(other).BlockStart();
  }
  BorderValue BorderBlockEndUsing(const ComputedStyle& other) const {
    return PhysicalBorderToLogical(other).BlockEnd();
  }
  BorderValue BorderInlineStartUsing(const ComputedStyle& other) const {
    return PhysicalBorderToLogical(other).InlineStart();
  }
  BorderValue BorderInlineEndUsing(const ComputedStyle& other) const {
    return PhysicalBorderToLogical(other).InlineEnd();
  }

  BorderValue BorderBlockStart() const { return BorderBlockStartUsing(*this); }
  BorderValue BorderBlockEnd() const { return BorderBlockEndUsing(*this); }
  BorderValue BorderInlineStart() const {
    return BorderInlineStartUsing(*this);
  }
  BorderValue BorderInlineEnd() const { return BorderInlineEndUsing(*this); }

  LayoutUnit BorderBlockEndWidth() const {
    return PhysicalBorderWidthToLogical().BlockEnd();
  }
  LayoutUnit BorderBlockStartWidth() const {
    return PhysicalBorderWidthToLogical().BlockStart();
  }
  LayoutUnit BorderInlineEndWidth() const {
    return PhysicalBorderWidthToLogical().InlineEnd();
  }
  LayoutUnit BorderInlineStartWidth() const {
    return PhysicalBorderWidthToLogical().InlineStart();
  }

  bool HasBorder() const {
    return BorderLeftWidth() || BorderRightWidth() || BorderTopWidth() ||
           BorderBottomWidth();
  }
  bool HasBorderDecoration() const {
    return HasBorder() || BorderImage().HasImage();
  }
  bool HasBorderRadius() const {
    if (!BorderTopLeftRadius().Width().IsZero()) {
      return true;
    }
    if (!BorderTopRightRadius().Width().IsZero()) {
      return true;
    }
    if (!BorderBottomLeftRadius().Width().IsZero()) {
      return true;
    }
    if (!BorderBottomRightRadius().Width().IsZero()) {
      return true;
    }
    return false;
  }

  bool RadiiEqual(const ComputedStyle& o) const {
    if (IsSurroundDataSharedWith(o)) {
      // Fast path; we don't need to do individual testing.
      return true;
    }
    return BorderTopLeftRadius() == o.BorderTopLeftRadius() &&
           BorderTopRightRadius() == o.BorderTopRightRadius() &&
           BorderBottomLeftRadius() == o.BorderBottomLeftRadius() &&
           BorderBottomRightRadius() == o.BorderBottomRightRadius();
  }

  bool BorderLeftEquals(const ComputedStyle& o) const {
    return BorderLeftWidthInternal() == o.BorderLeftWidthInternal() &&
           BorderLeftStyle() == o.BorderLeftStyle() &&
           ResolvedColor(BorderLeftColor()) ==
               o.ResolvedColor(o.BorderLeftColor());
  }
  bool BorderLeftEquals(const BorderValue& o) const {
    return BorderLeftWidthInternal() == o.Width() &&
           BorderLeftStyle() == o.Style() && BorderLeftColor() == o.GetColor();
  }

  bool BorderLeftVisuallyEqual(const ComputedStyle& o) const {
    if (BorderLeftStyle() == EBorderStyle::kNone &&
        o.BorderLeftStyle() == EBorderStyle::kNone) {
      return true;
    }
    if (BorderLeftStyle() == EBorderStyle::kHidden &&
        o.BorderLeftStyle() == EBorderStyle::kHidden) {
      return true;
    }
    return BorderLeftEquals(o);
  }

  bool BorderRightEquals(const ComputedStyle& o) const {
    return BorderRightWidthInternal() == o.BorderRightWidthInternal() &&
           BorderRightStyle() == o.BorderRightStyle() &&
           ResolvedColor(BorderRightColor()) ==
               o.ResolvedColor(o.BorderRightColor());
  }
  bool BorderRightEquals(const BorderValue& o) const {
    return BorderRightWidthInternal() == o.Width() &&
           BorderRightStyle() == o.Style() &&
           BorderRightColor() == o.GetColor();
  }

  bool BorderRightVisuallyEqual(const ComputedStyle& o) const {
    if (BorderRightStyle() == EBorderStyle::kNone &&
        o.BorderRightStyle() == EBorderStyle::kNone) {
      return true;
    }
    if (BorderRightStyle() == EBorderStyle::kHidden &&
        o.BorderRightStyle() == EBorderStyle::kHidden) {
      return true;
    }
    return BorderRightEquals(o);
  }

  bool BorderTopVisuallyEqual(const ComputedStyle& o) const {
    if (BorderTopStyle() == EBorderStyle::kNone &&
        o.BorderTopStyle() == EBorderStyle::kNone) {
      return true;
    }
    if (BorderTopStyle() == EBorderStyle::kHidden &&
        o.BorderTopStyle() == EBorderStyle::kHidden) {
      return true;
    }
    return BorderTopEquals(o);
  }

  bool BorderTopEquals(const ComputedStyle& o) const {
    return BorderTopWidthInternal() == o.BorderTopWidthInternal() &&
           BorderTopStyle() == o.BorderTopStyle() &&
           ResolvedColor(BorderTopColor()) ==
               o.ResolvedColor(o.BorderTopColor());
  }
  bool BorderTopEquals(const BorderValue& o) const {
    return BorderTopWidthInternal() == o.Width() &&
           BorderTopStyle() == o.Style() && BorderTopColor() == o.GetColor();
  }

  bool BorderBottomVisuallyEqual(const ComputedStyle& o) const {
    if (BorderBottomStyle() == EBorderStyle::kNone &&
        o.BorderBottomStyle() == EBorderStyle::kNone) {
      return true;
    }
    if (BorderBottomStyle() == EBorderStyle::kHidden &&
        o.BorderBottomStyle() == EBorderStyle::kHidden) {
      return true;
    }
    return BorderBottomEquals(o);
  }

  bool BorderBottomEquals(const ComputedStyle& o) const {
    return BorderBottomWidthInternal() == o.BorderBottomWidthInternal() &&
           BorderBottomStyle() == o.BorderBottomStyle() &&
           ResolvedColor(BorderBottomColor()) ==
               o.ResolvedColor(o.BorderBottomColor());
  }
  bool BorderBottomEquals(const BorderValue& o) const {
    return BorderBottomWidthInternal() == o.Width() &&
           BorderBottomStyle() == o.Style() &&
           BorderBottomColor() == o.GetColor();
  }

  bool BorderEquals(const ComputedStyle& o) const {
    return BorderLeftEquals(o) && BorderRightEquals(o) && BorderTopEquals(o) &&
           BorderBottomEquals(o) && BorderImage() == o.BorderImage();
  }

  bool BorderVisuallyEqual(const ComputedStyle& o) const {
    return BorderLeftVisuallyEqual(o) && BorderRightVisuallyEqual(o) &&
           BorderTopVisuallyEqual(o) && BorderBottomVisuallyEqual(o) &&
           BorderImage() == o.BorderImage();
  }

  bool BorderVisualOverflowEqual(const ComputedStyle& o) const {
    return BorderImage().Outset() == o.BorderImage().Outset();
  }

  CORE_EXPORT void AdjustDiffForClipPath(const ComputedStyle& o,
                                         StyleDifference& diff) const;

  CORE_EXPORT void AdjustDiffForBackgroundVisuallyEqual(
      const ComputedStyle& o,
      StyleDifference& diff) const;

  bool CanRenderBorderImage() const;

  // Float utility functions.
  bool IsFloating() const { return Floating() != EFloat::kNone; }
  EFloat UnresolvedFloating() const { return Floating(); }

  EFloat Floating(const ComputedStyle& cb_style) const {
    return Floating(cb_style.Direction());
  }

  EFloat Floating(TextDirection cb_direction) const {
    const EFloat value = Floating();
    switch (value) {
      case EFloat::kInlineStart:
        return IsLtr(cb_direction) ? EFloat::kLeft : EFloat::kRight;
      case EFloat::kInlineEnd:
        return IsLtr(cb_direction) ? EFloat::kRight : EFloat::kLeft;
      default:
        return value;
    }
  }

  // Mix-blend-mode utility functions.
  bool HasBlendMode() const { return GetBlendMode() != BlendMode::kNormal; }

  // Motion utility functions.
  bool HasOffset() const {
    return (!OffsetPosition().X().IsAuto() && !OffsetPosition().X().IsNone()) ||
           OffsetPath();
  }

  // Direction utility functions.
  bool IsLeftToRightDirection() const {
    return Direction() == TextDirection::kLtr;
  }

  // Perspective utility functions.
  bool HasPerspective() const { return Perspective() >= 0; }

  float UsedPerspective() const {
    DCHECK(HasPerspective());
    return std::max(1.0f, Perspective());
  }

  // Outline utility functions.
  // HasOutline is insufficient to determine whether Node has an outline.
  // Use NGOutlineUtils::HasPaintedOutline instead.
  bool HasOutline() const {
    return OutlineWidth() > 0 && OutlineStyle() > EBorderStyle::kHidden;
  }
  bool HasOutlineWithCurrentColor() const {
    return HasOutline() && OutlineColor().IsCurrentColor();
  }

  // Position utility functions.
  static bool HasOutOfFlowPosition(EPosition position) {
    return position == EPosition::kAbsolute || position == EPosition::kFixed;
  }
  bool HasOutOfFlowPosition() const {
    return HasOutOfFlowPosition(GetPosition());
  }
  bool HasInFlowPosition() const {
    return GetPosition() == EPosition::kRelative ||
           GetPosition() == EPosition::kSticky;
  }
  bool HasStickyConstrainedPosition() const {
    return GetPosition() == EPosition::kSticky &&
           (!UsedTop().IsAuto() || !UsedLeft().IsAuto() ||
            !UsedRight().IsAuto() || !UsedBottom().IsAuto());
  }
  static EPosition GetPosition(EDisplay display, EPosition position_internal) {
    // Applied sticky position is static for table columns and column groups.
    if (position_internal == EPosition::kSticky &&
        (display == EDisplay::kTableColumnGroup ||
         display == EDisplay::kTableColumn)) {
      return EPosition::kStatic;
    }
    return position_internal;
  }
  EPosition GetPosition() const {
    return GetPosition(Display(), PositionInternal());
  }

  // Clear utility functions.
  bool HasClear() const { return Clear() != EClear::kNone; }
  EClear UnresolvedClear() const { return Clear(); }

  EClear Clear(const ComputedStyle& cb_style) const {
    return Clear(cb_style.Direction());
  }

  EClear Clear(TextDirection cb_direction) const {
    const EClear value = Clear();
    switch (value) {
      case EClear::kInlineStart:
        return IsLtr(cb_direction) ? EClear::kLeft : EClear::kRight;
      case EClear::kInlineEnd:
        return IsLtr(cb_direction) ? EClear::kRight : EClear::kLeft;
      default:
        return value;
    }
  }

  // Clip utility functions.
  const Length& ClipLeft() const { return Clip().Left(); }
  const Length& ClipRight() const { return Clip().Right(); }
  const Length& ClipTop() const { return Clip().Top(); }
  const Length& ClipBottom() const { return Clip().Bottom(); }

  // Offset utility functions.
  // Accessors for positioned object edges that take into account writing mode.
  const Length& LogicalInlineStart() const {
    return PhysicalBoundsToLogical().InlineStart();
  }
  const Length& LogicalInlineEnd() const {
    return PhysicalBoundsToLogical().InlineEnd();
  }
  const Length& LogicalTop() const {
    return PhysicalBoundsToLogical().BlockStart();
  }
  const Length& LogicalBottom() const {
    return PhysicalBoundsToLogical().BlockEnd();
  }
  bool OffsetEqual(const ComputedStyle& other) const {
    return UsedLeft() == other.UsedLeft() && UsedRight() == other.UsedRight() &&
           UsedTop() == other.UsedTop() && UsedBottom() == other.UsedBottom();
  }

  // Whether or not a positioned element requires normal flow x/y to be computed
  // to determine its position.
  bool HasAutoLeftAndRight() const {
    return UsedLeft().IsAuto() && UsedRight().IsAuto();
  }
  bool HasAutoTopAndBottom() const {
    return UsedTop().IsAuto() && UsedBottom().IsAuto();
  }
  bool HasStaticInlinePosition(bool horizontal) const {
    return horizontal ? HasAutoLeftAndRight() : HasAutoTopAndBottom();
  }
  bool HasStaticBlockPosition(bool horizontal) const {
    return horizontal ? HasAutoTopAndBottom() : HasAutoLeftAndRight();
  }

  // Content utility functions.
  bool ContentDataEquivalent(const ComputedStyle& other) const {
    return base::ValuesEquivalent(GetContentData(), other.GetContentData());
  }

  // Contain utility functions.
  //
  // Containment can be enabled from a variety of sources, not just the
  // 'contain' property itself. The "effective containment" represents whether
  //  or not we should enable containment of a given type, taking those
  //  different sources into account.
  //
  // Note that even a certain type of containment appears to be in effect from
  // the perspective of ComputedStyle, containment may still not be applied if
  // the LayoutObject is ineligible for the given containment type. See
  // |LayoutObject::IsEligibleForSizeContainment| and similar functions.

  static unsigned EffectiveContainment(unsigned contain,
                                       unsigned container_type,
                                       EContentVisibility content_visibility,
                                       bool skips_contents) {
    unsigned effective = contain;

    if (container_type & kContainerTypeInlineSize) {
      effective |= kContainsStyle;
      effective |= kContainsLayout;
      effective |= kContainsInlineSize;
    }
    if (container_type & kContainerTypeBlockSize) {
      effective |= kContainsStyle;
      effective |= kContainsLayout;
      effective |= kContainsBlockSize;
    }
    if (!IsContentVisibilityVisible(content_visibility)) {
      effective |= kContainsStyle;
      effective |= kContainsLayout;
      effective |= kContainsPaint;
    }
    if (skips_contents) {
      effective |= kContainsSize;
    }

    return effective;
  }

  unsigned EffectiveContainment() const {
    return ComputedStyle::EffectiveContainment(
        Contain(), ContainerType(), ContentVisibility(), SkipsContents());
  }

  bool ContainsStyle() const { return EffectiveContainment() & kContainsStyle; }
  bool ContainsPaint() const { return EffectiveContainment() & kContainsPaint; }
  bool ContainsLayout() const {
    return EffectiveContainment() & kContainsLayout;
  }
  bool ContainsSize() const {
    return (EffectiveContainment() & kContainsSize) == kContainsSize;
  }
  bool ContainsInlineSize() const {
    return EffectiveContainment() & kContainsInlineSize;
  }
  bool ContainsBlockSize() const {
    return EffectiveContainment() & kContainsBlockSize;
  }

  CORE_EXPORT static bool ShouldApplyAnyContainment(
      const Element& element,
      const DisplayStyle&,
      unsigned effective_containment);

  CORE_EXPORT bool ShouldApplyAnyContainment(const Element& element) const {
    return ShouldApplyAnyContainment(element, GetDisplayStyle(),
                                     EffectiveContainment());
  }

  // Return true if an element can match size container queries. In addition to
  // checking if it has a size container-type, we check if we are never able to
  // reach NGBlockNode::Layout() for legacy layout objects or SVG elements.
  bool CanMatchSizeContainerQueries(const Element& element) const;

  bool IsContainerForSizeContainerQueries() const {
    return IsInlineOrBlockSizeContainer() && StyleType() == kPseudoIdNone;
  }

  bool IsContainerForStickyContainerQueries() const {
    return IsStickyContainer() && StyleType() == kPseudoIdNone;
  }

  bool DependsOnContainerQueries() const {
    return DependsOnSizeContainerQueries() ||
           DependsOnStyleContainerQueries() || DependsOnStateContainerQueries();
  }

  static bool IsContentVisibilityVisible(
      EContentVisibility content_visibility) {
    return content_visibility == EContentVisibility::kVisible;
  }

  bool IsContentVisibilityVisible() const {
    return IsContentVisibilityVisible(ContentVisibility());
  }

  // Interleaving roots are elements that may require layout to fully update
  // the style of their descendants.
  static bool IsInterleavingRoot(const ComputedStyle*);

  // Display utility functions.
  bool IsDisplayReplacedType() const {
    return IsDisplayReplacedType(Display());
  }
  bool IsDisplayInlineType() const { return IsDisplayInlineType(Display()); }
  bool IsDisplayBlockContainer() const {
    return IsDisplayBlockContainer(Display());
  }
  bool IsDisplayListItem() const { return IsDisplayListItem(Display()); }
  static bool IsDisplayListItem(EDisplay display) {
    return display == EDisplay::kListItem ||
           display == EDisplay::kInlineListItem ||
           display == EDisplay::kFlowRootListItem ||
           display == EDisplay::kInlineFlowRootListItem;
  }
  bool IsDisplayTableBox() const { return IsDisplayTableBox(Display()); }
  bool IsDisplayFlexibleBox() const { return IsDisplayFlexibleBox(Display()); }
  bool IsDisplayGridBox() const { return IsDisplayGridBox(Display()); }
  bool IsDisplayFlexibleOrGridBox() const {
    return IsDisplayFlexibleBox(Display()) || IsDisplayGridBox(Display());
  }
  bool IsDisplayLayoutCustomBox() const {
    return IsDisplayLayoutCustomBox(Display());
  }

  bool IsDisplayTableType() const { return IsDisplayTableType(Display()); }

  bool IsDisplayMathType() const { return IsDisplayMathBox(Display()); }

  bool BlockifiesChildren() const {
    return IsDisplayFlexibleOrGridBox() || IsDisplayMathType() ||
           IsDisplayLayoutCustomBox() ||
           (Display() == EDisplay::kContents && IsInBlockifyingDisplay());
  }

  // Return true if an element with this computed style requires LayoutNG
  // (i.e. has no legacy layout implementation).
  bool DisplayTypeRequiresLayoutNG() const {
    return IsDisplayMathType() || IsDisplayLayoutCustomBox();
  }

  // Isolation utility functions.
  bool HasIsolation() const { return Isolation() != EIsolation::kAuto; }

  DisplayStyle GetDisplayStyle() const {
    return DisplayStyle(Display(), StyleType(), GetContentData());
  }

  // Content utility functions.
  bool ContentBehavesAsNormal() const {
    return GetDisplayStyle().ContentBehavesAsNormal();
  }
  bool ContentPreventsBoxGeneration() const {
    return GetDisplayStyle().ContentPreventsBoxGeneration();
  }

  // Cursor utility functions.
  CursorList* Cursors() const { return CursorDataInternal().Get(); }

  // Resize utility functions.
  bool HasResize() const {
    return StyleType() == kPseudoIdNone && Resize() != EResize::kNone;
  }
  EResize UnresolvedResize() const { return Resize(); }

  EResize UsedResize() const {
    EResize value = Resize();
    switch (value) {
      case EResize::kBlock:
        return IsHorizontalWritingMode() ? EResize::kVertical
                                         : EResize::kHorizontal;
      case EResize::kInline:
        return IsHorizontalWritingMode() ? EResize::kHorizontal
                                         : EResize::kVertical;
      default:
        return value;
    }
  }

  // Pointer-events utility functions.
  EPointerEvents UsedPointerEvents() const {
    if (IsInert()) {
      return EPointerEvents::kNone;
    }
    return PointerEvents();
  }

  // User modify utility functions.
  EUserModify UsedUserModify() const {
    if (IsInert()) {
      return EUserModify::kReadOnly;
    }
    return UserModify();
  }

  // User select utility functions.
  EUserSelect UsedUserSelect() const {
    if (IsInert()) {
      return EUserSelect::kNone;
    }
    return UserSelect();
  }

  bool IsSelectable() const {
    return !IsInert() && !(UserSelect() == EUserSelect::kNone &&
                           UserModify() == EUserModify::kReadOnly);
  }

  bool IsFocusable() const {
    // TODO: Maybe `display: contents` shouldn't prevent focusability, see
    // discussion in https://github.com/whatwg/html/issues/1837
    // TODO: `visibility: hidden` shouldn't prevent focusability, see
    // https://html.spec.whatwg.org/multipage/interaction.html#focusable-area
    return !IsEnsuredInDisplayNone() && Display() != EDisplay::kContents &&
           !IsInert() && Visibility() == EVisibility::kVisible;
  }

  // Text decoration utility functions.
  bool TextDecorationVisualOverflowEqual(const ComputedStyle& o) const;
  CORE_EXPORT TextDecorationLine TextDecorationsInEffect() const;
  CORE_EXPORT const Vector<AppliedTextDecoration, 1>& AppliedTextDecorations()
      const;
  CORE_EXPORT base::RefCountedData<Vector<AppliedTextDecoration, 1>>*
  AppliedTextDecorationData() const {
    return IsDecoratingBox() ? EnsureAppliedTextDecorationsCache()
                             : BaseTextDecorationDataInternal().get();
  }
  const Vector<AppliedTextDecoration, 1>* BaseAppliedTextDecorations() const {
    const auto base = BaseTextDecorationDataInternal();
    return base ? &base->data : nullptr;
  }

  // Returns true if this a "decorating box".
  // https://drafts.csswg.org/css-text-decor-3/#decorating-box
  bool IsDecoratingBox() const {
    if (GetTextDecorationLine() == TextDecorationLine::kNone) {
      return false;
    }
    if (Display() == EDisplay::kContents) {
      return false;
    }
    return true;
  }

  // Returns true if there are any text decorations.
  bool HasAppliedTextDecorations() const {
    if (IsDecoratingBox()) {
      return true;
    }
    if (BaseTextDecorationDataInternal()) {
      DCHECK(!BaseTextDecorationDataInternal()->data.empty());
      return true;
    }
    return false;
  }

  // Returns (by value) the last text decoration, if any.
  absl::optional<AppliedTextDecoration> LastAppliedTextDecoration() const {
    if (HasAppliedTextDecorations()) {
      return AppliedTextDecorations().back();
    }
    return absl::nullopt;
  }

  // Overflow utility functions.

  EOverflow OverflowInlineDirection() const {
    return IsHorizontalWritingMode() ? OverflowX() : OverflowY();
  }
  EOverflow OverflowBlockDirection() const {
    return IsHorizontalWritingMode() ? OverflowY() : OverflowX();
  }

  // Returns true if 'overflow' is 'visible' along both axes. When 'clip' is
  // used the other axis may be 'visible'. In other words, if one axis is
  // 'visible' the other axis is not necessarily 'visible.'
  bool IsOverflowVisibleAlongBothAxes() const {
    // Overflip clip and overflow visible may be used along different axis.
    return OverflowX() == EOverflow::kVisible &&
           OverflowY() == EOverflow::kVisible;
  }

  // Returns true if 'overflow' is 'visible' or 'clip' along both axes.
  bool IsOverflowVisibleOrClip() const {
    bool overflow_x =
        OverflowX() == EOverflow::kVisible || OverflowX() == EOverflow::kClip;
    DCHECK(!overflow_x || OverflowY() == EOverflow::kVisible ||
           OverflowY() == EOverflow::kClip);
    return overflow_x;
  }

  // An overflow value of visible or clip is not a scroll container, all other
  // values result in a scroll container. Also note that if visible or clip is
  // set on one axis, then the other axis must also be visible or clip. For
  // example, "overflow-x: clip; overflow-y: visible" is allowed, but
  // "overflow-x: clip; overflow-y: hidden" is not.
  bool IsScrollContainer() const {
    return OverflowX() != EOverflow::kVisible &&
           OverflowX() != EOverflow::kClip;
  }

  // Returns true if object-fit, object-position and object-view-box would avoid
  // replaced contents overflow.
  bool ObjectPropertiesPreventReplacedOverflow() const {
    if (GetObjectFit() == EObjectFit::kNone ||
        GetObjectFit() == EObjectFit::kCover) {
      return false;
    }

    if (ObjectPosition() !=
        LengthPoint(Length::Percent(50.0), Length::Percent(50.0))) {
      return false;
    }

    if (ObjectViewBox()) {
      return false;
    }

    return true;
  }

  static bool HasAutoScroll(EOverflow overflow) {
    return overflow == EOverflow::kAuto || overflow == EOverflow::kOverlay;
  }

  static bool ScrollsOverflow(EOverflow overflow) {
    return overflow == EOverflow::kScroll || HasAutoScroll(overflow);
  }

  bool HasAutoHorizontalScroll() const {
    return ComputedStyle::HasAutoScroll(OverflowX());
  }

  bool HasAutoVerticalScroll() const {
    return ComputedStyle::HasAutoScroll(OverflowY());
  }

  bool ScrollsOverflowX() const {
    return ComputedStyle::ScrollsOverflow(OverflowX());
  }

  bool ScrollsOverflowY() const {
    return ComputedStyle::ScrollsOverflow(OverflowY());
  }

  bool ScrollsOverflow() const {
    return ScrollsOverflowX() || ScrollsOverflowY();
  }

  // Visibility utility functions.
  bool VisibleToHitTesting() const {
    return Visibility() == EVisibility::kVisible &&
           UsedPointerEvents() != EPointerEvents::kNone;
  }

  // Animation utility functions.
  bool HasCurrentTransformRelatedAnimation() const {
    return HasCurrentTransformAnimation() || HasCurrentScaleAnimation() ||
           HasCurrentRotateAnimation() || HasCurrentTranslateAnimation();
  }
  bool HasCurrentCompositableAnimation() const {
    return HasCurrentOpacityAnimation() ||
           HasCurrentTransformRelatedAnimation() ||
           HasCurrentFilterAnimation() || HasCurrentBackdropFilterAnimation() ||
           (RuntimeEnabledFeatures::CompositeBGColorAnimationEnabled() &&
            HasCurrentBackgroundColorAnimation());
  }
  bool ShouldCompositeForCurrentAnimations() const {
    return HasCurrentOpacityAnimation() ||
           HasCurrentTransformRelatedAnimation() ||
           HasCurrentFilterAnimation() || HasCurrentBackdropFilterAnimation();
  }
  bool RequiresPropertyNodeForAnimation() const {
    return IsRunningOpacityAnimationOnCompositor() ||
           IsRunningTransformAnimationOnCompositor() ||
           IsRunningScaleAnimationOnCompositor() ||
           IsRunningRotateAnimationOnCompositor() ||
           IsRunningTranslateAnimationOnCompositor() ||
           IsRunningFilterAnimationOnCompositor() ||
           IsRunningBackdropFilterAnimationOnCompositor();
  }

  // Opacity utility functions.
  bool HasOpacity() const { return Opacity() < 1.0f; }

  // Table layout utility functions.
  bool IsFixedTableLayout() const {
    // https://www.w3.org/TR/css-tables-3/#table-layout-property
    return TableLayout() == ETableLayout::kFixed &&
           (LogicalWidth().IsSpecified() || LogicalWidth().IsMinContent() ||
            LogicalWidth().IsFitContent());
  }

  LogicalSize TableBorderSpacing() const {
    if (BorderCollapse() == EBorderCollapse::kCollapse) {
      return LogicalSize();
    }
    return LogicalSize(LayoutUnit(HorizontalBorderSpacing()),
                       LayoutUnit(VerticalBorderSpacing()));
  }

  // Returns true if the computed style contains a 3D transform operation. This
  // can be individual operations from the transform property, or individual
  // values from translate/rotate/scale properties. Perspective is omitted since
  // it does not, by itself, specify a 3D transform.
  bool Has3DTransformOperation() const {
    return Transform().HasNonPerspective3DOperation() ||
           (Translate() && Translate()->Z() != 0) ||
           (Rotate() && (Rotate()->X() != 0 || Rotate()->Y() != 0)) ||
           (Scale() && Scale()->Z() != 1);
  }
  bool HasTransform() const {
    return HasTransformOperations() || HasOffset() ||
           HasCurrentTransformRelatedAnimation() || Translate() || Rotate() ||
           Scale();
  }
  bool HasTransformOperations() const {
    return !Transform().Operations().empty();
  }
  ETransformStyle3D UsedTransformStyle3D() const {
    if (TransformStyle3D() == ETransformStyle3D::kFlat) {
      return ETransformStyle3D::kFlat;
    }

    // Even if the user specified transform-style: preserves-3d (which is
    // non-default), it could be overridden by a number of other properties.
    // These checks are fairly expensive (since there are so many of them),
    // so we only bother going through them if needed.
    DCHECK_EQ(TransformStyle3D(), ETransformStyle3D::kPreserve3d);
    return HasGroupingPropertyForUsedTransformStyle3D()
               ? ETransformStyle3D::kFlat
               : ETransformStyle3D::kPreserve3d;
  }
  // Returns whether the transform operations for |otherStyle| differ from the
  // operations for this style instance. Note that callers may want to also
  // check hasTransform(), as it is possible for two styles to have matching
  // transform operations but differ in other transform-impacting style
  // respects.
  bool TransformDataEquivalent(const ComputedStyle& other) const {
    return !DiffTransformData(*this, other);
  }
  bool Preserves3D() const {
    return UsedTransformStyle3D() != ETransformStyle3D::kFlat;
  }
  enum ApplyTransformOrigin {
    kIncludeTransformOrigin,
    kExcludeTransformOrigin
  };
  enum ApplyMotionPath { kIncludeMotionPath, kExcludeMotionPath };
  enum ApplyIndependentTransformProperties {
    kIncludeIndependentTransformProperties,
    kExcludeIndependentTransformProperties
  };
  enum ApplyTransformOperations {
    kIncludeTransformOperations,
    kExcludeTransformOperations
  };
  void ApplyTransform(gfx::Transform&,
                      const LayoutBox* box,
                      const PhysicalRect& reference_box,
                      ApplyTransformOperations,
                      ApplyTransformOrigin,
                      ApplyMotionPath,
                      ApplyIndependentTransformProperties) const;
  void ApplyTransform(gfx::Transform&,
                      const LayoutBox* box,
                      const gfx::RectF& reference_box,
                      ApplyTransformOperations,
                      ApplyTransformOrigin,
                      ApplyMotionPath,
                      ApplyIndependentTransformProperties) const;

  // Returns |true| if any property that renders using filter operations is
  // used (including, but not limited to, 'filter' and 'box-reflect').
  bool HasFilterInducingProperty() const {
    return HasNonInitialFilter() || HasBoxReflect();
  }

  // Returns |true| if filter should be considered to have non-initial value
  // for the purposes of containing blocks.
  bool HasNonInitialFilter() const {
    return HasFilter() || HasWillChangeFilterHint();
  }

  // Returns |true| if backdrop-filter should be considered to have non-initial
  // value for the purposes of containing blocks.
  bool HasNonInitialBackdropFilter() const {
    return HasBackdropFilter() || HasWillChangeBackdropFilterHint();
  }

  // Returns |true| if opacity should be considered to have non-initial value
  // for the purpose of creating stacking contexts.
  bool HasNonInitialOpacity() const {
    return HasOpacity() || HasWillChangeOpacityHint() ||
           HasCurrentOpacityAnimation();
  }

  // Returns whether this style contains any grouping property as defined by
  // https://drafts.csswg.org/css-transforms-2/#grouping-property-values.
  //
  // |has_box_reflection| is a parameter instead of checking |BoxReflect()|
  // because box reflection styles only apply for some objects (see:
  // |LayoutObject::HasReflection()|).
  bool HasGroupingProperty(bool has_box_reflection) const {
    if (HasStackingGroupingProperty(has_box_reflection)) {
      return true;
    }
    // TODO(pdr): Also check for overflow because the spec requires "overflow:
    // any value other than visible or clip."
    if (!HasAutoClip() && HasOutOfFlowPosition()) {
      return true;
    }
    return false;
  }

  // This is the subset of grouping properties (see: |HasGroupingProperty|) that
  // also create stacking contexts.
  bool HasStackingGroupingProperty(bool has_box_reflection) const {
    if (HasNonInitialOpacity()) {
      return true;
    }
    if (HasNonInitialFilter()) {
      return true;
    }
    if (has_box_reflection) {
      return true;
    }
    if (HasClipPath()) {
      return true;
    }
    if (HasIsolation()) {
      return true;
    }
    if (HasMask()) {
      return true;
    }
    if (HasBlendMode()) {
      return true;
    }
    if (HasNonInitialBackdropFilter()) {
      return true;
    }
    if (ViewTransitionName() || ElementIsViewTransitionParticipant()) {
      return true;
    }
    return false;
  }

  // Grouping requires creating a flattened representation of the descendant
  // elements before they can be applied, and therefore force the element to
  // have a used style of flat for preserve-3d.
  CORE_EXPORT bool HasGroupingPropertyForUsedTransformStyle3D() const {
    return HasGroupingProperty(BoxReflect()) ||
           !IsOverflowVisibleAlongBothAxes();
  }

  // Return true if any transform related property (currently
  // transform/motionPath, transformStyle3D, perspective, or
  // will-change:transform) indicates that we are transforming.
  // will-change:transform should result in the same rendering behavior as
  // having a transform, including the creation of a containing block for fixed
  // position descendants.
  bool HasTransformRelatedProperty() const {
    return HasTransform() || Preserves3D() || HasPerspective() ||
           HasWillChangeHintForAnyTransformProperty();
  }
  bool HasTransformRelatedPropertyForSVG() const {
    return HasTransform() || HasWillChangeHintForAnyTransformProperty();
  }

  // Return true if this style has properties ('filter', 'clip-path' and 'mask')
  // that applies an effect to SVG elements.
  bool HasSVGEffect() const {
    return HasFilter() || HasClipPath() || MaskerResource();
  }

  // Returns true if any property has an <image> value that is a CSS paint
  // function that is using a given custom property.
  bool HasCSSPaintImagesUsingCustomProperty(
      const AtomicString& custom_property_name,
      const Document&) const;

  // FIXME: reflections should belong to this helper function but they are
  // currently handled through their self-painting layers. So the layout code
  // doesn't account for them.
  bool HasVisualOverflowingEffect() const {
    return BoxShadow() || HasBorderImageOutsets() || HasOutline() ||
           HasMaskBoxImageOutsets();
  }

  bool IsStackedWithoutContainment() const {
    return IsStackingContextWithoutContainment() ||
           GetPosition() != EPosition::kStatic;
  }

  // Pseudo element styles.
  static bool HasPseudoElementStyle(unsigned pseudo_styles, PseudoId pseudo) {
    DCHECK(pseudo >= kFirstPublicPseudoId);
    DCHECK(pseudo <= kLastTrackedPublicPseudoId);
    return (1 << (pseudo - kFirstPublicPseudoId)) & pseudo_styles;
  }

  bool HasAnyPseudoElementStyles() const;
  bool HasAnyHighlightPseudoElementStyles() const;
  bool HasPseudoElementStyle(PseudoId pseudo) const {
    return ComputedStyle::HasPseudoElementStyle(PseudoElementStylesInternal(),
                                                pseudo);
  }

  // Note: CanContainAbsolutePositionObjects should return true if
  // CanContainFixedPositionObjects.  We currently never use this value
  // directly, always OR'ing it with CanContainFixedPositionObjects.
  bool CanContainAbsolutePositionObjects() const {
    return GetPosition() != EPosition::kStatic;
  }

  // Whitespace utility functions.
  // Don't use these `EWhiteSpace` static functions directly, because the
  // `white-space` property may become a shorthand in future.
  // https://drafts.csswg.org/css-text-4/#white-space-property
  static bool DeprecatedAutoWrap(EWhiteSpace ws) {
    return blink::ShouldWrapLine(ToTextWrap(ws));
  }
  static bool DeprecatedPreserveNewline(EWhiteSpace ws) {
    return blink::ShouldPreserveBreaks(ToWhiteSpaceCollapse(ws));
  }
  static bool DeprecatedCollapseWhiteSpace(EWhiteSpace ws) {
    return blink::ShouldCollapseWhiteSpaces(ToWhiteSpaceCollapse(ws));
  }

  // This function may return values not defined as the enum values. See
  // `EWhiteSpace`. Prefer using semantic functions below.
  EWhiteSpace WhiteSpace() const {
    return ToWhiteSpace(GetWhiteSpaceCollapse(), GetTextWrap());
  }

  // Semantic functions for the `white-space` property and its longhands.
  bool ShouldPreserveWhiteSpaces() const {
    return blink::ShouldPreserveWhiteSpaces(GetWhiteSpaceCollapse());
  }
  bool ShouldCollapseWhiteSpaces() const {
    return blink::ShouldCollapseWhiteSpaces(GetWhiteSpaceCollapse());
  }
  bool ShouldPreserveBreaks() const {
    return blink::ShouldPreserveBreaks(GetWhiteSpaceCollapse());
  }
  bool ShouldCollapseBreaks() const {
    return blink::ShouldCollapseBreaks(GetWhiteSpaceCollapse());
  }
  bool IsCollapsibleWhiteSpace(UChar c) const {
    switch (c) {
      case ' ':
      case '\t':
        return ShouldCollapseWhiteSpaces();
      case '\n':
        return ShouldCollapseBreaks();
    }
    return false;
  }

  bool ShouldWrapLine() const { return blink::ShouldWrapLine(GetTextWrap()); }
  bool ShouldBreakSpaces() const {
    return blink::ShouldBreakSpaces(GetWhiteSpaceCollapse());
  }
  bool ShouldBreakOnlyAfterWhiteSpace() const {
    return (ShouldPreserveWhiteSpaces() && ShouldWrapLine()) ||
           GetLineBreak() == LineBreak::kAfterWhiteSpace;
  }
  bool NeedsTrailingSpace() const {
    return ShouldBreakOnlyAfterWhiteSpace() && ShouldWrapLine();
  }

  bool ShouldBreakWords() const {
    return (WordBreak() == EWordBreak::kBreakWord ||
            OverflowWrap() != EOverflowWrap::kNormal) &&
           ShouldWrapLine();
  }

  // Text direction utility functions.
  bool ShouldPlaceBlockDirectionScrollbarOnLogicalLeft() const {
    return !IsLeftToRightDirection() && IsHorizontalWritingMode();
  }

  // Border utility functions.
  static bool BorderStyleIsVisible(EBorderStyle style) {
    return style != EBorderStyle::kNone && style != EBorderStyle::kHidden;
  }
  bool BorderObscuresBackground() const;
  void GetBorderEdgeInfo(
      BorderEdge edges[],
      PhysicalBoxSides sides_to_include = PhysicalBoxSides()) const;

  bool HasBoxDecorations() const {
    return HasBorderDecoration() || HasBorderRadius() || HasOutline() ||
           HasEffectiveAppearance() || BoxShadow() ||
           HasFilterInducingProperty() || HasNonInitialBackdropFilter() ||
           HasResize();
  }

  // "Box decoration background" includes all box decorations and backgrounds
  // that are painted as the background of the object. It includes borders,
  // box-shadows, background-color and background-image, etc.
  bool HasBoxDecorationBackground() const {
    return HasBackground() || HasBorderDecoration() ||
           HasEffectiveAppearance() || BoxShadow();
  }

  PhysicalBoxStrut BoxDecorationOutsets() const;

  // Background utility functions.
  const FillLayer& BackgroundLayers() const { return BackgroundInternal(); }
  bool HasBackgroundRelatedColorReferencingCurrentColor() const {
    if (BackgroundColor().IsCurrentColor() ||
        InternalVisitedBackgroundColor().IsCurrentColor() ||
        InternalForcedBackgroundColor().IsCurrentColor()) {
      return true;
    }
    if (!BoxShadow()) {
      return false;
    }
    return ShadowListHasCurrentColor(BoxShadow());
  }

  CORE_EXPORT bool HasBackground() const;

  // Color utility functions.
  CORE_EXPORT blink::Color VisitedDependentColor(
      const Longhand& color_property,
      bool* is_current_color = nullptr) const;

  // A faster version of VisitedDependentColor() that specializes on the
  // concrete property class; for the common case of not being inside a link,
  // can inline the question in function directly. However, it uses more code
  // space, so only use it on code paths that are actually hot.
  template <class Property>
  blink::Color VisitedDependentColorFast(
      const Property& color_property,
      bool* is_current_color = nullptr) const {
    DCHECK(!Property().IsVisited());

    if (InsideLink() != EInsideLink::kInsideVisitedLink) {
      blink::Color color =
          color_property.ColorIncludingFallback(false, *this,
                                                /*is_current_color=*/nullptr);
      DCHECK(color == VisitedDependentColor(color_property, is_current_color));
      return color;
    } else {
      return VisitedDependentColor(color_property, is_current_color);
    }
  }

  // -webkit-appearance utility functions.
  static bool HasEffectiveAppearance(ControlPart effective_appearance) {
    return effective_appearance != kNoControlPart;
  }
  bool HasEffectiveAppearance() const {
    return HasEffectiveAppearance(EffectiveAppearance());
  }
  bool IsCheckboxOrRadioPart() const {
    return HasEffectiveAppearance() &&
           (EffectiveAppearance() == kCheckboxPart ||
            EffectiveAppearance() == kRadioPart);
  }

  // Other utility functions.
  bool RequireTransformOrigin(ApplyTransformOrigin apply_origin,
                              ApplyMotionPath) const;

  InterpolationQuality GetInterpolationQuality() const;

  bool CanGeneratePseudoElement(PseudoId pseudo) const {
    if (Display() == EDisplay::kNone) {
      return false;
    }
    if (IsEnsuredInDisplayNone()) {
      return false;
    }
    if (pseudo == kPseudoIdMarker) {
      return IsDisplayListItem();
    }
    if (pseudo == kPseudoIdBackdrop && Overlay() == EOverlay::kNone) {
      return false;
    }
    if (!HasPseudoElementStyle(pseudo)) {
      return false;
    }
    if (Display() != EDisplay::kContents) {
      return true;
    }
    // For display: contents elements, we still need to generate ::before and
    // ::after, but the rest of the pseudo-elements should only be used for
    // elements with an actual layout object.
    return pseudo == kPseudoIdBefore || pseudo == kPseudoIdAfter;
  }

  // Returns true if the element is rendered in the top layer. That is the case
  // when the overlay property computes to 'auto', or when the element is a
  // ::backdrop pseudo.
  bool IsRenderedInTopLayer(const Element& element) const;

  // Load the images of CSS properties that were deferred by LazyLoad.
  void LoadDeferredImages(Document&) const;

  static mojom::blink::ColorScheme UsedColorScheme(bool is_dark_color_scheme) {
    return is_dark_color_scheme ? mojom::blink::ColorScheme::kDark
                                : mojom::blink::ColorScheme::kLight;
  }
  mojom::blink::ColorScheme UsedColorScheme() const {
    return UsedColorScheme(DarkColorScheme());
  }

  bool GeneratesMarkerImage() const {
    return IsDisplayListItem() && ListStyleImage() &&
           !ListStyleImage()->ErrorOccurred() &&
           (ListStyleImage()->IsLoading() || ListStyleImage()->IsLoaded());
  }

  LogicalSize LogicalAspectRatio() const {
    DCHECK_NE(AspectRatio().GetType(), EAspectRatioType::kAuto);
    return AspectRatio().GetLayoutRatio().ConvertToLogical(GetWritingMode());
  }

  EBoxSizing BoxSizingForAspectRatio() const {
    if (AspectRatio().GetType() == EAspectRatioType::kRatio) {
      return BoxSizing();
    }
    return EBoxSizing::kContentBox;
  }

  bool ForceDark() const { return DarkColorScheme() && ColorSchemeForced(); }

  bool HasStaticViewportUnits() const {
    return ViewportUnitFlags() &
           static_cast<unsigned>(ViewportUnitFlag::kStatic);
  }
  bool HasDynamicViewportUnits() const {
    return ViewportUnitFlags() &
           static_cast<unsigned>(ViewportUnitFlag::kDynamic);
  }
  bool HasViewportUnits() const { return ViewportUnitFlags(); }

  bool OverflowClipMarginHasAnEffect() const {
    return OverflowClipMargin() &&
           (OverflowClipMargin()->GetReferenceBox() !=
                StyleOverflowClipMargin::ReferenceBox::kPaddingBox ||
            OverflowClipMargin()->GetMargin() != LayoutUnit());
  }

  // Field-sizing utility function
  bool ApplyControlFixedSize() const {
    return FieldSizing() == EFieldSizing::kFixed;
  }

 private:
  bool IsInlineSizeContainer() const {
    return ContainerType() & kContainerTypeInlineSize;
  }
  bool IsBlockSizeContainer() const {
    return ContainerType() & kContainerTypeBlockSize;
  }
  bool IsInlineOrBlockSizeContainer() const {
    return ContainerType() & kContainerTypeSize;
  }
  bool IsSizeContainer() const {
    return (ContainerType() & kContainerTypeSize) == kContainerTypeSize;
  }
  bool IsStickyContainer() const {
    return ContainerType() & kContainerTypeSticky;
  }
  bool IsSnapContainer() const { return ContainerType() & kContainerTypeSnap; }

  static bool IsDisplayBlockContainer(EDisplay display) {
    return display == EDisplay::kBlock || display == EDisplay::kListItem ||
           display == EDisplay::kInlineBlock ||
           display == EDisplay::kFlowRoot ||
           display == EDisplay::kInlineFlowRootListItem ||
           display == EDisplay::kTableCell ||
           display == EDisplay::kTableCaption;
  }

  static bool IsDisplayTableBox(EDisplay display) {
    return display == EDisplay::kTable || display == EDisplay::kInlineTable;
  }

  static bool IsDisplayFlexibleBox(EDisplay display) {
    return display == EDisplay::kFlex || display == EDisplay::kInlineFlex;
  }

  static bool IsDisplayGridBox(EDisplay display) {
    return display == EDisplay::kGrid || display == EDisplay::kInlineGrid;
  }

  static bool IsDisplayMathBox(EDisplay display) {
    return display == EDisplay::kMath || display == EDisplay::kBlockMath;
  }

  static bool IsDisplayLayoutCustomBox(EDisplay display) {
    return display == EDisplay::kLayoutCustom ||
           display == EDisplay::kInlineLayoutCustom;
  }

  static bool IsDisplayReplacedType(EDisplay display) {
    return display == EDisplay::kInlineBlock ||
           display == EDisplay::kInlineFlowRootListItem ||
           display == EDisplay::kWebkitInlineBox ||
           display == EDisplay::kInlineFlex ||
           display == EDisplay::kInlineTable ||
           display == EDisplay::kInlineGrid || display == EDisplay::kMath ||
           display == EDisplay::kInlineLayoutCustom;
  }

  static bool IsDisplayInlineType(EDisplay display) {
    return display == EDisplay::kInline ||
           display == EDisplay::kInlineListItem ||
           IsDisplayReplacedType(display);
  }

  static bool IsDisplayTableType(EDisplay display) {
    return display == EDisplay::kTable || display == EDisplay::kInlineTable ||
           display == EDisplay::kTableRowGroup ||
           display == EDisplay::kTableHeaderGroup ||
           display == EDisplay::kTableFooterGroup ||
           display == EDisplay::kTableRow ||
           display == EDisplay::kTableColumnGroup ||
           display == EDisplay::kTableColumn ||
           display == EDisplay::kTableCell ||
           display == EDisplay::kTableCaption;
  }

  bool InternalVisitedBorderLeftColorHasNotChanged(
      const ComputedStyle& other) const {
    return (InternalVisitedBorderLeftColor() ==
                other.InternalVisitedBorderLeftColor() ||
            !BorderLeftWidth());
  }
  bool InternalVisitedBorderRightColorHasNotChanged(
      const ComputedStyle& other) const {
    return (InternalVisitedBorderRightColor() ==
                other.InternalVisitedBorderRightColor() ||
            !BorderRightWidth());
  }
  bool InternalVisitedBorderBottomColorHasNotChanged(
      const ComputedStyle& other) const {
    return (InternalVisitedBorderBottomColor() ==
                other.InternalVisitedBorderBottomColor() ||
            !BorderBottomWidth());
  }
  bool InternalVisitedBorderTopColorHasNotChanged(
      const ComputedStyle& other) const {
    return (InternalVisitedBorderTopColor() ==
                other.InternalVisitedBorderTopColor() ||
            !BorderTopWidth());
  }

  bool InternalVisitedOutlineColorHasNotChanged(
      const ComputedStyle& other) const {
    return (InternalVisitedOutlineColor() ==
                other.InternalVisitedOutlineColor() ||
            !OutlineWidth());
  }

  StyleColor DecorationColorIncludingFallback(bool visited_link) const;

  bool HasAppearance() const { return Appearance() != kNoControlPart; }

  void ApplyMotionPathTransform(float origin_x,
                                float origin_y,
                                const LayoutBox* box,
                                const gfx::RectF& bounding_box,
                                gfx::Transform&) const;
  PointAndTangent CalculatePointAndTangentOnBasicShape(
      const BasicShape& shape,
      const gfx::PointF& starting_point,
      const gfx::SizeF& reference_box_size) const;
  PointAndTangent CalculatePointAndTangentOnRay(
      const StyleRay& ray,
      const LayoutBox* box,
      const gfx::PointF& starting_point,
      const gfx::SizeF& reference_box_size) const;
  PointAndTangent CalculatePointAndTangentOnPath(const Path& path) const;

  bool ScrollAnchorDisablingPropertyChanged(const ComputedStyle& other,
                                            const StyleDifference&) const;
  bool DiffNeedsFullLayoutAndPaintInvalidation(
      const ComputedStyle& other) const;
  bool DiffNeedsFullLayout(const Document&, const ComputedStyle& other) const;
  bool DiffNeedsFullLayoutForLayoutCustom(const Document&,
                                          const ComputedStyle& other) const;
  bool DiffNeedsFullLayoutForLayoutCustomChild(
      const Document&,
      const ComputedStyle& other) const;
  void AdjustDiffForNeedsPaintInvalidation(const ComputedStyle& other,
                                           StyleDifference&,
                                           const Document&) const;
  bool DiffNeedsPaintInvalidationForPaintImage(const StyleImage&,
                                               const ComputedStyle& other,
                                               const Document&) const;
  CORE_EXPORT void UpdatePropertySpecificDifferences(const ComputedStyle& other,
                                                     StyleDifference&) const;
  bool PotentialCompositingReasonsFor3DTransformChanged(
      const ComputedStyle& other) const;

  bool PropertiesEqual(const Vector<CSSPropertyID>& properties,
                       const ComputedStyle& other) const;
  CORE_EXPORT bool CustomPropertiesEqual(const Vector<AtomicString>& properties,
                                         const ComputedStyle& other) const;

  blink::Color GetCurrentColor(bool* is_current_color = nullptr) const;
  blink::Color GetInternalVisitedCurrentColor(
      bool* is_current_color = nullptr) const;
  blink::Color GetInternalForcedCurrentColor(
      bool* is_current_color = nullptr) const;
  blink::Color GetInternalForcedVisitedCurrentColor(
      bool* is_current_color = nullptr) const;

  // Helper for resolving a StyleColor which may contain currentColor or a
  // system color keyword. This is intended for cases where a given property
  // consists of a StyleColor plus additional information. For <color>
  // properties, prefer VisitedDependentColor() or
  // Longhand::ColorIncludingFallback() instead.
  blink::Color ResolvedColor(const StyleColor& color,
                             bool* is_current_color = nullptr) const;

  static bool ShadowListHasCurrentColor(const ShadowList*);

  PhysicalToLogical<const Length&> PhysicalMarginToLogical(
      const ComputedStyle& other) const {
    return PhysicalToLogical<const Length&>(other.GetWritingDirection(),
                                            MarginTop(), MarginRight(),
                                            MarginBottom(), MarginLeft());
  }

  PhysicalToLogical<const Length&> PhysicalPaddingToLogical() const {
    return PhysicalToLogical<const Length&>(GetWritingDirection(), PaddingTop(),
                                            PaddingRight(), PaddingBottom(),
                                            PaddingLeft());
  }

  PhysicalToLogical<BorderValue> PhysicalBorderToLogical(
      const ComputedStyle& other) const {
    return PhysicalToLogical<BorderValue>(other.GetWritingDirection(),
                                          BorderTop(), BorderRight(),
                                          BorderBottom(), BorderLeft());
  }

  PhysicalToLogical<LayoutUnit> PhysicalBorderWidthToLogical() const {
    return PhysicalToLogical<LayoutUnit>(
        GetWritingDirection(), BorderTopWidth(), BorderRightWidth(),
        BorderBottomWidth(), BorderLeftWidth());
  }

  PhysicalToLogical<EBorderStyle> PhysicalBorderStyleToLogical() const {
    return PhysicalToLogical<EBorderStyle>(
        GetWritingDirection(), BorderTopStyle(), BorderRightStyle(),
        BorderBottomStyle(), BorderLeftStyle());
  }

  PhysicalToLogical<const Length&> PhysicalBoundsToLogical() const {
    return PhysicalToLogical<const Length&>(GetWritingDirection(), UsedTop(),
                                            UsedRight(), UsedBottom(),
                                            UsedLeft());
  }

  static Difference ComputeDifferenceIgnoringInheritedFirstLineStyle(
      const ComputedStyle& old_style,
      const ComputedStyle& new_style);

  static bool ShouldForceColor(bool in_forced_colors_mode,
                               EForcedColorAdjust forced_color_adjust,
                               const StyleColor& unforced_color) {
    return in_forced_colors_mode &&
           forced_color_adjust == EForcedColorAdjust::kAuto &&
           !unforced_color.IsSystemColorIncludingDeprecated();
  }
  bool ShouldForceColor(const StyleColor& unforced_color) const {
    // If any other properties are added that are affected by ForcedColors mode,
    // adjust EditingStyle::RemoveForcedColorsIfNeeded and
    // EditingStyle::MergeStyleFromRulesForSerialization accordingly.
    return ShouldForceColor(InForcedColorsMode(), ForcedColorAdjust(),
                            unforced_color);
  }

  // Derived flags:
  bool CalculateIsStackingContextWithoutContainment() const;

  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesRespectsTransformAnimation);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsTransform);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesRespectsScaleAnimation);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesRespectsRotateAnimation);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesRespectsTranslateAnimation);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsOpacity);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsFilter);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsBackdropFilter);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsInlineTransform);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsBackfaceVisibility);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsWillChange);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsUsedStylePreserve3D);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsOverflow);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsContainsPaint);
  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest,
                           UpdatePropertySpecificDifferencesHasAlpha);
  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest, CustomPropertiesEqual_Values);
  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest, CustomPropertiesEqual_Data);
  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest, InitialVariableNames);
  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest,
                           InitialAndInheritedAndNonInheritedVariableNames);
  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest,
                           GetVariableNamesWithInitialData_Invalidation);
  FRIEND_TEST_ALL_PREFIXES(StyleCascadeTest, ForcedVisitedBackgroundColor);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      TextDecorationEqualDoesNotRequireRecomputeInkOverflow);
  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest,
                           TextDecorationNotEqualRequiresRecomputeInkOverflow);
  FRIEND_TEST_ALL_PREFIXES(StyleEngineTest, ScrollbarStyleNoExcessiveCaching);
  FRIEND_TEST_ALL_PREFIXES(PermissionShadowElementTest,
                           PropagateCSSPropertyInnerElement);
};

inline bool ComputedStyle::HasAnyPseudoElementStyles() const {
  return !!PseudoElementStylesInternal();
}

inline bool ComputedStyle::HasAnyHighlightPseudoElementStyles() const {
  static_assert(kPseudoIdSelection >= kFirstPublicPseudoId &&
                    kPseudoIdSelection <= kLastTrackedPublicPseudoId,
                "kPseudoIdSelection must be public");
  static_assert(kPseudoIdTargetText >= kFirstPublicPseudoId &&
                    kPseudoIdTargetText <= kLastTrackedPublicPseudoId,
                "kPseudoIdTargetText must be public");
  static_assert(kPseudoIdSpellingError >= kFirstPublicPseudoId &&
                    kPseudoIdSpellingError <= kLastTrackedPublicPseudoId,
                "kPseudoIdSpellingError must be public");
  static_assert(kPseudoIdGrammarError >= kFirstPublicPseudoId &&
                    kPseudoIdGrammarError <= kLastTrackedPublicPseudoId,
                "kPseudoIdGrammarError must be public");
  static_assert(kPseudoIdHighlight >= kFirstPublicPseudoId &&
                    kPseudoIdHighlight <= kLastTrackedPublicPseudoId,
                "kPseudoIdHighlight must be public");

  const unsigned mask = (1 << (kPseudoIdSelection - kFirstPublicPseudoId)) |
                        (1 << (kPseudoIdTargetText - kFirstPublicPseudoId)) |
                        (1 << (kPseudoIdSpellingError - kFirstPublicPseudoId)) |
                        (1 << (kPseudoIdGrammarError - kFirstPublicPseudoId)) |
                        (1 << (kPseudoIdHighlight - kFirstPublicPseudoId));

  return mask & PseudoElementStylesInternal();
}

class ComputedStyleBuilder final : public ComputedStyleBuilderBase {
  STACK_ALLOCATED();

 public:
  friend class ColorPropertyFunctions;
  // Access to Appearance().
  friend class LayoutTheme;
  friend class StyleAdjuster;
  friend class StyleResolverState;
  // Access to UserModify().
  friend class MatchedPropertiesCache;

  // Creates a new ComputedStyle based on the given initial style.
  CORE_EXPORT explicit ComputedStyleBuilder(const ComputedStyle& style);

  // Creates a new ComputedStyle based on the given initial style,
  // but with all inheritable properties from the given parent style.
  CORE_EXPORT ComputedStyleBuilder(
      const ComputedStyle& initial_style,
      const ComputedStyle& parent_style,
      IsAtShadowBoundary is_at_shadow_boundary = kNotAtShadowBoundary);

  ComputedStyleBuilder(const ComputedStyleBuilder& builder) = delete;
  ComputedStyleBuilder(ComputedStyleBuilder&&) = default;
  ComputedStyleBuilder& operator=(const ComputedStyleBuilder&) = delete;
  ComputedStyleBuilder& operator=(ComputedStyleBuilder&&) = default;

  CORE_EXPORT const ComputedStyle* TakeStyle();

  // NOTE: Prefer `TakeStyle()` if possible.
  CORE_EXPORT const ComputedStyle* CloneStyle() const;

  // Copies the values of any independent inherited properties from the parent
  // that are not explicitly set in this style.
  void PropagateIndependentInheritedProperties(
      const ComputedStyle& parent_style);

  // Pseudo-elements
  bool HasPseudoElementStyle(PseudoId pseudo) const {
    return ComputedStyle::HasPseudoElementStyle(PseudoElementStylesInternal(),
                                                pseudo);
  }

  // animations
  const CSSAnimationData* Animations() const {
    return AnimationsInternal().get();
  }
  CORE_EXPORT CSSAnimationData& AccessAnimations() {
    std::unique_ptr<CSSAnimationData>& animations = MutableAnimationsInternal();
    if (!animations) {
      animations = std::make_unique<CSSAnimationData>();
    }
    return *animations;
  }

  // appearance
  bool HasEffectiveAppearance() const {
    return ComputedStyle::HasEffectiveAppearance(EffectiveAppearance());
  }

  // backdrop-filter
  FilterOperations::FilterOperationVector& MutableBackdropFilterOperations() {
    return MutableBackdropFilterInternal().Operations();
  }
  bool HasBackdropFilter() const {
    return ComputedStyle::HasBackdropFilter(BackdropFilter());
  }

  // background
  FillLayer& AccessBackgroundLayers() { return MutableBackgroundInternal(); }
  void AdjustBackgroundLayers() {
    if (BackgroundInternal().Next()) {
      AccessBackgroundLayers().CullEmptyLayers();
      AccessBackgroundLayers().FillUnsetProperties();
    }
  }
  bool HasUrlBackgroundImage() const {
    return BackgroundInternal().AnyLayerHasUrlImage();
  }
  void ClearBackgroundImage();

  // border-*-width
  LayoutUnit BorderTopWidth() const {
    return ComputedStyle::BorderWidth(BorderTopStyle(),
                                      BorderTopWidthInternal());
  }
  LayoutUnit BorderBottomWidth() const {
    return ComputedStyle::BorderWidth(BorderBottomStyle(),
                                      BorderBottomWidthInternal());
  }
  LayoutUnit BorderLeftWidth() const {
    return ComputedStyle::BorderWidth(BorderLeftStyle(),
                                      BorderLeftWidthInternal());
  }
  LayoutUnit BorderRightWidth() const {
    return ComputedStyle::BorderWidth(BorderRightStyle(),
                                      BorderRightWidthInternal());
  }

  // border-image-*
  void SetBorderImageOutset(const BorderImageLengthBox& outset) {
    if (BorderImage().Outset() == outset) {
      return;
    }
    MutableBorderImageInternal().SetOutset(outset);
  }
  void SetBorderImageSlices(const LengthBox& slices) {
    if (BorderImage().ImageSlices() == slices) {
      return;
    }
    MutableBorderImageInternal().SetImageSlices(slices);
  }
  void SetBorderImageSlicesFill(bool fill) {
    if (BorderImage().Fill() == fill) {
      return;
    }
    MutableBorderImageInternal().SetFill(fill);
  }
  void SetBorderImageSource(StyleImage* image) {
    if (BorderImage().GetImage() == image) {
      return;
    }
    MutableBorderImageInternal().SetImage(image);
  }
  void SetBorderImageWidth(const BorderImageLengthBox& slices) {
    if (BorderImage().BorderSlices() == slices) {
      return;
    }
    MutableBorderImageInternal().SetBorderSlices(slices);
  }

  // clip
  void SetClip(const LengthBox& box) {
    SetHasAutoClipInternal(false);
    SetClipInternal(box);
  }
  void SetHasAutoClip() {
    SetHasAutoClipInternal(true);
    SetClipInternal(ComputedStyleInitialValues::InitialClip());
  }

  // clip-patch
  void SetClipPath(ClipPathOperation* clip_path) {
    SetHasClipPath(clip_path);
    SetClipPathInternal(clip_path);
  }

  // color
  blink::Color GetCurrentColor() const {
    return Color().Resolve(blink::Color(), UsedColorScheme());
  }

  // column-count
  void SetColumnCount(uint16_t c) {
    SetHasAutoColumnCountInternal(false);
    SetColumnCountInternal(ClampTo<uint16_t>(c, 1));
  }
  void SetHasAutoColumnCount() {
    SetHasAutoColumnCountInternal(true);
    SetColumnCountInternal(ComputedStyleInitialValues::InitialColumnCount());
  }

  // column-rule-width
  void SetColumnRuleWidth(uint16_t w) {
    SetColumnRuleWidthInternal(LayoutUnit(w));
  }

  // column-width
  void SetColumnWidth(float f) {
    SetHasAutoColumnWidthInternal(false);
    SetColumnWidthInternal(f);
  }
  void SetHasAutoColumnWidth() {
    SetHasAutoColumnWidthInternal(true);
    SetColumnWidthInternal(0);
  }

  // contain
  bool ShouldApplyAnyContainment(const Element& element) const {
    unsigned effective_containment = ComputedStyle::EffectiveContainment(
        Contain(), ContainerType(), ContentVisibility(), SkipsContents());
    return ComputedStyle::ShouldApplyAnyContainment(element, GetDisplayStyle(),
                                                    effective_containment);
  }

  // content
  ContentData* GetContentData() const { return ContentInternal().Get(); }

  // counter-*
  CounterDirectiveMap& AccessCounterDirectives() {
    std::unique_ptr<CounterDirectiveMap>& map =
        MutableCounterDirectivesInternal();
    if (!map) {
      map = std::make_unique<CounterDirectiveMap>();
    }
    return *map;
  }
  void ClearIncrementDirectives() {
    if (const auto& map = MutableCounterDirectivesInternal()) {
      for (auto& value_pair : *map) {
        value_pair.value.ClearIncrement();
      }
    }
  }
  void ClearResetDirectives() {
    if (const auto& map = MutableCounterDirectivesInternal()) {
      for (auto& value_pair : *map) {
        value_pair.value.ClearReset();
      }
    }
  }
  void ClearSetDirectives() {
    if (const auto& map = MutableCounterDirectivesInternal()) {
      for (auto& value_pair : *map) {
        value_pair.value.ClearSet();
      }
    }
  }

  // cursor
  void AddCursor(StyleImage* image,
                 bool hot_spot_specified,
                 const gfx::Point& hot_spot = gfx::Point()) {
    if (!CursorDataInternal()) {
      SetCursorDataInternal(MakeGarbageCollected<CursorList>());
    }
    MutableCursorDataInternal()->push_back(
        CursorData(image, hot_spot_specified, hot_spot));
  }
  void SetCursorList(CursorList* list) { SetCursorDataInternal(list); }
  void ClearCursorList() {
    if (CursorDataInternal()) {
      SetCursorDataInternal(nullptr);
    }
  }
  CursorList* Cursors() const { return CursorDataInternal().Get(); }

  // display
  bool IsDisplayInlineType() const {
    return ComputedStyle::IsDisplayInlineType(Display());
  }
  bool IsDisplayReplacedType() const {
    return ComputedStyle::IsDisplayReplacedType(Display());
  }
  bool IsDisplayMathType() const {
    return ComputedStyle::IsDisplayMathBox(Display());
  }
  bool IsDisplayTableBox() const {
    return ComputedStyle::IsDisplayTableBox(Display());
  }
  bool IsDisplayTableRowOrColumnType() const {
    return Display() == EDisplay::kTableRow ||
           Display() == EDisplay::kTableRowGroup ||
           Display() == EDisplay::kTableColumn ||
           Display() == EDisplay::kTableColumnGroup;
  }
  DisplayStyle GetDisplayStyle() const {
    return DisplayStyle(Display(), StyleType(), GetContentData());
  }

  // filter
  FilterOperations::FilterOperationVector& MutableFilterOperations() {
    return MutableFilterInternal().Operations();
  }

  // float
  bool IsFloating() const { return Floating() != EFloat::kNone; }

  // font
  void SetFontDescription(const FontDescription& v) {
    if (GetFont().GetFontDescription() != v) {
      SetFont(Font(v, GetFont().GetFontSelector()));
    }
  }
  const FontDescription& GetFontDescription() const {
    return GetFont().GetFontDescription();
  }
  int FontSize() const { return GetFontDescription().ComputedPixelSize(); }
  LayoutUnit FontHeight() const {
    if (const SimpleFontData* font_data = GetFont().PrimaryFont()) {
      return LayoutUnit(font_data->GetFontMetrics().Height());
    }
    return LayoutUnit();
  }
  FontOrientation ComputeFontOrientation() const;
  void UpdateFontOrientation();

  FontSizeStyle GetFontSizeStyle() const {
    return FontSizeStyle(GetFont(), LineHeightInternal(), EffectiveZoom());
  }

  // letter-spacing
  void SetLetterSpacing(float letter_spacing) {
    FontDescription description(GetFontDescription());
    description.SetLetterSpacing(letter_spacing);
    SetFontDescription(description);
  }

  // line-height
  bool HasInitialLineHeight() const {
    return LineHeightInternal() ==
           ComputedStyleInitialValues::InitialLineHeight();
  }
  const Length& LineHeight() const { return LineHeightInternal(); }

  // Sizing properties
  const Length& UsedWidth() const {
    if (LIKELY(!Width().HasAnchorQueries())) {
      return Width();
    }
    return HasOutOfFlowPosition() ? Width() : Length::Auto();
  }
  const Length& UsedHeight() const {
    if (LIKELY(!Height().HasAnchorQueries())) {
      return Height();
    }
    return HasOutOfFlowPosition() ? Height() : Length::Auto();
  }

  // margin-*
  void SetMarginTop(const Length& v) {
    if (MarginTop() != v) {
      if (!v.IsZero() || v.IsAuto()) {
        SetMayHaveMargin();
      }
      MutableMarginTopInternal() = v;
    }
  }
  void SetMarginRight(const Length& v) {
    if (MarginRight() != v) {
      if (!v.IsZero() || v.IsAuto()) {
        SetMayHaveMargin();
      }
      MutableMarginRightInternal() = v;
    }
  }
  void SetMarginBottom(const Length& v) {
    if (MarginBottom() != v) {
      if (!v.IsZero() || v.IsAuto()) {
        SetMayHaveMargin();
      }
      MutableMarginBottomInternal() = v;
    }
  }
  void SetMarginLeft(const Length& v) {
    if (MarginLeft() != v) {
      if (!v.IsZero() || v.IsAuto()) {
        SetMayHaveMargin();
      }
      MutableMarginLeftInternal() = v;
    }
  }

  // mask
  FillLayer& AccessMaskLayers() { return MutableMaskInternal(); }
  void AdjustMaskLayers() {
    if (MaskInternal().Next()) {
      AccessMaskLayers().CullEmptyLayers();
      AccessMaskLayers().FillUnsetProperties();
    }
  }

  // mask-box-image-*
  const NinePieceImage& MaskBoxImage() const { return MaskBoxImageInternal(); }
  void SetMaskBoxImage(const NinePieceImage& b) { SetMaskBoxImageInternal(b); }
  void SetMaskBoxImageOutset(const BorderImageLengthBox& outset) {
    MutableMaskBoxImageInternal().SetOutset(outset);
  }
  void SetMaskBoxImageSlices(const LengthBox& slices) {
    MutableMaskBoxImageInternal().SetImageSlices(slices);
  }
  void SetMaskBoxImageSlicesFill(bool fill) {
    MutableMaskBoxImageInternal().SetFill(fill);
  }
  void SetMaskBoxImageSource(StyleImage* v) {
    MutableMaskBoxImageInternal().SetImage(v);
  }
  void SetMaskBoxImageWidth(const BorderImageLengthBox& slices) {
    MutableMaskBoxImageInternal().SetBorderSlices(slices);
  }
  StyleImage* MaskBoxImageSource() const {
    return MaskBoxImageInternal().GetImage();
  }

  // opacity
  void SetOpacity(float f) {
    float v = ClampTo<float>(f, 0, 1);
    SetOpacityInternal(v);
  }

  // orphans
  void SetOrphans(int16_t o) { SetOrphansInternal(ClampTo<int16_t>(o, 1)); }

  // overflow
  bool ScrollsOverflow() const {
    return ComputedStyle::ScrollsOverflow(OverflowX()) ||
           ComputedStyle::ScrollsOverflow(OverflowY());
  }

  // padding-*
  void SetPaddingTop(const Length& v) {
    if (PaddingTop() != v) {
      if (!v.IsZero()) {
        SetMayHavePadding();
      }
      MutablePaddingTopInternal() = v;
    }
  }
  void SetPaddingRight(const Length& v) {
    if (PaddingRight() != v) {
      if (!v.IsZero()) {
        SetMayHavePadding();
      }
      MutablePaddingRightInternal() = v;
    }
  }
  void SetPaddingBottom(const Length& v) {
    if (PaddingBottom() != v) {
      if (!v.IsZero()) {
        SetMayHavePadding();
      }
      MutablePaddingBottomInternal() = v;
    }
  }
  void SetPaddingLeft(const Length& v) {
    if (PaddingLeft() != v) {
      if (!v.IsZero()) {
        SetMayHavePadding();
      }
      MutablePaddingLeftInternal() = v;
    }
  }

  // perspective-origin
  void SetPerspectiveOriginX(const Length& v) {
    SetPerspectiveOrigin(LengthPoint(v, PerspectiveOrigin().Y()));
  }
  void SetPerspectiveOriginY(const Length& v) {
    SetPerspectiveOrigin(LengthPoint(PerspectiveOrigin().X(), v));
  }

  // position
  EPosition GetPosition() const {
    return ComputedStyle::GetPosition(Display(), PositionInternal());
  }
  bool HasOutOfFlowPosition() const {
    return ComputedStyle::HasOutOfFlowPosition(GetPosition());
  }

  // shape-image-threshold
  void SetShapeImageThreshold(float shape_image_threshold) {
    float clamped_shape_image_threshold =
        ClampTo<float>(shape_image_threshold, 0, 1);
    SetShapeImageThresholdInternal(clamped_shape_image_threshold);
  }

  // shape-outside
  ShapeValue* ShapeOutside() const { return ShapeOutsideInternal().Get(); }

  // tab-size
  void SetTabSize(const TabSize& t) {
    if (t.GetPixelSize(1) < 0) {
      if (t.IsSpaces()) {
        SetTabSizeInternal(TabSize(0, TabSizeValueType::kSpace));
      } else {
        SetTabSizeInternal(TabSize(0, TabSizeValueType::kLength));
      }
    } else {
      SetTabSizeInternal(t);
    }
  }

  // transform-origin
  void SetTransformOriginX(const Length& v) {
    SetTransformOrigin(
        TransformOrigin(v, GetTransformOrigin().Y(), GetTransformOrigin().Z()));
  }
  void SetTransformOriginY(const Length& v) {
    SetTransformOrigin(
        TransformOrigin(GetTransformOrigin().X(), v, GetTransformOrigin().Z()));
  }
  void SetTransformOriginZ(float f) {
    SetTransformOrigin(
        TransformOrigin(GetTransformOrigin().X(), GetTransformOrigin().Y(), f));
  }

  // transitions
  const CSSTransitionData* Transitions() const {
    return TransitionsInternal().get();
  }
  CORE_EXPORT CSSTransitionData& AccessTransitions() {
    std::unique_ptr<CSSTransitionData>& transitions =
        MutableTransitionsInternal();
    if (!transitions) {
      transitions = std::make_unique<CSSTransitionData>();
    }
    return *transitions;
  }

  // -webkit-box-ordinal-group
  void SetBoxOrdinalGroup(unsigned ordinal_group) {
    SetBoxOrdinalGroupInternal(
        std::min(std::numeric_limits<unsigned>::max() - 1, ordinal_group));
  }

  // vertical-align
  void SetVerticalAlign(EVerticalAlign v) {
    SetVerticalAlignInternal(static_cast<unsigned>(v));
  }
  void SetVerticalAlignLength(const Length& length) {
    SetVerticalAlignInternal(static_cast<unsigned>(EVerticalAlign::kLength));
    SetVerticalAlignLengthInternal(length);
  }

  // widows
  void SetWidows(int16_t w) { SetWidowsInternal(ClampTo<int16_t>(w, 1)); }

  // word-spacing
  void SetWordSpacing(float word_spacing) {
    FontDescription description(GetFontDescription());
    description.SetWordSpacing(word_spacing);
    SetFontDescription(description);
  }

  // z-index
  void SetZIndex(int v) {
    SetHasAutoZIndexInternal(false);
    SetZIndexInternal(v);
  }
  void SetHasAutoZIndex() {
    SetHasAutoZIndexInternal(true);
    SetZIndexInternal(0);
  }

  // zoom
  CORE_EXPORT bool SetEffectiveZoom(float);

  // BaseData
  const ComputedStyle* GetBaseComputedStyle() const {
    if (auto* base_data = BaseData()) {
      return base_data->GetBaseComputedStyle();
    }
    return nullptr;
  }

  /// CallbackSelector
  void AddCallbackSelector(const String& selector) {
    if (!CallbackSelectors().Contains(selector)) {
      MutableCallbackSelectorsInternal().push_back(selector);
    }
  }

  // DocumentRulesSelectors
  void AddDocumentRulesSelector(StyleRule* selector) {
    if (!DocumentRulesSelectors()) {
      MutableDocumentRulesSelectorsInternal() =
          MakeGarbageCollected<HeapHashSet<WeakMember<StyleRule>>>();
    }
    DocumentRulesSelectors()->insert(selector);
  }

  // ::selection, etc
  StyleHighlightData& AccessHighlightData() {
    return MutableHighlightDataInternal();
  }

  // CustomHighlightNames
  void SetCustomHighlightNames(
      const HashSet<AtomicString>& custom_highlight_names) {
    SetCustomHighlightNamesInternal(
        std::make_unique<HashSet<AtomicString>>(custom_highlight_names));
  }

  // PaintImage
  void AddPaintImage(StyleImage* image) {
    if (!PaintImagesInternal()) {
      SetPaintImagesInternal(std::make_unique<PaintImages>());
    }
    MutablePaintImagesInternal()->push_back(image);
  }

  // TextAutosizingMultiplier
  CORE_EXPORT void SetTextAutosizingMultiplier(float);

  // ColorScheme and ForcedColors
  bool ShouldPreserveParentColor() const {
    return InForcedColorsMode() &&
           ForcedColorAdjust() == EForcedColorAdjust::kPreserveParentColor;
  }
  bool ShouldForceColor(const StyleColor& unforced_color) const {
    return ComputedStyle::ShouldForceColor(InForcedColorsMode(),
                                           ForcedColorAdjust(), unforced_color);
  }

  StyleColor InitialColorForColorScheme() const {
    // TODO(crbug.com/1046753, crbug.com/929098): The initial value of the color
    // property should be canvastext, but since we do not yet ship color-scheme
    // aware system colors, we use this method instead. This should be replaced
    // by default_value:"canvastext" in css_properties.json5.
    return StyleColor(DarkColorScheme() ? Color::kWhite : Color::kBlack);
  }

  // Helper method to adjust used values for color-scheme on the current
  // computed color-scheme passed in as flags. The computed value should
  // already have been set by the Apply* methods on the ColorScheme class, or
  // as the initial value.
  void SetUsedColorScheme(
      ColorSchemeFlags flags,
      mojom::blink::PreferredColorScheme preferred_color_scheme,
      bool force_dark);

  mojom::blink::ColorScheme UsedColorScheme() const {
    return ComputedStyle::UsedColorScheme(DarkColorScheme());
  }

  // Variables
  const StyleInheritedVariables* InheritedVariables() const {
    return InheritedVariablesInternal().get();
  }
  const StyleNonInheritedVariables* NonInheritedVariables() const {
    return NonInheritedVariablesInternal().get();
  }
  CSSVariableData* GetVariableData(const AtomicString&,
                                   bool is_inherited_property) const;
  CORE_EXPORT StyleInheritedVariables& MutableInheritedVariables();
  CORE_EXPORT StyleNonInheritedVariables& MutableNonInheritedVariables();
  void CopyInheritedVariablesFrom(const ComputedStyle*);
  void CopyNonInheritedVariablesFrom(const ComputedStyle*);
  CORE_EXPORT void SetVariableData(const AtomicString& name,
                                   scoped_refptr<CSSVariableData> value,
                                   bool is_inherited_property) {
    if (is_inherited_property) {
      MutableInheritedVariables().SetData(name, std::move(value));
    } else {
      MutableNonInheritedVariables().SetData(name, std::move(value));
    }
  }
  CORE_EXPORT void SetVariableValue(const AtomicString& name,
                                    const CSSValue* value,
                                    bool is_inherited_property) {
    if (is_inherited_property) {
      MutableInheritedVariables().SetValue(name, value);
    } else {
      MutableNonInheritedVariables().SetValue(name, value);
    }
  }
  CORE_EXPORT void SetInitialData(scoped_refptr<StyleInitialData> data) {
    MutableInitialDataInternal() = std::move(data);
  }

  EWhiteSpace WhiteSpace() const {
    return ToWhiteSpace(GetWhiteSpaceCollapse(), GetTextWrap());
  }
  void SetWhiteSpace(EWhiteSpace whitespace) {
    SetWhiteSpaceCollapse(ToWhiteSpaceCollapse(whitespace));
    SetTextWrap(ToTextWrap(whitespace));
  }

  // WritingMode
  WritingDirectionMode GetWritingDirection() const {
    return {GetWritingMode(), Direction()};
  }

  void SetHasStaticViewportUnits() {
    SetViewportUnitFlags(ViewportUnitFlags() |
                         static_cast<unsigned>(ViewportUnitFlag::kStatic));
  }
  void SetHasDynamicViewportUnits() {
    SetViewportUnitFlags(ViewportUnitFlags() |
                         static_cast<unsigned>(ViewportUnitFlag::kDynamic));
  }

  // ContainIntrinsicSize
  void SetContainIntrinsicSizeAuto() {
    StyleIntrinsicLength width = ContainIntrinsicWidth();
    width.SetHasAuto();
    SetContainIntrinsicWidth(width);

    StyleIntrinsicLength height = ContainIntrinsicHeight();
    height.SetHasAuto();
    SetContainIntrinsicHeight(height);
  }

};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COMPUTED_STYLE_H_
