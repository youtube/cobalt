// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/render_tree/dump_render_tree_to_string.h"

#include <sstream>

#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/matrix_transform_3d_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/node_visitor.h"
#include "cobalt/render_tree/punch_through_video_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/rect_shadow_node.h"
#include "cobalt/render_tree/text_node.h"

namespace cobalt {
namespace render_tree {

namespace animations {
class AnimateNode;
}  // namespace animations

namespace {

// A render tree visitor that accumulates node dumps to text within a
// std::ostringstream object.
class DebugTreePrinter : public NodeVisitor {
 public:
  DebugTreePrinter() : indent_(0) {}

  void Visit(animations::AnimateNode* /* animate */) override { NOTREACHED(); }
  void Visit(CompositionNode* composition) override;
  void Visit(FilterNode* text) override;
  void Visit(ImageNode* image) override;
  void Visit(MatrixTransform3DNode* transform) override;
  void Visit(MatrixTransformNode* transform) override;
  void Visit(PunchThroughVideoNode* punch_through) override;
  void Visit(RectNode* rect) override;
  void Visit(RectShadowNode* rect) override;
  void Visit(TextNode* text) override;

  // Returns the final result after visitation is complete.
  const std::string Result() const { return result_.str(); }

 private:
  // Adds an appropriate number of indent characters according to |indent_|.
  void AddIndentString();

  // Adds general render tree node information (e.g. bounds) for a given node.
  void AddNodeInfoString(Node* node);

  // A simple function that most of the time dumps all the generic information
  // about a given node.
  void AddNamedNodeString(Node* node, const char* type);

  // Helper function to increment and decrement |indent_|.
  class ScopedIncrement {
   public:
    explicit ScopedIncrement(int* value) : value_(value) { ++(*value_); }
    ~ScopedIncrement() { --(*value_); }

   private:
    int* value_;
  };

  // The results object in which we accumulate our string representation.
  std::ostringstream result_;

  // Our current indent when printing lines of information.
  int indent_;
};

void DebugTreePrinter::Visit(CompositionNode* composition) {
  AddNamedNodeString(composition, "CompositionNode");
  result_ << "\n";

  ScopedIncrement scoped_increment(&indent_);

  const render_tree::CompositionNode::Children& children =
      composition->data().children();
  for (render_tree::CompositionNode::Children::const_iterator iter =
           children.begin();
       iter != children.end(); ++iter) {
    (*iter)->Accept(this);
  }
}

void DebugTreePrinter::Visit(FilterNode* filter) {
  AddNamedNodeString(filter, "FilterNode");

  // Add some additional information to the FilterNode output to indicate which
  // filters were in effect.
  result_ << " { Filters: (";

  if (filter->data().opacity_filter) {
    result_ << "opacity, ";
  }
  if (filter->data().viewport_filter) {
    result_ << "viewport, ";
  }
  if (filter->data().blur_filter) {
    result_ << "blur_filter, ";
  }
  result_ << ") }\n";

  ScopedIncrement scoped_increment(&indent_);

  filter->data().source->Accept(this);
}

void DebugTreePrinter::Visit(ImageNode* image) {
  AddNamedNodeString(image, "ImageNode");
  result_ << "\n";
}

void DebugTreePrinter::Visit(MatrixTransform3DNode* transform) {
  AddNamedNodeString(transform, "MatrixTransform3DNode");
  result_ << "\n";

  ScopedIncrement scoped_increment(&indent_);

  transform->data().source->Accept(this);
}

void DebugTreePrinter::Visit(MatrixTransformNode* transform) {
  AddNamedNodeString(transform, "MatrixTransformNode");
  result_ << "\n";

  ScopedIncrement scoped_increment(&indent_);

  transform->data().source->Accept(this);
}

void DebugTreePrinter::Visit(PunchThroughVideoNode* punch_through) {
  AddNamedNodeString(punch_through, "PunchThroughVideoNode");
  result_ << "\n";
}

namespace {
class BrushPrinterVisitor : public render_tree::BrushVisitor {
 public:
  BrushPrinterVisitor() {}

  void Visit(
      const cobalt::render_tree::SolidColorBrush* solid_color_brush) override {
    UNREFERENCED_PARAMETER(solid_color_brush);
    brush_type_ = "(SolidColorBrush)";
  }
  void Visit(const cobalt::render_tree::LinearGradientBrush*
                 linear_gradient_brush) override {
    UNREFERENCED_PARAMETER(linear_gradient_brush);
    brush_type_ = "(LinearGradientBrush)";
  }
  void Visit(const cobalt::render_tree::RadialGradientBrush*
                 radial_gradient_brush) override {
    UNREFERENCED_PARAMETER(radial_gradient_brush);
    brush_type_ = "(RadialGradientBrush)";
  }

  const std::string& brush_type() const { return brush_type_; }

 private:
  std::string brush_type_;
};
}  // namespace

void DebugTreePrinter::Visit(RectNode* rect) {
  AddNamedNodeString(rect, "RectNode ");
  if (rect->data().background_brush) {
    BrushPrinterVisitor printer_brush_visitor;
    rect->data().background_brush->Accept(&printer_brush_visitor);
    result_ << printer_brush_visitor.brush_type();
  }
  result_ << "\n";
}

void DebugTreePrinter::Visit(RectShadowNode* rect) {
  AddNamedNodeString(rect, "RectShadowNode");
  result_ << "\n";
}

void DebugTreePrinter::Visit(TextNode* text) {
  AddNamedNodeString(text, "TextNode");
  result_ << "\n";
}

void DebugTreePrinter::AddIndentString() {
  result_ << std::string(static_cast<size_t>(indent_) * 2, ' ');
}

void DebugTreePrinter::AddNodeInfoString(Node* node) {
  const math::RectF bounds = node->GetBounds();

  result_ << "{ Bounds: (" << bounds.x() << ", " << bounds.y() << ", "
          << bounds.width() << ", " << bounds.height() << ") }";
}

void DebugTreePrinter::AddNamedNodeString(Node* node, const char* type_name) {
  AddIndentString();
  result_ << type_name << " ";
  AddNodeInfoString(node);
}

}  // namespace

std::string DumpRenderTreeToString(render_tree::Node* node) {
  DebugTreePrinter tree_printer;
  node->Accept(&tree_printer);

  return tree_printer.Result();
}

}  // namespace render_tree
}  // namespace cobalt
