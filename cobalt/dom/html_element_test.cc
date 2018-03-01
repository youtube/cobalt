// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/html_element.h"

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/testing/mock_css_parser.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_rect_list.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_div_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/layout_boxes.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/dom/window.h"
#include "cobalt/media_session/media_session.h"
#include "cobalt/network_bridge/net_poster.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::_;

namespace cobalt {
namespace dom {

namespace {

// Useful for using base::Bind() along with GMock actions.
ACTION_P(InvokeCallback0, callback) {
  UNREFERENCED_PARAMETER(args);
  UNREFERENCED_PARAMETER(arg0);
  UNREFERENCED_PARAMETER(arg1);
  UNREFERENCED_PARAMETER(arg2);
  UNREFERENCED_PARAMETER(arg3);
  UNREFERENCED_PARAMETER(arg4);
  UNREFERENCED_PARAMETER(arg5);
  UNREFERENCED_PARAMETER(arg6);
  UNREFERENCED_PARAMETER(arg7);
  UNREFERENCED_PARAMETER(arg8);
  UNREFERENCED_PARAMETER(arg9);
  callback.Run();
}

const char kFooBarDeclarationString[] = "foo: bar;";
const char kDisplayInlineDeclarationString[] = "display: inline;";

class MockLayoutBoxes : public LayoutBoxes {
 public:
  MOCK_CONST_METHOD0(type, Type());
  MOCK_CONST_METHOD0(GetClientRects, scoped_refptr<DOMRectList>());

  MOCK_CONST_METHOD0(IsInline, bool());

  MOCK_CONST_METHOD0(GetBorderEdgeLeft, float());
  MOCK_CONST_METHOD0(GetBorderEdgeTop, float());
  MOCK_CONST_METHOD0(GetBorderEdgeWidth, float());
  MOCK_CONST_METHOD0(GetBorderEdgeHeight, float());

  MOCK_CONST_METHOD0(GetBorderLeftWidth, float());
  MOCK_CONST_METHOD0(GetBorderTopWidth, float());

  MOCK_CONST_METHOD0(GetMarginEdgeWidth, float());
  MOCK_CONST_METHOD0(GetMarginEdgeHeight, float());

  MOCK_CONST_METHOD0(GetPaddingEdgeLeft, float());
  MOCK_CONST_METHOD0(GetPaddingEdgeTop, float());
  MOCK_CONST_METHOD0(GetPaddingEdgeWidth, float());
  MOCK_CONST_METHOD0(GetPaddingEdgeHeight, float());

  MOCK_METHOD0(InvalidateSizes, void());
  MOCK_METHOD0(InvalidateCrossReferences, void());
  MOCK_METHOD0(InvalidateRenderTreeNodes, void());
};

// Takes the fist child of the given element repeatedly to the given depth.
scoped_refptr<HTMLElement> GetFirstChildAtDepth(
    const scoped_refptr<HTMLElement>& element, int depth) {
  scoped_refptr<Element> child = element;
  while (depth-- && child) {
    child = child->first_element_child();
  }
  if (!child) {
    return scoped_refptr<HTMLElement>();
  }
  DCHECK(child->AsHTMLElement());
  return child->AsHTMLElement();
}

}  // namespace

class HTMLElementTest : public ::testing::Test {
 protected:
  HTMLElementTest()
      : dom_stat_tracker_(new DomStatTracker("HTMLElementTest")),
        html_element_context_(NULL, &css_parser_, NULL, NULL, NULL, NULL, NULL,
                              NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                              dom_stat_tracker_.get(), "",
                              base::kApplicationStateStarted, NULL),
        document_(new Document(&html_element_context_)) {}
  ~HTMLElementTest() override {}

  // This creates simple DOM tree with mock layout boxes for all elements except
  // the last child element. It returns the root html element.
  scoped_refptr<HTMLElement> CreateHTMLElementTreeWithMockLayoutBoxes(
      const char* null_terminated_element_names[]);

  void SetElementStyle(const scoped_refptr<cssom::CSSDeclaredStyleData>& data,
                       HTMLElement* html_element);

  cssom::testing::MockCSSParser css_parser_;
  scoped_ptr<DomStatTracker> dom_stat_tracker_;
  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
};

void HTMLElementTest::SetElementStyle(
    const scoped_refptr<cssom::CSSDeclaredStyleData>& data,
    HTMLElement* html_element) {
  EXPECT_CALL(css_parser_,
              ParseStyleDeclarationList(kFooBarDeclarationString, _))
      .WillOnce(Return(data));
  html_element->SetAttribute("style", kFooBarDeclarationString);
}

scoped_refptr<HTMLElement>
HTMLElementTest::CreateHTMLElementTreeWithMockLayoutBoxes(
    const char* null_terminated_element_names[]) {
  DCHECK(!document_->IsXMLDocument());
  scoped_refptr<HTMLElement> root_html_element;
  scoped_refptr<HTMLElement> parent_html_element;
  do {
    scoped_refptr<HTMLElement> child_html_element(
        document_->CreateElement(*null_terminated_element_names)
            ->AsHTMLElement());
    DCHECK(child_html_element);
    child_html_element->css_computed_style_declaration()->SetData(
        make_scoped_refptr(new cssom::CSSComputedStyleData()));

    if (parent_html_element) {
      // Set layout boxes for all elements that have a child.
      scoped_ptr<MockLayoutBoxes> layout_boxes(new MockLayoutBoxes);
      parent_html_element->set_layout_boxes(layout_boxes.PassAs<LayoutBoxes>());

      parent_html_element->AppendChild(child_html_element);
    }

    parent_html_element = child_html_element;
    if (!root_html_element) {
      root_html_element = parent_html_element;
    }
  } while (*(++null_terminated_element_names));

  // Set layout boxes for all elements that have a child.
  scoped_refptr<Element> parent_element = root_html_element;
  while (parent_element) {
    scoped_refptr<Element> child_element =
        parent_element->first_element_child();
    if (child_element) {
      scoped_ptr<MockLayoutBoxes> layout_boxes(new MockLayoutBoxes);
      parent_element->AsHTMLElement()->set_layout_boxes(
          layout_boxes.PassAs<LayoutBoxes>());
    }
    parent_element = child_element;
  }

  DCHECK(root_html_element);
  return root_html_element;
}

TEST_F(HTMLElementTest, Dir) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();
  EXPECT_EQ("", html_element->dir());

  html_element->set_dir("invalid");
  EXPECT_EQ("", html_element->dir());

  html_element->set_dir("ltr");
  EXPECT_EQ("ltr", html_element->dir());

  html_element->set_dir("rtl");
  EXPECT_EQ("rtl", html_element->dir());

  // Value "auto" is not supported.
  html_element->set_dir("auto");
  EXPECT_EQ("", html_element->dir());
}

TEST_F(HTMLElementTest, TabIndex) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();
  EXPECT_EQ(0, html_element->tab_index());

  html_element->set_tab_index(-1);
  EXPECT_EQ(-1, html_element->tab_index());
}

TEST_F(HTMLElementTest, Focus) {
  // Give the document browsing context which is needed for focus to work.
  testing::StubWindow window;
  document_->set_window(window.window());
  // Give the document initial computed style.
  document_->SetViewport(math::Size(320, 240));

  scoped_refptr<HTMLElement> html_element_1 =
      document_->CreateElement("div")->AsHTMLElement();
  scoped_refptr<HTMLElement> html_element_2 =
      document_->CreateElement("div")->AsHTMLElement();
  document_->AppendChild(html_element_1);
  document_->AppendChild(html_element_2);
  EXPECT_FALSE(document_->active_element());

  html_element_1->set_tab_index(-1);
  html_element_1->Focus();
  ASSERT_TRUE(document_->active_element());
  EXPECT_EQ(html_element_1, document_->active_element()->AsHTMLElement());

  html_element_2->set_tab_index(-1);
  html_element_2->Focus();
  ASSERT_TRUE(document_->active_element());
  EXPECT_EQ(html_element_2, document_->active_element()->AsHTMLElement());

  // Make sure that if we try to focus an element that has display set to none,
  // it will not take the focus.
  scoped_refptr<cssom::CSSDeclaredStyleData> display_none_style(
      new cssom::CSSDeclaredStyleData());
  display_none_style->SetPropertyValueAndImportance(
      cssom::kDisplayProperty, cssom::KeywordValue::GetNone(), false);
  SetElementStyle(display_none_style, html_element_1);
  html_element_1->Focus();
  ASSERT_TRUE(document_->active_element());
  EXPECT_EQ(html_element_2, document_->active_element()->AsHTMLElement());

  // Make sure that if we try to focus an element whose ancestor has display
  // set to none, it will not take the focus.
  scoped_refptr<HTMLElement> html_element_3 =
      document_->CreateElement("div")->AsHTMLElement();
  scoped_refptr<HTMLElement> html_element_4 =
      document_->CreateElement("div")->AsHTMLElement();
  document_->AppendChild(html_element_3);
  html_element_3->AppendChild(html_element_4);

  html_element_4->set_tab_index(-1);
  SetElementStyle(display_none_style, html_element_3);
  html_element_4->Focus();
  ASSERT_TRUE(document_->active_element());
  EXPECT_EQ(html_element_2, document_->active_element()->AsHTMLElement());
}

TEST_F(HTMLElementTest, Blur) {
  // Give the document browsing context which is needed for focus to work.
  testing::StubWindow window;
  document_->set_window(window.window());
  // Give the document initial computed style.
  document_->SetViewport(math::Size(320, 240));

  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();
  document_->AppendChild(html_element);
  EXPECT_FALSE(document_->active_element());

  html_element->set_tab_index(-1);
  html_element->Focus();
  ASSERT_TRUE(document_->active_element());
  EXPECT_EQ(html_element, document_->active_element()->AsHTMLElement());

  html_element->Blur();
  EXPECT_FALSE(document_->active_element());
}

TEST_F(HTMLElementTest, RemoveActiveElementShouldRunBlur) {
  // Give the document browsing context which is needed for focus to work.
  testing::StubWindow window;
  document_->set_window(window.window());
  // Give the document initial computed style.
  document_->SetViewport(math::Size(320, 240));

  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();
  document_->AppendChild(html_element);
  EXPECT_FALSE(document_->active_element());

  html_element->set_tab_index(-1);
  html_element->Focus();
  ASSERT_TRUE(document_->active_element());
  EXPECT_EQ(html_element, document_->active_element()->AsHTMLElement());

  document_->RemoveChild(html_element);
  EXPECT_FALSE(document_->active_element());
}

TEST_F(HTMLElementTest, LayoutBoxesGetter) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();

  scoped_ptr<MockLayoutBoxes> mock_layout_boxes(new MockLayoutBoxes);
  MockLayoutBoxes* saved_mock_layout_boxes_ptr = mock_layout_boxes.get();
  html_element->set_layout_boxes(mock_layout_boxes.PassAs<LayoutBoxes>());
  DCHECK(mock_layout_boxes.get() == NULL);

  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  html_element->layout_boxes()),
              type())
      .WillOnce(Return(LayoutBoxes::kLayoutLayoutBoxes));
  LayoutBoxes* layout_boxes = html_element->layout_boxes();
  EXPECT_EQ(layout_boxes, saved_mock_layout_boxes_ptr);
  EXPECT_EQ(layout_boxes->type(), LayoutBoxes::kLayoutLayoutBoxes);
}

TEST_F(HTMLElementTest, GetBoundingClientRectWithoutLayoutBox) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();
  scoped_refptr<DOMRect> rect = html_element->GetBoundingClientRect();
  DCHECK(rect);
  EXPECT_FLOAT_EQ(rect->x(), 0.0f);
  EXPECT_FLOAT_EQ(rect->y(), 0.0f);
  EXPECT_FLOAT_EQ(rect->width(), 0.0f);
  EXPECT_FLOAT_EQ(rect->height(), 0.0f);
  EXPECT_FLOAT_EQ(rect->top(), 0.0f);
  EXPECT_FLOAT_EQ(rect->right(), 0.0f);
  EXPECT_FLOAT_EQ(rect->bottom(), 0.0f);
  EXPECT_FLOAT_EQ(rect->left(), 0.0f);
}

// Algorithm for client_top:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-clienttop
TEST_F(HTMLElementTest, ClientTop) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();

  // 1. If the element has no associated CSS layout box, return zero.
  EXPECT_FLOAT_EQ(html_element->client_top(), 0.0f);

  scoped_ptr<MockLayoutBoxes> layout_boxes(new MockLayoutBoxes);
  html_element->set_layout_boxes(layout_boxes.PassAs<LayoutBoxes>());

  // 1. If the CSS layout box is inline, return zero.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  html_element->layout_boxes()),
              IsInline())
      .WillOnce(Return(true));
  EXPECT_FLOAT_EQ(html_element->client_top(), 0.0f);

  // 2. Return the computed value of the 'border-top-width' property plus the
  // height of any scrollbar rendered between the top padding edge and the top
  // border edge, ignoring any transforms that apply to the element and its
  // ancestors.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  html_element->layout_boxes()),
              IsInline())
      .WillOnce(Return(false));
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  html_element->layout_boxes()),
              GetBorderTopWidth())
      .WillOnce(Return(10.0f));
  EXPECT_FLOAT_EQ(html_element->client_top(), 10.0f);
}

// Algorithm for client_left:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-clientleft
TEST_F(HTMLElementTest, ClientLeft) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();

  // 1. If the element has no associated CSS layout box, return zero.
  EXPECT_FLOAT_EQ(html_element->client_left(), 0.0f);

  scoped_ptr<MockLayoutBoxes> layout_boxes(new MockLayoutBoxes);
  html_element->set_layout_boxes(layout_boxes.PassAs<LayoutBoxes>());

  // 1. If the CSS layout box is inline, return zero.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  html_element->layout_boxes()),
              IsInline())
      .WillOnce(Return(true));
  EXPECT_FLOAT_EQ(html_element->client_left(), 0.0f);

  // 2. Return the computed value of the 'border-left-width' property plus the
  // width of any scrollbar rendered between the left padding edge and the left
  // border edge, ignoring any transforms that apply to the element and its
  // ancestors.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  html_element->layout_boxes()),
              IsInline())
      .WillOnce(Return(false));
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  html_element->layout_boxes()),
              GetBorderLeftWidth())
      .WillOnce(Return(10.0f));
  EXPECT_FLOAT_EQ(html_element->client_left(), 10.0f);
}

// Algorithm for client_width:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-clientwidth
TEST_F(HTMLElementTest, ClientWidth) {
  const char* element_names[] = {"html", "body", "div", "div", NULL};
  scoped_refptr<HTMLElement> root_html_element =
      CreateHTMLElementTreeWithMockLayoutBoxes(element_names);

  DCHECK_EQ(GetFirstChildAtDepth(root_html_element, 0), root_html_element);
  DCHECK_EQ(GetFirstChildAtDepth(root_html_element, 1),
            root_html_element->first_element_child()->AsHTMLElement());

  // 1. If the element has no associated CSS layout box, return zero.
  EXPECT_FLOAT_EQ(GetFirstChildAtDepth(root_html_element, 3)->client_width(),
                  0.0f);

  // 1. If the CSS layout box is inline, return zero.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  GetFirstChildAtDepth(root_html_element, 1)->layout_boxes()),
              IsInline())
      .WillOnce(Return(true));
  EXPECT_FLOAT_EQ(GetFirstChildAtDepth(root_html_element, 1)->client_width(),
                  0.0f);

  // 2. If the element is the root element, return the viewport width.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  root_html_element->layout_boxes()),
              IsInline())
      .WillOnce(Return(false));
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  root_html_element->layout_boxes()),
              GetMarginEdgeWidth())
      .WillOnce(Return(1920.0f));
  EXPECT_FLOAT_EQ(root_html_element->client_width(), 1920.0f);

  // 3. Return the width of the padding edge, ignoring any transforms that apply
  // to the element and its ancestors.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  GetFirstChildAtDepth(root_html_element, 2)->layout_boxes()),
              IsInline())
      .WillOnce(Return(false));
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  GetFirstChildAtDepth(root_html_element, 2)->layout_boxes()),
              GetPaddingEdgeWidth())
      .WillOnce(Return(10.0f));
  EXPECT_FLOAT_EQ(GetFirstChildAtDepth(root_html_element, 2)->client_width(),
                  10.0f);
}

// Algorithm for client_height:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-clientheight
TEST_F(HTMLElementTest, ClientHeight) {
  const char* element_names[] = {"html", "body", "div", "div", NULL};
  scoped_refptr<HTMLElement> root_html_element =
      CreateHTMLElementTreeWithMockLayoutBoxes(element_names);

  DCHECK_EQ(GetFirstChildAtDepth(root_html_element, 0), root_html_element);
  DCHECK_EQ(GetFirstChildAtDepth(root_html_element, 1),
            root_html_element->first_element_child()->AsHTMLElement());

  // 1. If the element has no associated CSS layout box, return zero.
  EXPECT_FLOAT_EQ(GetFirstChildAtDepth(root_html_element, 3)->client_height(),
                  0.0f);

  // 1. If the CSS layout box is inline, return zero.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  GetFirstChildAtDepth(root_html_element, 1)->layout_boxes()),
              IsInline())
      .WillOnce(Return(true));
  EXPECT_FLOAT_EQ(GetFirstChildAtDepth(root_html_element, 1)->client_height(),
                  0.0f);

  // 2. If the element is the root element, return the viewport height.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  root_html_element->layout_boxes()),
              IsInline())
      .WillOnce(Return(false));
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  root_html_element->layout_boxes()),
              GetMarginEdgeHeight())
      .WillOnce(Return(1080.0f));
  EXPECT_FLOAT_EQ(root_html_element->client_height(), 1080.0f);

  // Return the height of the padding edge, ignoring any transforms that apply
  // to the element and its ancestors.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  GetFirstChildAtDepth(root_html_element, 2)->layout_boxes()),
              IsInline())
      .WillOnce(Return(false));
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  GetFirstChildAtDepth(root_html_element, 2)->layout_boxes()),
              GetPaddingEdgeHeight())
      .WillOnce(Return(10.0f));
  EXPECT_FLOAT_EQ(GetFirstChildAtDepth(root_html_element, 2)->client_height(),
                  10.0f);
}

TEST_F(HTMLElementTest, OffsetParent) {
  const char* element_names[] = {"html", "body", "div", "div",
                                 "div",  "div",  "div", NULL};
  scoped_refptr<HTMLElement> root_html_element =
      CreateHTMLElementTreeWithMockLayoutBoxes(element_names);

  DCHECK_EQ(GetFirstChildAtDepth(root_html_element, 0), root_html_element);
  DCHECK_EQ(GetFirstChildAtDepth(root_html_element, 1),
            root_html_element->first_element_child()->AsHTMLElement());

  // Algorithm for offsetParent:
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-htmlelement-offsetparent

  // Return null if the element is the root element.
  EXPECT_FALSE(root_html_element->offset_parent());

  // Return null if the element is the HTML body element.
  DCHECK(GetFirstChildAtDepth(root_html_element, 1)->AsHTMLBodyElement());
  EXPECT_FALSE(GetFirstChildAtDepth(root_html_element, 1)->offset_parent());

  scoped_refptr<cssom::CSSComputedStyleData> computed_style_relative =
      make_scoped_refptr(new cssom::CSSComputedStyleData());
  computed_style_relative->set_position(cssom::KeywordValue::GetRelative());
  GetFirstChildAtDepth(root_html_element, 2)
      ->css_computed_style_declaration()
      ->SetData(computed_style_relative);

  // Return ancestor if it is the HTML body element.
  EXPECT_EQ(GetFirstChildAtDepth(root_html_element, 2)->offset_parent(),
            GetFirstChildAtDepth(root_html_element, 1));

  // Return null if the element's computed value of the 'position' property is
  // 'fixed'.
  scoped_refptr<cssom::CSSComputedStyleData> computed_style_fixed =
      make_scoped_refptr(new cssom::CSSComputedStyleData());
  computed_style_fixed->set_position(cssom::KeywordValue::GetFixed());
  GetFirstChildAtDepth(root_html_element, 3)
      ->css_computed_style_declaration()
      ->SetData(computed_style_fixed);
  EXPECT_FALSE(GetFirstChildAtDepth(root_html_element, 3)->offset_parent());

  // Return ancestor if its computed value of the 'position' property is not
  // 'static'.
  EXPECT_EQ(GetFirstChildAtDepth(root_html_element, 4)->offset_parent(),
            GetFirstChildAtDepth(root_html_element, 3));

  // Return ancestor if its computed value of the 'position' property is not
  // 'static'.
  EXPECT_EQ(GetFirstChildAtDepth(root_html_element, 5)->offset_parent(),
            GetFirstChildAtDepth(root_html_element, 3));

  // Return null if the element does not have an associated CSS layout box.
  EXPECT_FALSE(GetFirstChildAtDepth(root_html_element, 6)->offset_parent());
}

// Algorithm for offset_top:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-htmlelement-offsettop
TEST_F(HTMLElementTest, OffsetTop) {
  const char* element_names[] = {"html", "body", "div", "div", NULL};
  scoped_refptr<HTMLElement> root_html_element =
      CreateHTMLElementTreeWithMockLayoutBoxes(element_names);

  DCHECK_EQ(GetFirstChildAtDepth(root_html_element, 0), root_html_element);
  DCHECK_EQ(GetFirstChildAtDepth(root_html_element, 1),
            root_html_element->first_element_child()->AsHTMLElement());

  // 1. If the element is the HTML body element or does not have any associated
  // CSS layout box return zero and terminate this algorithm.
  DCHECK(GetFirstChildAtDepth(root_html_element, 1)->AsHTMLBodyElement());
  EXPECT_FLOAT_EQ(GetFirstChildAtDepth(root_html_element, 1)->offset_top(),
                  0.0f);
  EXPECT_FLOAT_EQ(GetFirstChildAtDepth(root_html_element, 3)->offset_top(),
                  0.0f);

  // 2. If the offsetParent of the element is null return the y-coordinate of
  // the top border edge of the first CSS layout box associated with the
  // element, relative to the initial containing block origin, ignoring any
  // transforms that apply to the element and its ancestors, and terminate this
  // algorithm.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  GetFirstChildAtDepth(root_html_element, 0)->layout_boxes()),
              GetBorderEdgeTop())
      .WillOnce(Return(10.0f));
  EXPECT_FLOAT_EQ(GetFirstChildAtDepth(root_html_element, 0)->offset_top(),
                  10.0f);

  // 3. Return the result of subtracting the y-coordinate of the top padding
  // edge of the first CSS layout box associated with the offsetParent of the
  // element from the y-coordinate of the top border edge of the first CSS
  // layout box associated with the element, relative to the initial containing
  // block origin, ignoring any transforms that apply to the element and its
  // ancestors.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  GetFirstChildAtDepth(root_html_element, 1)->layout_boxes()),
              GetPaddingEdgeTop())
      .WillOnce(Return(20.0f));
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  GetFirstChildAtDepth(root_html_element, 2)->layout_boxes()),
              GetBorderEdgeTop())
      .WillOnce(Return(100.0f));
  EXPECT_FLOAT_EQ(GetFirstChildAtDepth(root_html_element, 2)->offset_top(),
                  80.0f);
}

// Algorithm for offset_left:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-htmlelement-offsetleft
TEST_F(HTMLElementTest, OffsetLeft) {
  const char* element_names[] = {"html", "body", "div", "div", NULL};
  scoped_refptr<HTMLElement> root_html_element =
      CreateHTMLElementTreeWithMockLayoutBoxes(element_names);

  DCHECK_EQ(GetFirstChildAtDepth(root_html_element, 0), root_html_element);
  DCHECK_EQ(GetFirstChildAtDepth(root_html_element, 1),
            root_html_element->first_element_child()->AsHTMLElement());

  // 1. If the element is the HTML body element or does not have any associated
  // CSS layout box return zero and terminate this algorithm.
  DCHECK(GetFirstChildAtDepth(root_html_element, 1)->AsHTMLBodyElement());
  EXPECT_FLOAT_EQ(GetFirstChildAtDepth(root_html_element, 1)->offset_left(),
                  0.0f);
  EXPECT_FLOAT_EQ(GetFirstChildAtDepth(root_html_element, 3)->offset_left(),
                  0.0f);

  // 2. If the offsetParent of the element is null return the x-coordinate of
  // the left border edge of the first CSS layout box associated with the
  // element, relative to the initial containing block origin, ignoring any
  // transforms that apply to the element and its ancestors, and terminate this
  // algorithm.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  GetFirstChildAtDepth(root_html_element, 0)->layout_boxes()),
              GetBorderEdgeLeft())
      .WillOnce(Return(10.0f));
  EXPECT_FLOAT_EQ(GetFirstChildAtDepth(root_html_element, 0)->offset_left(),
                  10.0f);

  // 3. Return the result of subtracting the x-coordinate of the left padding
  // edge of the first CSS layout box associated with the offsetParent of the
  // element from the x-coordinate of the left border edge of the first CSS
  // layout box associated with the element, relative to the initial containing
  // block origin, ignoring any transforms that apply to the element and its
  // ancestors.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  GetFirstChildAtDepth(root_html_element, 1)->layout_boxes()),
              GetPaddingEdgeLeft())
      .WillOnce(Return(20.0f));
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  GetFirstChildAtDepth(root_html_element, 2)->layout_boxes()),
              GetBorderEdgeLeft())
      .WillOnce(Return(100.0f));
  EXPECT_FLOAT_EQ(GetFirstChildAtDepth(root_html_element, 2)->offset_left(),
                  80.0f);
}

// Algorithm for offset_width:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-htmlelement-offsetwidth
TEST_F(HTMLElementTest, OffsetWidth) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();

  // 1. If the element does not have any associated CSS layout box return zero
  // and terminate this algorithm.
  EXPECT_FLOAT_EQ(html_element->offset_width(), 0.0f);

  scoped_ptr<MockLayoutBoxes> layout_boxes(new MockLayoutBoxes);
  html_element->set_layout_boxes(layout_boxes.PassAs<LayoutBoxes>());

  // 2. Return the border edge width of the first CSS layout box associated with
  // the element, ignoring any transforms that apply to the element and its
  // ancestors.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  html_element->layout_boxes()),
              GetBorderEdgeWidth())
      .WillOnce(Return(10.0f));
  EXPECT_FLOAT_EQ(html_element->offset_width(), 10.0f);
}

// Algorithm for offset_height:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-htmlelement-offsetheight
TEST_F(HTMLElementTest, OffsetHeight) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();

  // 1. If the element does not have any associated CSS layout box return zero
  // and terminate this algorithm.
  EXPECT_FLOAT_EQ(html_element->offset_height(), 0.0f);

  scoped_ptr<MockLayoutBoxes> layout_boxes(new MockLayoutBoxes);
  html_element->set_layout_boxes(layout_boxes.PassAs<LayoutBoxes>());

  // 2. Return the border edge height of the first CSS layout box associated
  // with the element, ignoring any transforms that apply to the element and its
  // ancestors.
  EXPECT_CALL(*base::polymorphic_downcast<MockLayoutBoxes*>(
                  html_element->layout_boxes()),
              GetBorderEdgeHeight())
      .WillOnce(Return(10.0f));
  EXPECT_FLOAT_EQ(html_element->offset_height(), 10.0f);
}

TEST_F(HTMLElementTest, SetAttributeMatchesGetAttribute) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();
  html_element->SetAttribute("foo", "bar");
  EXPECT_EQ(1, html_element->attributes()->length());
  EXPECT_EQ("bar", html_element->GetAttribute("foo").value());
}

TEST_F(HTMLElementTest, SetAttributeStyleSetsElementStyle) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();
  scoped_refptr<cssom::CSSDeclaredStyleData> style(
      new cssom::CSSDeclaredStyleData());
  EXPECT_CALL(css_parser_, ParseStyleDeclarationList("", _))
      .WillOnce(Return(style));
  html_element->SetAttribute("style", "");
  EXPECT_EQ(style, html_element->style()->data());
}

TEST_F(HTMLElementTest, SetAttributeStyleReplacesExistingElementStyle) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();
  scoped_refptr<cssom::CSSDeclaredStyleData> style(
      new cssom::CSSDeclaredStyleData());
  EXPECT_CALL(css_parser_,
              ParseStyleDeclarationList(kDisplayInlineDeclarationString, _))
      .WillOnce(Return(style));
  html_element->SetAttribute("style", kDisplayInlineDeclarationString);
  style->SetPropertyValueAndImportance(cssom::kDisplayProperty,
                                       cssom::KeywordValue::GetInline(), false);
  EXPECT_EQ(style, html_element->style()->data());
  EXPECT_EQ(kDisplayInlineDeclarationString,
            html_element->GetAttribute("style").value());

  scoped_refptr<cssom::CSSDeclaredStyleData> new_style(
      new cssom::CSSDeclaredStyleData());
  EXPECT_CALL(css_parser_,
              ParseStyleDeclarationList(kFooBarDeclarationString, _))
      .WillOnce(Return(new_style));
  html_element->SetAttribute("style", kFooBarDeclarationString);
  EXPECT_EQ(1, html_element->attributes()->length());
  EXPECT_EQ(new_style, html_element->style()->data());
  EXPECT_EQ(kFooBarDeclarationString,
            html_element->GetAttribute("style").value());
}

TEST_F(HTMLElementTest, GetAttributeStyleMatchesSetAttributeStyle) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();
  scoped_refptr<cssom::CSSDeclaredStyleData> style(
      new cssom::CSSDeclaredStyleData());
  EXPECT_CALL(css_parser_,
              ParseStyleDeclarationList(kFooBarDeclarationString, _))
      .WillOnce(Return(style));
  html_element->SetAttribute("style", kFooBarDeclarationString);
  EXPECT_EQ(1, html_element->attributes()->length());
  EXPECT_EQ(kFooBarDeclarationString,
            html_element->GetAttribute("style").value());
}

TEST_F(HTMLElementTest,
       GetAttributeStyleDoesNotMatchSetAttributeStyleAfterStyleMutation) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();
  scoped_refptr<cssom::CSSDeclaredStyleData> style(
      new cssom::CSSDeclaredStyleData());
  EXPECT_CALL(css_parser_,
              ParseStyleDeclarationList(kFooBarDeclarationString, _))
      .WillOnce(Return(style));
  html_element->SetAttribute("style", kFooBarDeclarationString);
  EXPECT_EQ(1, html_element->attributes()->length());
  EXPECT_CALL(css_parser_, ParsePropertyIntoDeclarationData("display", "inline",
                                                            _, style.get()))
      .WillOnce(InvokeCallback0(base::Bind(
          &cssom::CSSDeclaredStyleData::SetPropertyValueAndImportance,
          base::Unretained(style.get()), cssom::kDisplayProperty,
          cssom::KeywordValue::GetInline(), false)));
  html_element->style()->SetPropertyValue("display", "inline", NULL);

  EXPECT_NE(kFooBarDeclarationString,
            html_element->GetAttribute("style").value());
}

TEST_F(HTMLElementTest,
       GetAttributeStyleMatchesSerializedStyleAfterStyleMutation) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();
  scoped_refptr<cssom::CSSDeclaredStyleData> style(
      new cssom::CSSDeclaredStyleData());
  EXPECT_CALL(css_parser_,
              ParseStyleDeclarationList(kFooBarDeclarationString, _))
      .WillOnce(Return(style));
  html_element->SetAttribute("style", kFooBarDeclarationString);
  EXPECT_EQ(1, html_element->attributes()->length());
  EXPECT_CALL(css_parser_, ParsePropertyIntoDeclarationData("display", "inline",
                                                            _, style.get()))
      .WillOnce(InvokeCallback0(base::Bind(
          &cssom::CSSDeclaredStyleData::SetPropertyValueAndImportance,
          base::Unretained(style.get()), cssom::kDisplayProperty,
          cssom::KeywordValue::GetInline(), false)));
  html_element->style()->SetPropertyValue("display", "inline", NULL);

  EXPECT_EQ(kDisplayInlineDeclarationString,
            html_element->GetAttribute("style").value());
}

TEST_F(HTMLElementTest, Duplicate) {
  scoped_refptr<HTMLElement> html_element =
      document_->CreateElement("div")->AsHTMLElement();
  html_element->SetAttribute("a", "1");
  html_element->SetAttribute("b", "2");
  scoped_refptr<HTMLElement> new_html_element =
      html_element->Duplicate()->AsElement()->AsHTMLElement();
  ASSERT_TRUE(new_html_element);
  EXPECT_TRUE(new_html_element->AsHTMLDivElement());
  EXPECT_EQ(2, new_html_element->attributes()->length());
  EXPECT_EQ("1", new_html_element->GetAttribute("a").value());
  EXPECT_EQ("2", new_html_element->GetAttribute("b").value());
}

}  // namespace dom
}  // namespace cobalt
