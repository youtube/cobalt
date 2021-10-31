// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_TESTING_HTML_COLLECTION_TESTING_H_
#define COBALT_DOM_TESTING_HTML_COLLECTION_TESTING_H_

#include "cobalt/dom/document.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_element_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
namespace testing {

// Given an empty node of a type T, test the functionality of
// T::GetElementsByClassName.
template <typename T>
void TestGetElementsByClassName(const scoped_refptr<T>& node);

// Given an empty node of a type T, test the functionality of
// T::GetElementsByTagName.
template <typename T>
void TestGetElementsByTagName(const scoped_refptr<T>& node);

/////////////////////////////////////////////////////////////////////////////
// Implementation
/////////////////////////////////////////////////////////////////////////////

template <typename T>
void TestGetElementsByClassName(const scoped_refptr<T>& node) {
  const scoped_refptr<Node> kNullNode{};
  const scoped_refptr<HTMLCollection> kNullCollection{};

  Document* document = node->node_document();

  if (!document) {
    document = node->AsDocument();
    DCHECK(document);
  }

  // Construct a tree:
  // node
  //   a1
  //     b1
  //       c1
  //   a2
  //     d1
  //     d2
  scoped_refptr<Node> a1 =
      node->AppendChild(new Element(document, base::Token("a1")));
  scoped_refptr<Node> a2 =
      node->AppendChild(new Element(document, base::Token("a2")));
  scoped_refptr<Node> b1 =
      a1->AppendChild(new Element(document, base::Token("b1")));
  scoped_refptr<Node> c1 =
      b1->AppendChild(new Element(document, base::Token("c1")));
  scoped_refptr<Node> d1 =
      a2->AppendChild(new Element(document, base::Token("d1")));
  scoped_refptr<Node> d2 =
      a2->AppendChild(new Element(document, base::Token("d2")));

  scoped_refptr<HTMLCollection> collection =
      node->GetElementsByClassName("class");

  // Start with an empty class.
  EXPECT_EQ(0, collection->length());

  // Set the matching class on all elements.
  a1->AsElement()->set_class_name("class");
  a2->AsElement()->set_class_name("class");
  b1->AsElement()->set_class_name("class");
  c1->AsElement()->set_class_name("class");
  d1->AsElement()->set_class_name("class");
  d2->AsElement()->set_class_name("class");

  EXPECT_EQ(6, collection->length());
  EXPECT_EQ(a1, collection->Item(0));
  EXPECT_EQ(b1, collection->Item(1));
  EXPECT_EQ(c1, collection->Item(2));
  EXPECT_EQ(a2, collection->Item(3));
  EXPECT_EQ(d1, collection->Item(4));
  EXPECT_EQ(d2, collection->Item(5));
  EXPECT_EQ(kNullNode, collection->Item(6));
  EXPECT_EQ(a1, collection->Item(0));

  // Modify the class name on some elements.
  a1->AsElement()->set_class_name("other_class");
  a2->AsElement()->set_class_name("other_class");
  b1->AsElement()->set_class_name("other_class");
  d1->AsElement()->set_class_name("other_class");

  EXPECT_EQ(2, collection->length());
  EXPECT_EQ(c1, collection->Item(0));
  EXPECT_EQ(d2, collection->Item(1));
  EXPECT_EQ(kNullNode, collection->Item(2));
  EXPECT_EQ(c1, collection->Item(0));

  // d1 is not matched by the collection so adding the id shouldn't change
  // anything.
  d1->AsElement()->set_id("id");
  EXPECT_EQ(kNullNode, collection->NamedItem("id"));

  // d2 is matched by the collection so adding the id should work.
  d2->AsElement()->set_id("id");
  EXPECT_EQ(d2, collection->NamedItem("id"));

  // Removing a2 should also remove its children from the collection, including
  // d2.
  node->RemoveChild(a2);
  EXPECT_EQ(1, collection->length());
  EXPECT_EQ(c1, collection->Item(0));
  EXPECT_EQ(kNullNode, collection->Item(1));
  EXPECT_EQ(kNullNode, collection->NamedItem("id"));

  c1->AsElement()->set_class_name("other_class");
  EXPECT_EQ(0, collection->length());
  EXPECT_EQ(kNullNode, collection->Item(0));

  // Test elements with multiple classes
  a1->AsElement()->set_class_name("class other_class");
  b1->AsElement()->set_class_name("class yet_another_class");
  c1->AsElement()->set_class_name("other_class yet_another_class");
  EXPECT_EQ(2, collection->length());
  EXPECT_EQ(a1, collection->Item(0));
  EXPECT_EQ(b1, collection->Item(1));
}

template <typename T>
void TestGetElementsByTagName(const scoped_refptr<T>& node) {
  const scoped_refptr<Node> kNullNode{};
  const scoped_refptr<HTMLCollection> kNullCollection{};
  Document* document = node->node_document();

  if (!document) {
    document = node->AsDocument();
    DCHECK(document);
  }

  // Construct a tree:
  // node
  //   a1
  //     b1
  //       c1
  //   a3
  //     d1
  HTMLElementFactory html_element_factory;
  html_element_factory.CreateHTMLElement(document, base::Token("a1"));

  scoped_refptr<Node> a1 = node->AppendChild(
      html_element_factory.CreateHTMLElement(document, base::Token("a1")));

  scoped_refptr<Node> a3 = node->AppendChild(
      html_element_factory.CreateHTMLElement(document, base::Token("a2")));
  scoped_refptr<Node> b1 = a1->AppendChild(
      html_element_factory.CreateHTMLElement(document, base::Token("b1")));
  scoped_refptr<Node> c1 = b1->AppendChild(
      html_element_factory.CreateHTMLElement(document, base::Token("element")));
  scoped_refptr<Node> d1 = a3->AppendChild(
      html_element_factory.CreateHTMLElement(document, base::Token("element")));

  // GetElementsByTagName should return all elements when provided with
  // parameter "*".
  scoped_refptr<HTMLCollection> collection = node->GetElementsByTagName("*");
  EXPECT_EQ(5, collection->length());
  EXPECT_EQ(a1, collection->Item(0));
  EXPECT_EQ(b1, collection->Item(1));
  EXPECT_EQ(c1, collection->Item(2));
  EXPECT_EQ(a3, collection->Item(3));
  EXPECT_EQ(d1, collection->Item(4));
  EXPECT_EQ(kNullNode, collection->Item(5));

  // GetElementsByTagName should only return elements with the specific local
  // name when that is provided.
  collection = node->GetElementsByTagName("element");
  EXPECT_EQ(2, collection->length());
  EXPECT_EQ(c1, collection->Item(0));
  EXPECT_EQ(d1, collection->Item(1));
  EXPECT_EQ(kNullNode, collection->Item(2));

  // a3 is not matched by the collection so adding the id shouldn't change
  // anything.
  a3->AsElement()->set_id("id");
  EXPECT_EQ(kNullNode, collection->NamedItem("id"));

  // d1 is matched by the collection so adding the id should work.
  d1->AsElement()->set_id("id");
  EXPECT_EQ(d1, collection->NamedItem("id"));

  // Add a new node with a matching tag.
  scoped_refptr<Node> a2 = node->InsertBefore(
      html_element_factory.CreateHTMLElement(document, base::Token("element")),
      a3);
  EXPECT_EQ(3, collection->length());
  EXPECT_EQ(c1, collection->Item(0));
  EXPECT_EQ(a2, collection->Item(1));
  EXPECT_EQ(d1, collection->Item(2));
  EXPECT_EQ(kNullNode, collection->Item(3));

  // Removing a3 should also remove its children from the collection.
  node->RemoveChild(a3);
  EXPECT_EQ(2, collection->length());
  EXPECT_EQ(c1, collection->Item(0));
  EXPECT_EQ(a2, collection->Item(1));
  EXPECT_EQ(kNullNode, collection->Item(2));
  EXPECT_EQ(kNullNode, collection->NamedItem("id"));
}

}  // namespace testing
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TESTING_HTML_COLLECTION_TESTING_H_
