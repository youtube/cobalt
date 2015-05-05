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
#include "cobalt/cssom/transform_function.h"
#include "cobalt/cssom/transform_list_value.h"
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
  DCHECK_NE(static_cast<cssom::TransitionSet*>(NULL), transitions_);
}

Box::~Box() {}

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

  parent_composition_node_builder->AddChild(
      new render_tree::CompositionNode(composition_node_builder.Pass()),
      GetTransform());
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

math::Matrix3F Box::GetTransform() const {
  math::Matrix3F transform_matrix =
      math::TranslateMatrix(used_frame().x(), used_frame().y());

  // Check if this block has any CSS transforms applied to it.  If so, build
  // a matrix representing all transformations applied in the correct order.
  DCHECK_NE(scoped_refptr<cssom::PropertyValue>(),
            computed_style()->transform());
  if (IsTransformable() &&
      computed_style()->transform() != cssom::KeywordValue::GetNone()) {
    const cssom::TransformListValue* transform =
        base::polymorphic_downcast<const cssom::TransformListValue*>(
            computed_style()->transform().get());
    DCHECK(!transform->value().empty());

    math::Matrix3F css_transform_matrix(math::Matrix3F::Identity());
    for (cssom::TransformListValue::TransformFunctions::const_iterator iter =
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
    transform_matrix = transform_matrix *
                       math::TranslateMatrix(used_frame().width() * 0.5f,
                                             used_frame().height() * 0.5f) *
                       css_transform_matrix *
                       math::TranslateMatrix(-used_frame().width() * 0.5f,
                                             -used_frame().height() * 0.5f);
  }

  return transform_matrix;
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
