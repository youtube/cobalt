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

#include "cobalt/dom/element.h"

#include <vector>

#include "base/run_loop.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/dom/attr.h"
#include "cobalt/dom/comment.h"
#include "cobalt/dom/dom_token_list.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/node_list.h"
#include "cobalt/dom/stats.h"
#include "cobalt/dom/testing/gtest_workarounds.h"
#include "cobalt/dom/testing/html_collection_testing.h"
#include "cobalt/dom/text.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

//////////////////////////////////////////////////////////////////////////
// ElementTest
//////////////////////////////////////////////////////////////////////////

class ElementTest : public ::testing::Test {
 protected:
  ElementTest();
  ~ElementTest() OVERRIDE;

  scoped_ptr<css_parser::Parser> css_parser_;
  dom::HTMLElementContext html_element_context_;
};

ElementTest::ElementTest()
    : css_parser_(css_parser::Parser::Create()),
      html_element_context_(NULL, css_parser_.get(), NULL, NULL) {
  EXPECT_TRUE(Stats::GetInstance()->CheckNoLeaks());
}

ElementTest::~ElementTest() {
  EXPECT_TRUE(Stats::GetInstance()->CheckNoLeaks());
}

//////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////

TEST_F(ElementTest, CreateElement) {
  scoped_refptr<Element> element = new Element();
  ASSERT_NE(NULL, element);

  EXPECT_EQ(Node::kElementNode, element->node_type());
  EXPECT_EQ("#element", element->node_name());

  EXPECT_EQ("#element", element->tag_name());
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

TEST_F(ElementTest, AsElement) {
  scoped_refptr<Element> element = new Element();
  scoped_refptr<Text> text = new Text("text");
  scoped_refptr<Node> node = element;

  EXPECT_EQ(element, node->AsElement());
  EXPECT_EQ(NULL, text->AsElement());
}

TEST_F(ElementTest, AttributeMethods) {
  scoped_refptr<Element> element = new Element();

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
  scoped_refptr<Element> element = new Element();
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
  scoped_refptr<Element> element = new Element();

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
  scoped_refptr<Element> element1 = new Element();
  scoped_refptr<Element> element2 = new Element();

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
  ASSERT_NE(NULL, previous_attribute);
  EXPECT_EQ("2", previous_attribute->value());
  EXPECT_EQ(std::string("1"), element2->GetAttribute("a"));

  // Make sure that the previous attribute is detached from element2.
  previous_attribute->set_value("3");
  EXPECT_EQ(std::string("1"), element2->GetAttribute("a"));
}

TEST_F(ElementTest, ClassList) {
  scoped_refptr<Element> element = new Element();
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
  scoped_refptr<Element> root = new Element();
  testing::TestGetElementsByClassName(root);
}

TEST_F(ElementTest, GetElementsByTagName) {
  scoped_refptr<Element> root = new Element();
  testing::TestGetElementsByTagName(root);
}

TEST_F(ElementTest, InnerHTML) {
  scoped_refptr<Element> root = new Element(&html_element_context_);

  // Manually construct the DOM tree and compare serialization result:
  // root
  //   element_a
  //     text
  //     element_b1
  //     text
  //     element_b2
  //     text
  //
  scoped_refptr<Element> element_a =
      root->AppendChild(new Element())->AsElement();
  element_a->SetAttribute("key", "value");

  element_a->AppendChild(new Text("\n  "));

  scoped_refptr<Element> element_b1 =
      element_a->AppendChild(new Element())->AsElement();
  element_b1->SetAttribute("just_key", "");

  element_a->AppendChild(new Text("\n  "));

  scoped_refptr<Element> element_b2 =
      element_a->AppendChild(new Element())->AsElement();
  element_b2->AppendChild(new Text("Text"));

  element_a->AppendChild(new Text("\n"));

  const char* kExpectedHTML =
      "<#element key=\"value\">\n"
      "  <#element just_key></#element>\n"
      "  <#element>Text</#element>\n"
      "</#element>";
  EXPECT_EQ(kExpectedHTML, root->inner_html());

  // Setting inner HTML should remove all previous children.
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
  const char* kAnotherHTML =
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
  EXPECT_TRUE(root->first_child()->IsElement());

  scoped_refptr<Element> element_1 = root->first_child()->AsElement();
  EXPECT_TRUE(element_1->HasAttribute("key"));
  EXPECT_EQ("value", element_1->GetAttribute("key").value_or(""));
  EXPECT_TRUE(element_1->first_child()->IsText());
  EXPECT_TRUE(element_1->next_sibling()->IsText());

  scoped_refptr<Text> text_2 = element_1->first_child()->AsText();
  EXPECT_TRUE(text_2->next_sibling()->IsElement());

  scoped_refptr<Element> element_3 = text_2->next_sibling()->AsElement();
  EXPECT_TRUE(element_3->HasAttributes());
  EXPECT_TRUE(element_3->next_sibling()->IsText());

  scoped_refptr<Text> text_4 = element_3->next_sibling()->AsText();
  EXPECT_TRUE(text_4->next_sibling()->IsElement());

  scoped_refptr<Element> element_5 = text_4->next_sibling()->AsElement();
  EXPECT_TRUE(element_5->first_child()->IsText());
  EXPECT_EQ("Text", element_5->first_child()->AsText()->data());
  EXPECT_TRUE(element_5->next_sibling()->IsText());

  scoped_refptr<Text> text_8 = element_1->next_sibling()->AsText();
  EXPECT_TRUE(text_8->next_sibling()->IsElement());

  scoped_refptr<Element> element_9 = text_8->next_sibling()->AsElement();
  EXPECT_TRUE(element_9->first_child()->IsText());

  scoped_refptr<Text> text_10 = element_9->first_child()->AsText();
  EXPECT_TRUE(text_10->next_sibling()->IsComment());

  scoped_refptr<Comment> comment_11 = text_10->next_sibling()->AsComment();
  EXPECT_EQ("Comment", comment_11->data());
  EXPECT_TRUE(comment_11->next_sibling()->IsText());

  // Compare serialization result with the original HTML.
  EXPECT_EQ(kAnotherHTML, root->inner_html());
}

TEST_F(ElementTest, OuterHTML) {
  scoped_refptr<Element> root = new Element(&html_element_context_);

  // Manually construct the DOM tree and compare serialization result:
  // root
  //   element_a
  //     text
  //     element_b1
  //     text
  //     element_b2
  //     text
  //
  scoped_refptr<Element> element_a =
      root->AppendChild(new Element(&html_element_context_))->AsElement();
  element_a->SetAttribute("key", "value");

  element_a->AppendChild(new Text("\n  "));

  scoped_refptr<Element> element_b1 =
      element_a->AppendChild(new Element())->AsElement();
  element_b1->SetAttribute("just_key", "");

  element_a->AppendChild(new Text("\n  "));

  scoped_refptr<Element> element_b2 =
      element_a->AppendChild(new Element())->AsElement();
  element_b2->AppendChild(new Text("Text"));

  element_a->AppendChild(new Text("\n"));

  const char* kExpectedHTML =
      "<#element><#element key=\"value\">\n"
      "  <#element just_key></#element>\n"
      "  <#element>Text</#element>\n"
      "</#element></#element>";
  EXPECT_EQ(kExpectedHTML, root->outer_html());

  // Setting outer HTML should remove the node from its parent.
  element_a->set_outer_html("");
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
  root->AppendChild(new Element(&html_element_context_));
  const char* kAnotherHTML =
      "<div key=\"value\">\n"
      "  <div just_key></div>\n"
      "  <div>Text</div>\n"
      "</div>\n"
      "<div>\n"
      "  This is the second div under root.\n"
      "  <!--Comment-->\n"
      "</div>";
  root->first_element_child()->set_outer_html(kAnotherHTML);
  EXPECT_EQ(2, root->child_element_count());
  EXPECT_EQ(3, root->child_nodes()->length());
  EXPECT_TRUE(root->first_child()->IsElement());

  scoped_refptr<Element> element_1 = root->first_child()->AsElement();
  EXPECT_TRUE(element_1->HasAttribute("key"));
  EXPECT_EQ("value", element_1->GetAttribute("key").value_or(""));
  EXPECT_TRUE(element_1->first_child()->IsText());
  EXPECT_TRUE(element_1->next_sibling()->IsText());

  scoped_refptr<Text> text_2 = element_1->first_child()->AsText();
  EXPECT_TRUE(text_2->next_sibling()->IsElement());

  scoped_refptr<Element> element_3 = text_2->next_sibling()->AsElement();
  EXPECT_TRUE(element_3->HasAttributes());
  EXPECT_TRUE(element_3->next_sibling()->IsText());

  scoped_refptr<Text> text_4 = element_3->next_sibling()->AsText();
  EXPECT_TRUE(text_4->next_sibling()->IsElement());

  scoped_refptr<Element> element_5 = text_4->next_sibling()->AsElement();
  EXPECT_TRUE(element_5->first_child()->IsText());
  EXPECT_EQ("Text", element_5->first_child()->AsText()->data());
  EXPECT_TRUE(element_5->next_sibling()->IsText());

  scoped_refptr<Text> text_8 = element_1->next_sibling()->AsText();
  EXPECT_TRUE(text_8->next_sibling()->IsElement());

  scoped_refptr<Element> element_9 = text_8->next_sibling()->AsElement();
  EXPECT_TRUE(element_9->first_child()->IsText());

  scoped_refptr<Text> text_10 = element_9->first_child()->AsText();
  EXPECT_TRUE(text_10->next_sibling()->IsComment());

  scoped_refptr<Comment> comment_11 = text_10->next_sibling()->AsComment();
  EXPECT_EQ("Comment", comment_11->data());
  EXPECT_TRUE(comment_11->next_sibling()->IsText());

  // Compare serialization result with the original HTML.
  EXPECT_EQ(kAnotherHTML, root->inner_html());
}

TEST_F(ElementTest, QuerySelector) {
  scoped_refptr<Element> root = new Element(&html_element_context_);
  root->AppendChild(
      html_element_context_.html_element_factory()->CreateHTMLElement("div"));
  root->AppendChild(
      html_element_context_.html_element_factory()->CreateHTMLElement("div"));
  root->QuerySelector("div");
  EXPECT_FALSE(root->QuerySelector("span"));
  // QuerySelector should return first matching child.
  EXPECT_EQ(root->first_element_child(), root->QuerySelector("div"));
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
  scoped_refptr<Element> root = new Element();
  root->AppendChild(new Element())->AppendChild(new Text("This "));
  root->AppendChild(new Element())->AppendChild(new Text("is "));
  root->AppendChild(new Element())->AppendChild(new Comment("not "));
  root->AppendChild(new Element())->AppendChild(new Text("Sparta."));
  // NodeValue should always be NULL.
  EXPECT_EQ(base::nullopt, root->node_value());
  // TextContent should be all texts concatenated.
  EXPECT_EQ("This is Sparta.", root->text_content().value());

  // After setting new text content, check the result.
  const char* kTextContent = "New text content";
  root->set_text_content(std::string(kTextContent));
  EXPECT_EQ(kTextContent, root->text_content().value());

  // There should be only one text child node.
  scoped_refptr<Node> child = root->first_child();
  EXPECT_NE(NULL, child);
  EXPECT_TRUE(child->IsText());
  EXPECT_EQ(child, root->last_child());

  // Setting text content as empty string shouldn't add new child.
  root->set_text_content(std::string());
  EXPECT_EQ(NULL, root->first_child());
}

}  // namespace dom
}  // namespace cobalt
