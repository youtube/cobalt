// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_operator_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/mathml/mathml_operator_element.h"
#include "third_party/blink/renderer/platform/fonts/shaping/stretchy_operator_shaper.h"

namespace blink {

namespace {

inline LayoutUnit DisplayOperatorMinHeight(const ComputedStyle& style) {
  return LayoutUnit(
      MathConstant(
          style, OpenTypeMathSupport::MathConstants::kDisplayOperatorMinHeight)
          .value_or(0));
}

}  // namespace

NGMathOperatorLayoutAlgorithm::NGMathOperatorLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
  container_builder_.SetIsInlineFormattingContext(
      Node().IsInlineFormattingContextRoot());
}

const NGLayoutResult* NGMathOperatorLayoutAlgorithm::Layout() {
  // This algorithm can only be used for operators with a single text node,
  // which itself must contain only one glyph. We ensure that the subtree is
  // properly laid out but the glyph will actually be used to determine a
  // "large" or "stretched" version, from which we perform measurement and
  // painting.
  // See https://w3c.github.io/mathml-core/#layout-of-operators
  NGLayoutInputNode child = Node().FirstChild();
  DCHECK(child && child.IsInline());
  DCHECK(!child.NextSibling());
  DCHECK(!child.IsOutOfFlowPositioned());

  NGInlineChildLayoutContext context(To<NGInlineNode>(child),
                                     &container_builder_);
  const NGLayoutResult* child_layout_result = To<NGInlineNode>(child).Layout(
      ConstraintSpace(), /* break_token */ nullptr,
      /* column_spanner_path */ nullptr, &context);
  container_builder_.AddResult(*child_layout_result, {});

  // https://w3c.github.io/mathml-core/#layout-of-operators
  LayoutUnit operator_target_size;
  LayoutUnit target_stretch_ascent, target_stretch_descent;
  auto* element = DynamicTo<MathMLOperatorElement>(Node().GetDOMNode());
  if (element->HasBooleanProperty(MathMLOperatorElement::kStretchy)) {
    // "If the operator has the stretchy property:"
    if (!element->IsVertical()) {
      // "If the stretch axis of the operator is inline."
      if (ConstraintSpace().HasTargetStretchInlineSize())
        operator_target_size = ConstraintSpace().TargetStretchInlineSize();
    } else {
      // "Otherwise, the stretch axis of the operator is block."
      if (auto target_stretch_block_sizes =
              ConstraintSpace().TargetStretchBlockSizes()) {
        target_stretch_ascent = target_stretch_block_sizes->ascent;
        target_stretch_descent = target_stretch_block_sizes->descent;
        if (element->HasBooleanProperty(MathMLOperatorElement::kSymmetric)) {
          // "If the operator has the symmetric property then set the target
          // sizes Tascent and Tdescent to Sascent and Sdescent respectively:
          // Sascent = max( Uascent − AxisHeight, Udescent + AxisHeight ) +
          // AxisHeight
          // Sdescent = max( Uascent − AxisHeight, Udescent + AxisHeight ) −
          // AxisHeight"
          LayoutUnit axis = MathAxisHeight(Style());
          LayoutUnit half_target_stretch_size = std::max(
              target_stretch_ascent - axis, target_stretch_descent + axis);
          target_stretch_ascent = half_target_stretch_size + axis;
          target_stretch_descent = half_target_stretch_size - axis;
        }
        operator_target_size = target_stretch_ascent + target_stretch_descent;
        // "If minsize < 0 then set minsize to 0."
        LayoutUnit min_size =
            (Style().GetMathMinSize().GetType() == Length::kAuto
                 ? LayoutUnit(Style().FontSize())
                 : ValueForLength(Style().GetMathMinSize(),
                                  operator_target_size))
                .ClampNegativeToZero();
        // "If maxsize < minsize then set maxsize to minsize."
        LayoutUnit max_size = std::max<LayoutUnit>(
            (Style().GetMathMaxSize().GetType() == Length::kAuto
                 ? LayoutUnit(kIntMaxForLayoutUnit)
                 : ValueForLength(Style().GetMathMaxSize(),
                                  operator_target_size)),
            min_size);
        // "Then 0 ≤ minsize ≤ maxsize:"
        DCHECK(LayoutUnit() <= min_size && min_size <= max_size);
        if (operator_target_size <= LayoutUnit()) {
          // "If T ≤ 0 then set Tascent to minsize/2 and then set Tdescent to
          // minsize - Tascent."
          target_stretch_ascent = min_size / 2;
          target_stretch_descent = min_size - target_stretch_ascent;
        } else if (operator_target_size < min_size) {
          // "Otherwise, if 0 < T < minsize then first multiply Tascent by
          // minsize / T and then set Tdescent to minsize - Tascent."
          target_stretch_ascent =
              target_stretch_ascent.MulDiv(min_size, operator_target_size);
          target_stretch_descent = min_size - target_stretch_ascent;
        } else if (max_size < operator_target_size) {
          // "Otherwise, if maxsize < T then first multiply Tascent by maxsize
          // / T and then set Tdescent to maxsize − Tascent."
          target_stretch_ascent =
              target_stretch_descent.MulDiv(max_size, operator_target_size);
          target_stretch_descent = max_size - target_stretch_ascent;
        }
        operator_target_size = target_stretch_ascent + target_stretch_descent;
      }
    }
  } else {
    // "If the operator has the largeop property and if math-style on the <mo>
    // element is normal."
    DCHECK(element->HasBooleanProperty(MathMLOperatorElement::kLargeOp));
    DCHECK(HasDisplayStyle(Node().Style()));
    operator_target_size = DisplayOperatorMinHeight(Style());
  }

  StretchyOperatorShaper shaper(
      GetBaseCodePoint(),
      element->IsVertical() ? OpenTypeMathStretchData::StretchAxis::Vertical
                            : OpenTypeMathStretchData::StretchAxis::Horizontal);
  StretchyOperatorShaper::Metrics metrics;
  scoped_refptr<ShapeResult> shape_result =
      shaper.Shape(&Style().GetFont(), operator_target_size, &metrics);
  scoped_refptr<const ShapeResultView> shape_result_view =
      ShapeResultView::Create(shape_result.get());

  if (metrics.italic_correction) {
    container_builder_.SetMathItalicCorrection(
        LayoutUnit(metrics.italic_correction));
  }

  // TODO(http://crbug.com/1124301): The spec says the inline size should be
  // the one of the stretched glyph, but LayoutNG currently relies on the
  // min-max sizes. This means there can be excessive gap around vertical
  // stretchy operators and that unstretched size will be used for horizontal
  // stretchy operators. See also NGMathMLPainter::PaintOperator.
  LayoutUnit operator_ascent = LayoutUnit::FromFloatFloor(metrics.ascent);
  LayoutUnit operator_descent = LayoutUnit::FromFloatFloor(metrics.descent);

  container_builder_.SetMathMLPaintInfo(
      GetBaseCodePoint(), std::move(shape_result_view),
      LayoutUnit(metrics.advance), operator_ascent, operator_descent);

  LayoutUnit ascent = BorderScrollbarPadding().block_start + operator_ascent;
  LayoutUnit descent = operator_descent + BorderScrollbarPadding().block_end;
  if (element->HasBooleanProperty(MathMLOperatorElement::kStretchy) &&
      element->IsVertical()) {
    // "The stretchy glyph is shifted towards the line-under by a value Δ so
    // that its center aligns with the center of the target"
    LayoutUnit delta = ((operator_ascent - operator_descent) -
                        (target_stretch_ascent - target_stretch_descent)) /
                       2;
    ascent -= delta;
    descent += delta;
  }
  LayoutUnit intrinsic_block_size = ascent + descent;
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(), intrinsic_block_size,
      container_builder_.InitialBorderBoxSize().inline_size);
  container_builder_.SetBaselines(ascent);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);
  container_builder_.SetIsMathMLOperator();

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGMathOperatorLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  MinMaxSizes sizes;
  // https://w3c.github.io/mathml-core/#layout-of-operators
  auto* element = DynamicTo<MathMLOperatorElement>(Node().GetDOMNode());
  if (element->HasBooleanProperty(MathMLOperatorElement::kStretchy)) {
    // "If the operator has the stretchy property:"
    if (!element->IsVertical()) {
      // "If the stretch axis of the operator is inline."
      // The spec current says we should rely on the layout algorithm of
      // § 3.2.1.1 Layout of <mtext>. Instead, we perform horizontal stretching
      // with target size of 0 so that the size of the base glyph is used.
      StretchyOperatorShaper shaper(GetBaseCodePoint(),
                                    OpenTypeMathStretchData::Horizontal);
      StretchyOperatorShaper::Metrics metrics;
      shaper.Shape(&Style().GetFont(), 0, &metrics);
      sizes.Encompass(LayoutUnit(metrics.advance));
    } else {
      // "Otherwise, the stretch axis of the operator is block."
      sizes = GetMinMaxSizesForVerticalStretchyOperator(Style(),
                                                        GetBaseCodePoint());
    }
  } else {
    // "If the operator has the largeop property and if math-style on the <mo>
    // element is normal."
    StretchyOperatorShaper shaper(GetBaseCodePoint(),
                                  OpenTypeMathStretchData::Vertical);
    StretchyOperatorShaper::Metrics metrics;
    LayoutUnit operator_target_size = DisplayOperatorMinHeight(Style());
    shaper.Shape(&Style().GetFont(), operator_target_size, &metrics);
    sizes.Encompass(LayoutUnit(metrics.advance));
  }

  sizes += BorderScrollbarPadding().InlineSum();
  return MinMaxSizesResult(sizes, /* depends_on_block_constraints */ false);
}

UChar32 NGMathOperatorLayoutAlgorithm::GetBaseCodePoint() const {
  return DynamicTo<MathMLOperatorElement>(Node().GetDOMNode())
      ->GetTokenContent()
      .code_point;
}

}  // namespace blink
