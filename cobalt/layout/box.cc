/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/layout/box.h"

#include <limits>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/initial_style.h"
#include "cobalt/cssom/integer_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/property_names.h"
#include "cobalt/cssom/transform_function_list_value.h"
#include "cobalt/dom/serializer.h"
#include "cobalt/layout/container_box.h"
#include "cobalt/layout/transition_render_tree_animations.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/rect_node.h"

using cobalt::render_tree::CompositionNode;
using cobalt::render_tree::FilterNode;
using cobalt::render_tree::OpacityFilter;
using cobalt::render_tree::RectNode;
using cobalt::render_tree::ViewportFilter;
using cobalt::render_tree::animations::Animation;
using cobalt::render_tree::animations::NodeAnimationsMap;

namespace cobalt {
namespace layout {

Box::Box(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const UsedStyleProvider* used_style_provider)
    : computed_style_(computed_style),
      used_style_provider_(used_style_provider),
      transitions_(transitions),
      parent_(NULL),
      containing_block_(NULL),
      stacking_context_(NULL) {
  DCHECK(transitions_);
  DCHECK(used_style_provider_);

#ifdef _DEBUG
  margin_box_offset_from_containing_block_.SetVector(
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN());
  margin_insets_.SetInsets(std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::quiet_NaN());
  border_insets_.SetInsets(std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::quiet_NaN());
  padding_insets_.SetInsets(std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::quiet_NaN());
  content_size_.SetSize(std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::quiet_NaN());
#endif  // _DEBUG
}

Box::~Box() {}

bool Box::IsPositioned() const {
  return computed_style()->position() != cssom::KeywordValue::GetStatic();
}

bool Box::IsTransformed() const {
  return computed_style()->transform() != cssom::KeywordValue::GetNone();
}

bool Box::IsAbsolutelyPositioned() const {
  return computed_style()->position() == cssom::KeywordValue::GetAbsolute() ||
         computed_style()->position() == cssom::KeywordValue::GetFixed();
}

void Box::UpdateSize(const LayoutParams& layout_params) {
  if (ValidateUpdateSizeInputs(layout_params)) {
    return;
  }

  UpdateBorders();
  UpdatePaddings(layout_params);
  UpdateContentSizeAndMargins(layout_params);
}

bool Box::ValidateUpdateSizeInputs(const LayoutParams& params) {
  if (last_update_size_params_ && params == *last_update_size_params_) {
    return true;
  } else {
    last_update_size_params_ = params;
    return false;
  }
}

float Box::GetMarginBoxWidth() const {
  return margin_left() + GetBorderBoxWidth() + margin_right();
}

float Box::GetMarginBoxHeight() const {
  return margin_top() + GetBorderBoxHeight() + margin_bottom();
}

float Box::GetRightMarginEdgeOffsetFromContainingBlock() const {
  return left() + GetMarginBoxWidth();
}

float Box::GetBottomMarginEdgeOffsetFromContainingBlock() const {
  return top() + GetMarginBoxHeight();
}

float Box::GetBorderBoxWidth() const {
  return border_left_width() + GetPaddingBoxWidth() + border_right_width();
}

float Box::GetBorderBoxHeight() const {
  return border_top_width() + GetPaddingBoxHeight() + border_bottom_width();
}

math::SizeF Box::GetBorderBoxSize() const {
  return math::SizeF(GetBorderBoxWidth(), GetBorderBoxHeight());
}

float Box::GetPaddingBoxWidth() const {
  return padding_left() + width() + padding_right();
}

float Box::GetPaddingBoxHeight() const {
  return padding_top() + height() + padding_bottom();
}

math::SizeF Box::GetPaddingBoxSize() const {
  return math::SizeF(GetPaddingBoxWidth(), GetPaddingBoxHeight());
}

math::Vector2dF Box::GetContentBoxOffsetFromMarginBox() const {
  return math::Vector2dF(margin_left() + border_left_width() + padding_left(),
                         margin_top() + border_top_width() + padding_top());
}

void Box::RenderAndAnimate(
    CompositionNode::Builder* parent_content_node_builder,
    NodeAnimationsMap::Builder* node_animations_map_builder) const {
  float opacity = base::polymorphic_downcast<const cssom::NumberValue*>(
      computed_style_->opacity().get())->value();
  bool opacity_animated = transitions_->GetTransitionForProperty(
                              cssom::kOpacityPropertyName) != NULL;
  if (opacity <= 0.0f && !opacity_animated) {
    // If the box has 0 opacity, and opacity is not animated, then we do not
    // need to proceed any farther, the box is invisible.
    return;
  }

  render_tree::CompositionNode::Builder border_node_builder;

  // The painting order is:
  // - background color.
  // - background image.
  // - border.
  //   http://www.w3.org/TR/CSS21/zindex.html
  //
  // TODO(***REMOVED***): Fully implement the stacking algorithm:
  //               http://www.w3.org/TR/CSS21/visuren.html#z-index and
  //               http://www.w3.org/TR/CSS21/zindex.html.
  RenderAndAnimateBackgroundColor(&border_node_builder,
                                  node_animations_map_builder);
  RenderAndAnimateBackgroundImage(&border_node_builder,
                                  node_animations_map_builder);
  RenderAndAnimateBorder(&border_node_builder, node_animations_map_builder);

  RenderAndAnimateContent(&border_node_builder, node_animations_map_builder);

  scoped_refptr<render_tree::Node> border_node =
      new CompositionNode(border_node_builder.Pass());
  border_node = RenderAndAnimateOpacity(
      border_node, node_animations_map_builder, opacity, opacity_animated);
  math::Matrix3F border_node_transform =
      math::TranslateMatrix(left() + margin_left(), top() + margin_top());
  border_node = RenderAndAnimateTransform(
      border_node, node_animations_map_builder, &border_node_transform);

  parent_content_node_builder->AddChild(border_node, border_node_transform);
}

AnonymousBlockBox* Box::AsAnonymousBlockBox() { return NULL; }

#ifdef COBALT_BOX_DUMP_ENABLED

void Box::SetGeneratingNode(dom::Node* generating_node) {
  std::stringstream stream;
  dom::Serializer html_serializer(&stream);
  html_serializer.SerializeSelfOnly(generating_node);
  generating_html_ = stream.str();
}

void Box::DumpWithIndent(std::ostream* stream, int indent) const {
  if (!generating_html_.empty()) {
    DumpIndent(stream, indent);
    *stream << "# " << generating_html_ << "\n";
  }

  DumpIndent(stream, indent);
  DumpClassName(stream);
  DumpProperties(stream);
  *stream << "\n";

  static const int INDENT_SIZE = 2;
  DumpChildrenWithIndent(stream, indent + INDENT_SIZE);
}

#endif  // COBALT_BOX_DUMP_ENABLED

namespace {
void SetupRectNodeFromStyle(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& style,
    RectNode::Builder* rect_node_builder) {
  rect_node_builder->background_brush =
      scoped_ptr<render_tree::Brush>(new render_tree::SolidColorBrush(
          GetUsedColor(style->background_color())));
}
}  // namespace


bool Box::HasNonZeroMarginOrBorderOrPadding() const {
  return width() != GetMarginBoxWidth() || height() != GetMarginBoxHeight();
}

#ifdef COBALT_BOX_DUMP_ENABLED

void Box::DumpIndent(std::ostream* stream, int indent) const {
  while (indent--) {
    *stream << " ";
  }
}

void Box::DumpProperties(std::ostream* stream) const {
  *stream << "left=" << left() << " "
          << "top=" << top() << " "
          << "width=" << width() << " "
          << "height=" << height() << " ";

  *stream << "margin=" << margin_insets_.ToString() << " ";
  *stream << "border_width=" << border_insets_.ToString() << " ";
  *stream << "padding=" << padding_insets_.ToString() << " ";

  *stream << "baseline=" << GetBaselineOffsetFromTopMarginEdge() << " ";
}

void Box::DumpChildrenWithIndent(std::ostream* /*stream*/,
                                 int /*indent*/) const {}

#endif  // COBALT_BOX_DUMP_ENABLED

int Box::GetZIndex() const {
  if (computed_style()->z_index() == cssom::KeywordValue::GetAuto()) {
    return 0;
  } else {
    return base::polymorphic_downcast<cssom::IntegerValue*>(
               computed_style()->z_index().get())->value();
  }
}

void Box::UpdateCrossReferences(ContainerBox* fixed_containing_block) {
  // TODO(***REMOVED***): While passing NULL into the following parameters works fine
  //               for the initial containing block, if we wish to support
  //               partial layouts, we will need to search up our ancestor
  //               chain for the correct values to pass in here as context.
  UpdateCrossReferencesWithContext(fixed_containing_block, NULL, NULL);
}

void Box::UpdateCrossReferencesWithContext(
    ContainerBox* fixed_containing_block,
    ContainerBox* absolute_containing_block, ContainerBox* stacking_context) {
  if (IsPositioned() || IsTransformed()) {
    // Stacking context and containing blocks only matter for positioned
    // boxes.
    ContainerBox* containing_block;
    if (computed_style()->position() == cssom::KeywordValue::GetAbsolute()) {
      containing_block = absolute_containing_block;
    } else if (computed_style()->position() ==
               cssom::KeywordValue::GetFixed()) {
      containing_block = fixed_containing_block;
    } else {
      containing_block = parent_;
    }

    // Notify our containing block that we are a positioned child of theirs.
    if (containing_block && stacking_context) {
      SetupAsPositionedChild(containing_block, stacking_context);
    }
  }
}

void Box::UpdateBorders() {
  // TODO(***REMOVED***): Calculate used values of border widths.
  NOTIMPLEMENTED() << "Borders are not implemented yet.";

  border_insets_ = math::InsetsF();
}

void Box::UpdatePaddings(const LayoutParams& layout_params) {
  padding_insets_.SetInsets(
      GetUsedPaddingLeft(computed_style_, layout_params.containing_block_size),
      GetUsedPaddingTop(computed_style_, layout_params.containing_block_size),
      GetUsedPaddingRight(computed_style_, layout_params.containing_block_size),
      GetUsedPaddingBottom(computed_style_,
                           layout_params.containing_block_size));
}

void Box::SetupAsPositionedChild(ContainerBox* containing_block,
                                 ContainerBox* stacking_context) {
  DCHECK(IsPositioned() || IsTransformed());

  DCHECK_EQ(parent_, containing_block_);
  DCHECK(!stacking_context_);

  // Setup the link between this child box and its containing block.
  containing_block_ = containing_block;

  // Now setup this child box within its containing block/stacking context's
  // list of children.
  containing_block->AddContainingBlockChild(this);

  if (GetZIndex() != 0) {
    stacking_context_ = stacking_context;
  } else {
    stacking_context_ = containing_block;
  }
  stacking_context_->AddStackingContextChild(this);
}

namespace {

// Returns a matrix representing the transform on the object induced by its
// CSS transform style property.  If the object does not have a transform
// style property set, this will be the identity matrix.  Otherwise, it is
// calculated from the property value and returned.  The transform-origin
// style property will also be taken into account, and therefore the layed
// out size of the object is also required in order to resolve a
// percentage-based transform-origin.
math::Matrix3F GetCSSTransform(cssom::PropertyValue* transform_property_value,
                               const math::SizeF& used_size) {
  if (transform_property_value == cssom::KeywordValue::GetNone()) {
    return math::Matrix3F::Identity();
  }

  cssom::TransformFunctionListValue* transform =
      base::polymorphic_downcast<cssom::TransformFunctionListValue*>(
          transform_property_value);
  DCHECK(!transform->value().empty());

  scoped_refptr<cssom::TransformFunctionListValue> used_transform =
      GetUsedTransformListValue(transform, used_size);

  math::Matrix3F css_transform_matrix(used_transform->ToMatrix());

  // Apply the CSS transformations, taking into account the CSS
  // transform-origin property.
  // TODO(***REMOVED***): We are not actually taking advantage of the
  //               transform-origin property yet, instead we are just
  //               assuming that it is the default, 50% 50%.
  return math::TranslateMatrix(used_size.width() * 0.5f,
                               used_size.height() * 0.5f) *
         css_transform_matrix *
         math::TranslateMatrix(-used_size.width() * 0.5f,
                               -used_size.height() * 0.5f);
}

// Used within the animation callback for CSS transforms.  This will
// set the transform of a single-child composition node to that specified by
// the CSS transform of the provided CSS Style Declaration.
void SetupCompositionNodeFromCSSSStyleTransform(
    const math::SizeF& used_size,
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& style,
    CompositionNode::Builder* composition_node_builder) {
  DCHECK_EQ(1, composition_node_builder->composed_children().size());
  composition_node_builder->GetChild(0)->transform =
      GetCSSTransform(style->transform(), used_size);
}

void SetupFilterNodeFromStyle(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& style,
    FilterNode::Builder* filter_node_builder) {
  float opacity = base::polymorphic_downcast<const cssom::NumberValue*>(
                      style->opacity().get())->value();

  if (opacity < 1.0f) {
    filter_node_builder->opacity_filter.emplace(opacity);
  } else {
    // If opacity is 1, then no opacity filter should be applied, so the
    // source render tree should appear fully opaque.
    filter_node_builder->opacity_filter = base::nullopt;
  }
}

}  // namespace

void Box::RenderAndAnimateBorder(
    CompositionNode::Builder* /*border_node_builder*/,
    NodeAnimationsMap::Builder* /*node_animations_map_builder*/) const {
  // TODO(***REMOVED***): Render the border.
  NOTIMPLEMENTED() << "Borders are not implemented yet.";
}

void Box::RenderAndAnimateBackgroundColor(
    render_tree::CompositionNode::Builder* border_node_builder,
    NodeAnimationsMap::Builder* node_animations_map_builder) const {
  // Only create the RectNode if the background color is not the initial value
  // (which we know is transparent) and not transparent.  If it's animated,
  // add it no matter what since its value may change over time to be
  // non-transparent.
  bool background_color_transparent =
      GetUsedColor(computed_style_->background_color()).a() == 0.0f;
  bool background_color_animated =
      transitions_->GetTransitionForProperty(
          cssom::kBackgroundColorPropertyName) != NULL;
  if (!background_color_transparent || background_color_animated) {
    RectNode::Builder rect_node_builder(GetPaddingBoxSize(),
                                        scoped_ptr<render_tree::Brush>());
    SetupRectNodeFromStyle(computed_style_, &rect_node_builder);

    scoped_refptr<RectNode> rect_node(new RectNode(rect_node_builder.Pass()));
    border_node_builder->AddChild(
        rect_node,
        math::TranslateMatrix(border_left_width(), border_top_width()));

    if (background_color_animated) {
      AddTransitionAnimations<RectNode>(
          base::Bind(&SetupRectNodeFromStyle), *computed_style_, rect_node,
          *transitions_, node_animations_map_builder);
    }
  }
}

void Box::RenderAndAnimateBackgroundImage(
    CompositionNode::Builder* border_node_builder,
    NodeAnimationsMap::Builder* node_animations_map_builder) const {
  UNREFERENCED_PARAMETER(node_animations_map_builder);

  if (computed_style_->background_image() != cssom::KeywordValue::GetNone()) {
    cssom::PropertyListValue* property_list =
        base::polymorphic_downcast<cssom::PropertyListValue*>(
            computed_style_->background_image().get());
    // The farthest image is added to |composition_node_builder| first.
    for (cssom::PropertyListValue::Builder::const_reverse_iterator
             image_iterator = property_list->value().rbegin();
         image_iterator != property_list->value().rend(); ++image_iterator) {
      UsedBackgroundNodeProvider background_node_provider(
          used_style_provider_, GetPaddingBoxSize(),
          computed_style_->background_size(),
          computed_style_->background_position(),
          computed_style_->background_repeat());
      (*image_iterator)->Accept(&background_node_provider);
      scoped_refptr<render_tree::Node> background_node =
          background_node_provider.background_node();

      if (background_node) {
        border_node_builder->AddChild(
            background_node,
            math::TranslateMatrix(border_left_width(), border_top_width()));
      }
    }
  }
}

scoped_refptr<render_tree::Node> Box::RenderAndAnimateOpacity(
    const scoped_refptr<render_tree::Node>& border_node,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder,
    float opacity, bool opacity_animated) const {
  bool overflow_hidden =
      computed_style_->overflow().get() == cssom::KeywordValue::GetHidden();
  if (overflow_hidden || opacity < 1.0f || opacity_animated) {
    FilterNode::Builder filter_node_builder(border_node);

    if (overflow_hidden) {
      // TODO(***REMOVED***): The "overflow" property specifies whether a box is
      //               clipped to its padding edge.
      //                 http://www.w3.org/TR/CSS21/visufx.html#overflow
      //               Currently it's clipped to a border edge.
      filter_node_builder.viewport_filter =
          ViewportFilter(math::RectF(GetBorderBoxSize()));
    }

    if (opacity < 1.0f) {
      filter_node_builder.opacity_filter = OpacityFilter(opacity);
    }

    scoped_refptr<FilterNode> filter_node = new FilterNode(filter_node_builder);

    if (opacity_animated) {
      // Possibly setup an animation for transitioning opacity.
      AddTransitionAnimations<FilterNode>(
          base::Bind(&SetupFilterNodeFromStyle), *computed_style_, filter_node,
          *transitions_, node_animations_map_builder);
    }
    return filter_node;
  }

  return border_node;
}

scoped_refptr<render_tree::Node> Box::RenderAndAnimateTransform(
    const scoped_refptr<render_tree::Node>& border_node,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder,
    math::Matrix3F* border_node_transform) const {
  if (transitions_->GetTransitionForProperty(cssom::kTransformPropertyName)) {
    // If the CSS transform is animated, we cannot flatten it into the layout
    // transform, thus we create a new composition node to separate it and
    // animate that node only.
    render_tree::CompositionNode::Builder css_transform_node_builder;
    css_transform_node_builder.AddChild(border_node,
                                        math::Matrix3F::Identity());
    scoped_refptr<CompositionNode> css_transform_node =
        new CompositionNode(css_transform_node_builder.Pass());

    // Specifically animate only the composition node with the CSS transform.
    AddTransitionAnimations<CompositionNode>(
        base::Bind(&SetupCompositionNodeFromCSSSStyleTransform,
                   GetBorderBoxSize()),
        *computed_style(), css_transform_node, *transitions_,
        node_animations_map_builder);

    return css_transform_node;
  }

  if (computed_style_->transform() != cssom::KeywordValue::GetNone()) {
    // Combine layout transform and CSS transform.
    *border_node_transform =
        *border_node_transform *
        GetCSSTransform(computed_style_->transform(), GetBorderBoxSize());
  }
  return border_node;
}

// Based on http://www.w3.org/TR/CSS21/visudet.html#blockwidth.
void Box::UpdateHorizontalMarginsAssumingBlockLevelInFlowBox(
    float containing_block_width, float border_box_width,
    const base::optional<float>& possibly_overconstrained_margin_left,
    const base::optional<float>& possibly_overconstrained_margin_right) {
  base::optional<float> maybe_margin_left =
      possibly_overconstrained_margin_left;
  base::optional<float> maybe_margin_right =
      possibly_overconstrained_margin_right;

  // If "border-left-width" + "padding-left" + "width" + "padding-right" +
  // "border-right-width" (plus any of "margin-left" or "margin-right" that are
  // not "auto") is larger than the width of the containing block, then any
  // "auto" values for "margin-left" or "margin-right" are, for the following
  // rules, treated as zero.
  if (maybe_margin_left.value_or(0.0f) + border_box_width +
          maybe_margin_right.value_or(0.0f) >
      containing_block_width) {
    maybe_margin_left = maybe_margin_left.value_or(0.0f);
    maybe_margin_right = maybe_margin_right.value_or(0.0f);
  }

  if (maybe_margin_left) {
    // If all of the above have a computed value other than "auto", the values
    // are said to be "over-constrained" and the specified value of
    // "margin-right" is ignored and the value is calculated so as to make
    // the equality true.
    //
    // If there is exactly one value specified as "auto", its used value
    // follows from the equality.
    set_margin_left(*maybe_margin_left);
    set_margin_right(containing_block_width - *maybe_margin_left -
                     border_box_width);
  } else if (maybe_margin_right) {
    // If there is exactly one value specified as "auto", its used value
    // follows from the equality.
    set_margin_left(containing_block_width - border_box_width -
                    *maybe_margin_right);
    set_margin_right(*maybe_margin_right);
  } else {
    // If both "margin-left" and "margin-right" are "auto", their used values
    // are equal.
    float horizontal_margin = (containing_block_width - border_box_width) / 2;
    set_margin_left(horizontal_margin);
    set_margin_right(horizontal_margin);
  }
}


}  // namespace layout
}  // namespace cobalt
