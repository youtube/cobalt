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

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_names.h"
#include "cobalt/cssom/transform_function_list_value.h"
#include "cobalt/layout/transition_render_tree_animations.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/rect_node.h"

using cobalt::render_tree::CompositionNode;
using cobalt::render_tree::RectNode;
using cobalt::render_tree::animations::Animation;
using cobalt::render_tree::animations::NodeAnimationsMap;

namespace cobalt {
namespace layout {

Box::Box(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions)
    : computed_style_(computed_style), transitions_(transitions) {
  DCHECK(transitions_);
}

Box::~Box() {}

namespace {

// Returns a matrix representing the transform on the object, relative to its
// containing block, induced by its layout.  This will be a translation matrix
// that essentially just offsets to the object's layed out (x,y) position.
math::Matrix3F GetOffsetTransform(const math::PointF& offset) {
  return math::TranslateMatrix(offset.x(), offset.y());
}

// Returns a matrix representing the transform on the object induced by its
// CSS transform style property.  If the object does not have a transform
// style property set, this will be the identity matrix.  Otherwise, it is
// calculated from the property value and returned.  The transform-origin
// style property will also be taken into account, and therefore the layed
// out size of the object is also required in order to resolve a
// percentage-based transform-origin.
math::Matrix3F GetCSSTransform(
    const cssom::PropertyValue* transform_property_value,
    const math::SizeF& used_size) {
  if (transform_property_value == cssom::KeywordValue::GetNone()) {
    return math::Matrix3F::Identity();
  }

  const cssom::TransformFunctionListValue* transform =
      base::polymorphic_downcast<const cssom::TransformFunctionListValue*>(
          transform_property_value);
  DCHECK(!transform->value().empty());

  // Iterate through all transforms in the transform list appending them
  // to our css_transform_matrix.
  math::Matrix3F css_transform_matrix(math::Matrix3F::Identity());
  for (cssom::TransformFunctionListValue::Builder::const_iterator iter =
           transform->value().begin();
       iter != transform->value().end(); ++iter) {
    cssom::PostMultiplyMatrixByTransform(
        const_cast<cssom::TransformFunction*>(*iter), &css_transform_matrix);
  }

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

}  // namespace

void Box::AddToRenderTree(
    CompositionNode::Builder* parent_composition_node_builder,
    NodeAnimationsMap::Builder* node_animations_map_builder) const {
  render_tree::CompositionNode::Builder composition_node_builder;

  // TODO(***REMOVED***): Fully implement the stacking algorithm quoted below.

  // The background and borders of the element forming the stacking context
  // are rendered first.
  //   http://www.w3.org/TR/CSS21/visuren.html#z-index
  AddBackgroundToRenderTree(&composition_node_builder,
                            node_animations_map_builder);
  // TODO(***REMOVED***): Implement borders.
  AddContentToRenderTree(&composition_node_builder,
                         node_animations_map_builder);

  if (transitions_->GetTransitionForProperty(cssom::kTransformPropertyName)) {
    // If the CSS transform is animated, we cannot flatten it into the layout
    // transform, thus we create a new composition node to separate it and
    // animate that node only.
    scoped_refptr<CompositionNode> composition_node =
        new CompositionNode(composition_node_builder.Pass());

    render_tree::CompositionNode::Builder css_transform_node_builder;
    css_transform_node_builder.AddChild(
        composition_node, math::Matrix3F::Identity());
    scoped_refptr<CompositionNode> css_transform_node =
        new CompositionNode(css_transform_node_builder.Pass());

    // Specifically animate only the composition node with the CSS transform.
    AddTransitionAnimations<CompositionNode>(
      base::Bind(&SetupCompositionNodeFromCSSSStyleTransform,
                 used_frame().size()),
      *computed_style(), css_transform_node, *transitions_,
      node_animations_map_builder);

    // Now add that transform node to the parent composition node, along with
    // the application of the layout transform.
    parent_composition_node_builder->AddChild(
        css_transform_node, GetOffsetTransform(used_frame().origin()));
  } else {
    // Add all child render nodes to the parent composition node, with the
    // layout transform and CSS transform combined into one matrix.
    parent_composition_node_builder->AddChild(
        new CompositionNode(composition_node_builder.Pass()),
        GetOffsetTransform(used_frame().origin()) *
            GetCSSTransform(computed_style_->transform(), used_frame().size()));
  }
}

AnonymousBlockBox* Box::AsAnonymousBlockBox() { return NULL; }

void Box::DumpWithIndent(std::ostream* stream, int indent) const {
  DumpIndent(stream, indent);
  DumpClassName(stream);
  DumpProperties(stream);
  *stream << "\n";

  static const int INDENT_SIZE = 2;
  DumpChildrenWithIndent(stream, indent + INDENT_SIZE);
}

namespace {
void SetupRectNodeFromStyle(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& style,
    RectNode::Builder* rect_node_builder) {
  rect_node_builder->background_brush =
      scoped_ptr<render_tree::Brush>(new render_tree::SolidColorBrush(
          GetUsedColor(style->background_color())));
}
}  // namespace

void Box::AddBackgroundToRenderTree(
    CompositionNode::Builder* composition_node_builder,
    NodeAnimationsMap::Builder* node_animations_map_builder) const {
  RectNode::Builder rect_node_builder(used_frame().size(),
                                      scoped_ptr<render_tree::Brush>());
  SetupRectNodeFromStyle(computed_style_, &rect_node_builder);

  scoped_refptr<RectNode> rect_node(new RectNode(rect_node_builder.Pass()));
  composition_node_builder->AddChild(rect_node, math::Matrix3F::Identity());

  if (!transitions_->empty()) {
    AddTransitionAnimations<RectNode>(base::Bind(&SetupRectNodeFromStyle),
                                      *computed_style_, rect_node,
                                      *transitions_,
                                      node_animations_map_builder);
  }
}

void Box::DumpIndent(std::ostream* stream, int indent) const {
  while (indent--) {
    *stream << " ";
  }
}

void Box::DumpProperties(std::ostream* stream) const {
  *stream << "level=";
  switch (GetLevel()) {
    case kBlockLevel:
      *stream << "block ";
      break;
    case kInlineLevel:
      *stream << "inline ";
      break;
  }

  *stream << "left=" << used_frame_.x() << " "
          << "top=" << used_frame_.y() << " "
          << "width=" << used_frame_.width() << " "
          << "height=" << used_frame_.height() << " "
          << "height_above_baseline=" << GetHeightAboveBaseline() << " ";
}

void Box::DumpChildrenWithIndent(std::ostream* /*stream*/,
                                 int /*indent*/) const {}

}  // namespace layout
}  // namespace cobalt
