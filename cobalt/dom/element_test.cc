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

#include "cobalt/dom/attr.h"
#include "cobalt/dom/dom_token_list.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/stats.h"
#include "cobalt/dom/testing/gtest_workarounds.h"
#include "cobalt/dom/testing/html_collection_testing.h"
#include "cobalt/dom/testing/parent_node_testing.h"
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
};

ElementTest::ElementTest() {
  EXPECT_TRUE(Stats::GetInstance()->CheckNoLeaks());
}

ElementTest::~ElementTest() {
  EXPECT_TRUE(Stats::GetInstance()->CheckNoLeaks());
}

//////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////

TEST_F(ElementTest, CreateElement) {
  scoped_refptr<Element> element = Element::Create();
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
  scoped_refptr<Element> element = Element::Create();
  scoped_refptr<Text> text = Text::Create("text");
  scoped_refptr<Node> node = element;

  EXPECT_EQ(element, node->AsElement());
  EXPECT_EQ(NULL, text->AsElement());
}

TEST_F(ElementTest, AttributeMethods) {
  scoped_refptr<Element> element = Element::Create();

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
  scoped_refptr<Element> element = Element::Create();
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
  scoped_refptr<Element> element = Element::Create();

  scoped_refptr<Attr> attribute = Attr::Create("a", "1", NULL);
  EXPECT_EQ("1", attribute->value());

  attribute->set_value("2");
  EXPECT_EQ("2", attribute->value());

  scoped_refptr<NamedNodeMap> attributes = element->attributes();
  EXPECT_EQ(NULL, attributes->SetNamedItem(attribute));
  EXPECT_EQ(std::string("2"), element->GetAttribute("a"));

  attributes->SetNamedItem(Attr::Create("a", "3", NULL));
  EXPECT_EQ(std::string("3"), element->GetAttribute("a"));
}

TEST_F(ElementTest, AttributesPropertyTransfer) {
  scoped_refptr<Element> element1 = Element::Create();
  scoped_refptr<Element> element2 = Element::Create();

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
  scoped_refptr<Element> element = Element::Create();
  scoped_refptr<DOMTokenList> class_list = element->class_list();
  element->set_class_name("  a             a b d");
  EXPECT_EQ(4, class_list->length());
  EXPECT_TRUE(class_list->Contains("a"));
  EXPECT_TRUE(class_list->Contains("b"));
  EXPECT_FALSE(class_list->Contains("c"));

  // Add class
  class_list->Add("c");
  EXPECT_EQ(5, class_list->length());
  EXPECT_TRUE(class_list->Contains("c"));

  // Invalid token, should throw JS Exceptions.
  class_list->Add("");
  class_list->Add(" z");
  class_list->Add("\tz");
  EXPECT_EQ(5, class_list->length());
  EXPECT_FALSE(class_list->Contains("z"));

  // Remove class
  class_list->Remove("a");
  EXPECT_EQ(3, class_list->length());
  EXPECT_FALSE(class_list->Contains("a"));

  EXPECT_EQ("b d c", element->class_name());

  // Item
  EXPECT_EQ(std::string("b"), class_list->Item(0));
  EXPECT_EQ(std::string("d"), class_list->Item(1));
  EXPECT_EQ(std::string("c"), class_list->Item(2));
  EXPECT_EQ(base::nullopt, class_list->Item(3));
}

TEST_F(ElementTest, ParentNodeAllExceptChilden) {
  scoped_refptr<Element> root = Element::Create();
  testing::TestParentNodeAllExceptChilden(root);
}

TEST_F(ElementTest, ParentNodeChildren) {
  scoped_refptr<Element> root = Element::Create();
  testing::TestParentNodeChildren(root);
}

TEST_F(ElementTest, GetElementsByClassName) {
  scoped_refptr<Element> root = Element::Create();
  testing::TestGetElementsByClassName(root);
}

// TODO(***REMOVED***): This test should be on HTMLElement.
/*
TEST_F(ElementTest, GetElementsByTagName) {
  scoped_refptr<Element> root = Element::Create();
  testing::TestGetElementsByTagName(root);
}
*/

TEST_F(ElementTest, InnerHTML) {
  scoped_refptr<Element> root = Element::Create();

  scoped_refptr<Element> element_a =
      root->AppendChild(Element::Create())->AsElement();
  element_a->SetAttribute("key", "value");

  element_a->AppendChild(Text::Create("\n  "));

  scoped_refptr<Element> element_b1 =
      element_a->AppendChild(Element::Create())->AsElement();
  element_b1->SetAttribute("just_key", "");

  element_a->AppendChild(Text::Create("\n  "));

  scoped_refptr<Element> element_b2 =
      element_a->AppendChild(Element::Create())->AsElement();
  element_b2->AppendChild(Text::Create("Text"));

  element_a->AppendChild(Text::Create("\n"));

  const char* kExpectedHTML =
      "<#element key=\"value\">\n"
      "  <#element just_key></#element>\n"
      "  <#element>Text</#element>\n"
      "</#element>";

  EXPECT_EQ(kExpectedHTML, root->inner_html());
}

}  // namespace dom
}  // namespace cobalt
