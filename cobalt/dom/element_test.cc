// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/element.h"

#include <vector>

#include "base/run_loop.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/dom/attr.h"
#include "cobalt/dom/comment.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_rect.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/dom_token_list.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/node_list.h"
#include "cobalt/dom/testing/gtest_workarounds.h"
#include "cobalt/dom/testing/html_collection_testing.h"
#include "cobalt/dom/text.h"
#include "cobalt/dom/xml_document.h"
#include "cobalt/dom_parser/parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

//////////////////////////////////////////////////////////////////////////
// ElementTest
//////////////////////////////////////////////////////////////////////////

class ElementTest : public ::testing::Test {
 protected:
  ElementTest();
  ~ElementTest() override;

  scoped_ptr<css_parser::Parser> css_parser_;
  scoped_ptr<dom_parser::Parser> dom_parser_;
  scoped_ptr<DomStatTracker> dom_stat_tracker_;
  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
  scoped_refptr<XMLDocument> xml_document_;
};

ElementTest::ElementTest()
    : css_parser_(css_parser::Parser::Create()),
      dom_parser_(new dom_parser::Parser()),
      dom_stat_tracker_(new DomStatTracker("ElementTest")),
      html_element_context_(NULL, css_parser_.get(), dom_parser_.get(), NULL,
                            NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                            NULL, NULL, dom_stat_tracker_.get(), "",
                            base::kApplicationStateStarted) {
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  document_ = new Document(&html_element_context_);
  xml_document_ = new XMLDocument(&html_element_context_);
}

ElementTest::~ElementTest() {
  xml_document_ = NULL;
  document_ = NULL;
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
}

//////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////

TEST_F(ElementTest, CreateElement) {
  scoped_refptr<Element> element =
      new Element(document_, base::Token("element"));
  ASSERT_TRUE(element);

  EXPECT_EQ(Node::kElementNode, element->node_type());
  EXPECT_EQ("ELEMENT", element->node_name());
  EXPECT_EQ("ELEMENT", element->tag_name());
  EXPECT_EQ("element", element->local_name());
  EXPECT_EQ("", element->id());
  EXPECT_EQ("", element->class_name());

  element->set_id("id");
  element->set_class_name("class");
  EXPECT_EQ("id", element->id());
  EXPECT_EQ("class", element->class_name());

  element->SetAttribute("id", "other_id");
  element->SetAttribute("class", "other_class");
  EXPECT_EQ("other_id", element->id());
  EXPECT_EQ("other_class", element->class_name());
}

TEST_F(ElementTest, Duplicate) {
  scoped_refptr<Element> element =
      new Element(document_, base::Token("element"));
  element->SetAttribute("a", "1");
  element->SetAttribute("b", "2");
  scoped_refptr<Element> new_element = element->Duplicate()->AsElement();
  ASSERT_TRUE(new_element);
  EXPECT_EQ(2, new_element->attributes()->length());
  EXPECT_EQ("1", new_element->GetAttribute("a").value());
  EXPECT_EQ("2", new_element->GetAttribute("b").value());
}

TEST_F(ElementTest, AsElement) {
  scoped_refptr<Element> element =
      new Element(document_, base::Token("element"));
  scoped_refptr<Text> text = new Text(document_, "text");
  scoped_refptr<Node> node = element;

  EXPECT_EQ(element, node->AsElement());
  EXPECT_EQ(NULL, text->AsElement());
}

TEST_F(ElementTest, TagNameAndLocalName) {
  scoped_refptr<Element> element_in_html =
      new Element(document_, base::Token("eLeMeNt"));
  EXPECT_EQ("ELEMENT", element_in_html->tag_name());
  EXPECT_EQ("eLeMeNt", element_in_html->local_name());

  scoped_refptr<Element> element_in_xml =
      new Element(xml_document_, base::Token("eLeMeNt"));
  EXPECT_EQ("eLeMeNt", element_in_xml->tag_name());
  EXPECT_EQ("eLeMeNt", element_in_html->local_name());
}

TEST_F(ElementTest, AttributeMethods) {
  scoped_refptr<Element> element =
      new Element(document_, base::Token("element"));

  element->SetAttribute("a", "1");
  EXPECT_TRUE(element->HasAttribute("a"));
  EXPECT_EQ(std::string("1"), element->GetAttribute("a"));

  element->SetAttribute("b", "2");
  EXPECT_TRUE(element->HasAttribute("b"));
  EXPECT_EQ(std::string("2"), element->GetAttribute("b"));

  EXPECT_FALSE(element->HasAttribute("c"));
  EXPECT_EQ(base::nullopt, element->GetAttribute("c"));

  element->RemoveAttribute("a");
  EXPECT_FALSE(element->HasAttribute("a"));

  element->SetAttribute("b", "3");
  EXPECT_EQ(std::string("3"), element->GetAttribute("b"));
}

TEST_F(ElementTest, AttributesPropertyGetAndRemove) {
  scoped_refptr<Element> element =
      new Element(document_, base::Token("element"));
  scoped_refptr<NamedNodeMap> attributes = element->attributes();

  // Start with nothing.
  EXPECT_EQ(0, attributes->length());

  // Make sure that setting an attribute through the element affects
  // the NamedNodeMap.
  element->SetAttribute("a", "1");
  EXPECT_EQ(1, attributes->length());
  EXPECT_EQ("a", attributes->GetNamedItem("a")->name());
  EXPECT_EQ("1", attributes->GetNamedItem("a")->value());
  EXPECT_EQ("a", attributes->Item(0)->name());
  EXPECT_EQ("1", attributes->Item(0)->value());

  // Make sure that changing an attribute through the element affects
  // the NamedNodeMap.
  element->SetAttribute("a", "2");
  EXPECT_EQ("2", attributes->GetNamedItem("a")->value());

  // Make sure that adding another attribute through the element affects
  // the NamedNodeMap.
  element->SetAttribute("b", "2");
  EXPECT_EQ(2, attributes->length());
  EXPECT_EQ("b", attributes->GetNamedItem("b")->name());
  EXPECT_EQ("2", attributes->GetNamedItem("b")->value());
  EXPECT_EQ("b", attributes->Item(1)->name());
  EXPECT_EQ("2", attributes->Item(1)->value());

  // Make sure that removing an attribute through the element affects
  // the NamedNodeMap.
  element->RemoveAttribute("a");
  EXPECT_EQ(1, attributes->length());
  EXPECT_EQ("b", attributes->GetNamedItem("b")->name());
  EXPECT_EQ("2", attributes->GetNamedItem("b")->value());
  EXPECT_EQ("b", attributes->Item(0)->name());
  EXPECT_EQ("2", attributes->Item(0)->value());

  // Make sure that a reference to an Attr is still working if a reference to
  // the containing NamedNodeMap is released.
  scoped_refptr<Attr> attribute = attributes->Item(0);
  attributes = NULL;

  // Setting through the attribute should affect the element.
  attribute->set_value("3");
  EXPECT_EQ(std::string("3"), element->GetAttribute("b"));

  // Setting through the element should effect the attribute.
  element->SetAttribute("b", "2");
  EXPECT_EQ("2", attribute->value());

  attributes = element->attributes();

  // Removing an invalid attribute shouldn't change anything.
  EXPECT_EQ(NULL, attributes->RemoveNamedItem("a"));
  EXPECT_EQ(1, attributes->length());

  // Test that removing an attribute through NamedNodeMap works.
  EXPECT_EQ(attribute, attributes->RemoveNamedItem("b"));
  EXPECT_EQ(0, attributes->length());
  EXPECT_FALSE(element->HasAttributes());
}

TEST_F(ElementTest, AttributesPropertySet) {
  scoped_refptr<Element> element =
      new Element(document_, base::Token("element"));

  scoped_refptr<Attr> attribute = new Attr("a", "1", NULL);
  EXPECT_EQ("1", attribute->value());

  attribute->set_value("2");
  EXPECT_EQ("2", attribute->value());

  scoped_refptr<NamedNodeMap> attributes = element->attributes();
  EXPECT_EQ(NULL, attributes->SetNamedItem(attribute));
  EXPECT_EQ(std::string("2"), element->GetAttribute("a"));

  attributes->SetNamedItem(new Attr("a", "3", NULL));
  EXPECT_EQ(std::string("3"), element->GetAttribute("a"));
}

TEST_F(ElementTest, AttributesPropertyTransfer) {
  scoped_refptr<Element> element1 =
      new Element(document_, base::Token("element1"));
  scoped_refptr<Element> element2 =
      new Element(document_, base::Token("element2"));

  scoped_refptr<NamedNodeMap> attributes1 = element1->attributes();
  scoped_refptr<NamedNodeMap> attributes2 = element2->attributes();

  element1->SetAttribute("a", "1");
  element2->SetAttribute("a", "2");

  EXPECT_EQ(1, attributes1->length());
  EXPECT_EQ(1, attributes2->length());

  scoped_refptr<Attr> attribute = attributes1->Item(0);

  // Setting an attribute to the same NamedNodeMap that contains it should
  // work.
  EXPECT_EQ(attribute, attributes1->SetNamedItem(attribute));

  // This should fail since attribute is still attached to element1.
  EXPECT_EQ(NULL, attributes2->SetNamedItem(attribute));
  EXPECT_EQ(std::string("2"), element2->GetAttribute("a"));

  element1->RemoveAttribute("a");

  // Should work this time around since we deleted attribute from element1.
  scoped_refptr<Attr> previous_attribute = attributes2->SetNamedItem(attribute);
  ASSERT_TRUE(previous_attribute);
  EXPECT_EQ("2", previous_attribute->value());
  EXPECT_EQ(std::string("1"), element2->GetAttribute("a"));

  // Make sure that the previous attribute is detached from element2.
  previous_attribute->set_value("3");
  EXPECT_EQ(std::string("1"), element2->GetAttribute("a"));
}

TEST_F(ElementTest, ClassList) {
  scoped_refptr<Element> element = new Element(document_, base::Token("root"));
  scoped_refptr<DOMTokenList> class_list = element->class_list();
  element->set_class_name("  a             a b d");
  EXPECT_EQ(4, class_list->length());
  EXPECT_TRUE(class_list->Contains("a"));
  EXPECT_TRUE(class_list->Contains("b"));
  EXPECT_FALSE(class_list->Contains("c"));

  std::vector<std::string> token_list;

  // Add class
  token_list.clear();
  token_list.push_back("c");
  class_list->Add(token_list);
  EXPECT_EQ(5, class_list->length());
  EXPECT_TRUE(class_list->Contains("c"));

  // Invalid token, should throw JS Exceptions.
  token_list.clear();
  token_list.push_back("");
  token_list.push_back(" z");
  token_list.push_back("\tz");
  class_list->Add(token_list);
  EXPECT_EQ(5, class_list->length());
  EXPECT_FALSE(class_list->Contains("z"));

  // Remove class
  token_list.clear();
  token_list.push_back("a");
  class_list->Remove(token_list);
  EXPECT_EQ(3, class_list->length());
  EXPECT_FALSE(class_list->Contains("a"));

  EXPECT_EQ("b d c", element->class_name());

  // Item
  EXPECT_EQ(std::string("b"), class_list->Item(0));
  EXPECT_EQ(std::string("d"), class_list->Item(1));
  EXPECT_EQ(std::string("c"), class_list->Item(2));
  EXPECT_EQ(base::nullopt, class_list->Item(3));
}

TEST_F(ElementTest, GetElementsByClassName) {
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));
  testing::TestGetElementsByClassName(root);
}

TEST_F(ElementTest, GetElementsByTagName) {
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));
  testing::TestGetElementsByTagName(root);
}

TEST_F(ElementTest, GetBoundingClientRect) {
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));
  scoped_refptr<DOMRect> rect = root->GetBoundingClientRect();
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

TEST_F(ElementTest, ClientTopLeftWidthHeight) {
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));
  EXPECT_FLOAT_EQ(root->client_top(), 0.0f);
  EXPECT_FLOAT_EQ(root->client_left(), 0.0f);
  EXPECT_FLOAT_EQ(root->client_width(), 0.0f);
  EXPECT_FLOAT_EQ(root->client_height(), 0.0f);
}

TEST_F(ElementTest, InnerHTML) {
  // Manually construct the DOM tree and compare serialization result:
  // root
  //   element_a
  //     text
  //     element_b1
  //     text
  //     element_b2
  //     text
  //
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));
  scoped_refptr<Element> element_a =
      root->AppendChild(new Element(document_, base::Token("element_a")))
          ->AsElement();
  element_a->SetAttribute("key", "value");

  element_a->AppendChild(new Text(document_, "\n  "));

  scoped_refptr<Element> element_b1 =
      element_a->AppendChild(new Element(document_, base::Token("element_b1")))
          ->AsElement();
  element_b1->SetAttribute("just_key", "");

  element_a->AppendChild(new Text(document_, "\n  "));

  scoped_refptr<Element> element_b2 =
      element_a->AppendChild(new Element(document_, base::Token("element_b2")))
          ->AsElement();
  element_b2->AppendChild(new Text(document_, "Text"));

  element_a->AppendChild(new Text(document_, "\n"));

  const char kExpectedHTML[] =
      "<element_a key=\"value\">\n"
      "  <element_b1 just_key></element_b1>\n"
      "  <element_b2>Text</element_b2>\n"
      "</element_a>";
  EXPECT_EQ(kExpectedHTML, root->inner_html());
}

TEST_F(ElementTest, SetInnerHTML) {
  // Setting inner HTML should remove all previous children.
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));
  scoped_refptr<Element> element_a =
      root->AppendChild(new Element(document_, base::Token("element_a")))
          ->AsElement();
  root->set_inner_html("");
  EXPECT_FALSE(root->HasChildNodes());

  // After setting valid HTML, check whether the children are set up correctly.
  // The expected structure is:
  // root
  //   element_1
  //     text_2
  //     element_3
  //     text_4
  //     element_5
  //       text_6
  //     text_7
  //   text_8
  //   element_9
  //     text_10
  //     comment_11
  //     text_12
  //
  const char kAnotherHTML[] =
      "<div key=\"value\">\n"
      "  <div just_key></div>\n"
      "  <div>Text</div>\n"
      "</div>\n"
      "<div>\n"
      "  This is the second div under root.\n"
      "  <!--Comment-->\n"
      "</div>";
  root->set_inner_html(kAnotherHTML);
  EXPECT_EQ(2, root->child_element_count());
  EXPECT_EQ(3, root->child_nodes()->length());
  ASSERT_TRUE(root->first_child());
  EXPECT_TRUE(root->first_child()->IsElement());

  scoped_refptr<Element> element_1 = root->first_child()->AsElement();
  EXPECT_TRUE(element_1->HasAttribute("key"));
  EXPECT_EQ("value", element_1->GetAttribute("key").value_or(""));
  ASSERT_TRUE(element_1->first_child());
  EXPECT_TRUE(element_1->first_child()->IsText());
  ASSERT_TRUE(element_1->next_sibling());
  EXPECT_TRUE(element_1->next_sibling()->IsText());

  scoped_refptr<Text> text_2 = element_1->first_child()->AsText();
  ASSERT_TRUE(text_2->next_sibling());
  EXPECT_TRUE(text_2->next_sibling()->IsElement());

  scoped_refptr<Element> element_3 = text_2->next_sibling()->AsElement();
  EXPECT_TRUE(element_3->HasAttributes());
  ASSERT_TRUE(element_3->next_sibling());
  EXPECT_TRUE(element_3->next_sibling()->IsText());

  scoped_refptr<Text> text_4 = element_3->next_sibling()->AsText();
  ASSERT_TRUE(text_4->next_sibling());
  EXPECT_TRUE(text_4->next_sibling()->IsElement());

  scoped_refptr<Element> element_5 = text_4->next_sibling()->AsElement();
  EXPECT_TRUE(element_5->first_child()->IsText());
  EXPECT_EQ("Text", element_5->first_child()->AsText()->data());
  ASSERT_TRUE(element_5->next_sibling());
  EXPECT_TRUE(element_5->next_sibling()->IsText());

  scoped_refptr<Text> text_8 = element_1->next_sibling()->AsText();
  ASSERT_TRUE(text_8->next_sibling());
  EXPECT_TRUE(text_8->next_sibling()->IsElement());

  scoped_refptr<Element> element_9 = text_8->next_sibling()->AsElement();
  ASSERT_TRUE(element_9->first_child());
  EXPECT_TRUE(element_9->first_child()->IsText());

  scoped_refptr<Text> text_10 = element_9->first_child()->AsText();
  ASSERT_TRUE(text_10->next_sibling());
  EXPECT_TRUE(text_10->next_sibling()->IsComment());

  scoped_refptr<Comment> comment_11 = text_10->next_sibling()->AsComment();
  EXPECT_EQ("Comment", comment_11->data());
  ASSERT_TRUE(comment_11->next_sibling());
  EXPECT_TRUE(comment_11->next_sibling()->IsText());

  // Compare serialization result with the original HTML.
  EXPECT_EQ(kAnotherHTML, root->inner_html());
}

TEST_F(ElementTest, InnerHTMLGetterReturnsText) {
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));
  root->AppendChild(new Text(document_, "Cobalt"));
  EXPECT_EQ(root->inner_html(), "Cobalt");
}

TEST_F(ElementTest, InnerHTMLSetterCreatesElement) {
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));
  scoped_refptr<Element> element =
      root->AppendChild(new Element(document_, base::Token("element")))
          ->AsElement();
  element->set_inner_html("<div>Cobalt</div>");
  ASSERT_TRUE(element->first_child());
  EXPECT_TRUE(element->first_child()->IsElement());
}

TEST_F(ElementTest, InnerHTMLSetterWithTextCreatesTextNode) {
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));
  scoped_refptr<Element> element =
      root->AppendChild(new Element(document_, base::Token("element")))
          ->AsElement();
  element->set_inner_html("Cobalt");
  ASSERT_TRUE(element->first_child());
  EXPECT_TRUE(element->first_child()->IsText());
}

TEST_F(ElementTest, InnerHTMLSetterAndGetterAreConsistent) {
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));
  root->set_inner_html("<div>Cobalt</div>");
  EXPECT_EQ(root->inner_html(), "<div>Cobalt</div>");
}

TEST_F(ElementTest, InnerHTMLSetterAndGetterAreConsistentWithText) {
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));
  root->set_inner_html("Cobalt");
  EXPECT_EQ(root->inner_html(), "Cobalt");
}

TEST_F(ElementTest, OuterHTML) {
  // Manually construct the DOM tree and compare serialization result:
  // root
  //   element_a
  //     text
  //     element_b1
  //     text
  //     element_b2
  //     text
  //
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));
  scoped_refptr<Element> element_a =
      root->AppendChild(new Element(document_, base::Token("element_a")))
          ->AsElement();
  element_a->SetAttribute("key", "value");

  element_a->AppendChild(new Text(document_, "\n  "));

  scoped_refptr<Element> element_b1 =
      element_a->AppendChild(new Element(document_, base::Token("element_b1")))
          ->AsElement();
  element_b1->SetAttribute("just_key", "");

  element_a->AppendChild(new Text(document_, "\n  "));

  scoped_refptr<Element> element_b2 =
      element_a->AppendChild(new Element(document_, base::Token("element_b2")))
          ->AsElement();
  element_b2->AppendChild(new Text(document_, "Text"));

  element_a->AppendChild(new Text(document_, "\n"));

  const char kExpectedHTML[] =
      "<root><element_a key=\"value\">\n"
      "  <element_b1 just_key></element_b1>\n"
      "  <element_b2>Text</element_b2>\n"
      "</element_a></root>";
  EXPECT_EQ(kExpectedHTML, root->outer_html(NULL));

  // Setting outer HTML should remove the node from its parent.
  element_a->set_outer_html("", NULL);
  EXPECT_FALSE(root->HasChildNodes());

  // After setting valid HTML, check whether the children are set up correctly.
  // The expected structure is:
  // root
  //   element_1
  //     text_2
  //     element_3
  //     text_4
  //     element_5
  //       text_6
  //     text_7
  //   text_8
  //   element_9
  //     text_10
  //     comment_11
  //     text_12
  //
  root->AppendChild(new Element(document_, base::Token("root")));
  const char kAnotherHTML[] =
      "<div key=\"value\">\n"
      "  <div just_key></div>\n"
      "  <div>Text</div>\n"
      "</div>\n"
      "<div>\n"
      "  This is the second div under root.\n"
      "  <!--Comment-->\n"
      "</div>";
  root->first_element_child()->set_outer_html(kAnotherHTML, NULL);
  EXPECT_EQ(2, root->child_element_count());
  EXPECT_EQ(3, root->child_nodes()->length());
  ASSERT_TRUE(root->first_child());
  EXPECT_TRUE(root->first_child()->IsElement());

  scoped_refptr<Element> element_1 = root->first_child()->AsElement();
  EXPECT_TRUE(element_1->HasAttribute("key"));
  EXPECT_EQ("value", element_1->GetAttribute("key").value_or(""));
  ASSERT_TRUE(element_1->first_child());
  EXPECT_TRUE(element_1->first_child()->IsText());
  ASSERT_TRUE(element_1->next_sibling());
  EXPECT_TRUE(element_1->next_sibling()->IsText());

  scoped_refptr<Text> text_2 = element_1->first_child()->AsText();
  ASSERT_TRUE(text_2->next_sibling());
  EXPECT_TRUE(text_2->next_sibling()->IsElement());

  scoped_refptr<Element> element_3 = text_2->next_sibling()->AsElement();
  EXPECT_TRUE(element_3->HasAttributes());
  ASSERT_TRUE(element_3->next_sibling());
  EXPECT_TRUE(element_3->next_sibling()->IsText());

  scoped_refptr<Text> text_4 = element_3->next_sibling()->AsText();
  ASSERT_TRUE(text_4->next_sibling());
  EXPECT_TRUE(text_4->next_sibling()->IsElement());

  scoped_refptr<Element> element_5 = text_4->next_sibling()->AsElement();
  ASSERT_TRUE(element_5->first_child());
  EXPECT_TRUE(element_5->first_child()->IsText());
  EXPECT_EQ("Text", element_5->first_child()->AsText()->data());
  ASSERT_TRUE(element_5->next_sibling());
  EXPECT_TRUE(element_5->next_sibling()->IsText());

  scoped_refptr<Text> text_8 = element_1->next_sibling()->AsText();
  ASSERT_TRUE(text_8->next_sibling());
  EXPECT_TRUE(text_8->next_sibling()->IsElement());

  scoped_refptr<Element> element_9 = text_8->next_sibling()->AsElement();
  ASSERT_TRUE(element_9->first_child());
  EXPECT_TRUE(element_9->first_child()->IsText());

  scoped_refptr<Text> text_10 = element_9->first_child()->AsText();
  ASSERT_TRUE(text_10->next_sibling());
  EXPECT_TRUE(text_10->next_sibling()->IsComment());

  scoped_refptr<Comment> comment_11 = text_10->next_sibling()->AsComment();
  EXPECT_EQ("Comment", comment_11->data());
  ASSERT_TRUE(comment_11->next_sibling());
  EXPECT_TRUE(comment_11->next_sibling()->IsText());

  // Compare serialization result with the original HTML.
  EXPECT_EQ(kAnotherHTML, root->inner_html());
}

TEST_F(ElementTest, NodeValueAndTextContent) {
  // Setup the following structure and check the nodeValue and textContent:
  // root
  //   element
  //     text("This ")
  //   element
  //     text("is ")
  //   element
  //     comment("not ")
  //   element
  //     text("Sparta.")
  scoped_refptr<Element> root = new Element(document_, base::Token("root"));
  root->AppendChild(new Element(document_, base::Token("element")))
      ->AppendChild(new Text(document_, "This "));
  root->AppendChild(new Element(document_, base::Token("element")))
      ->AppendChild(new Text(document_, "is "));
  root->AppendChild(new Element(document_, base::Token("element")))
      ->AppendChild(new Comment(document_, "not "));
  root->AppendChild(new Element(document_, base::Token("element")))
      ->AppendChild(new Text(document_, "Sparta."));
  // NodeValue should always be NULL.
  EXPECT_EQ(base::nullopt, root->node_value());
  // TextContent should be all texts concatenated.
  EXPECT_EQ("This is Sparta.", root->text_content().value());

  // After setting new text content, check the result.
  const char kTextContent[] = "New text content";
  root->set_text_content(std::string(kTextContent));
  EXPECT_EQ(kTextContent, root->text_content().value());

  // There should be only one text child node.
  scoped_refptr<Node> child = root->first_child();
  EXPECT_TRUE(child);
  EXPECT_TRUE(child->IsText());
  EXPECT_EQ(child, root->last_child());

  // Setting text content as empty string shouldn't add new child.
  root->set_text_content(std::string());
  EXPECT_EQ(NULL, root->first_child());
}

}  // namespace dom
}  // namespace cobalt
