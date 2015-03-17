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

#include "cobalt/dom/document.h"

#include "cobalt/dom/attr.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_style_element.h"
#include "cobalt/dom/location.h"
#include "cobalt/dom/stats.h"
#include "cobalt/dom/testing/gtest_workarounds.h"
#include "cobalt/dom/testing/html_collection_testing.h"
#include "cobalt/dom/testing/parent_node_testing.h"
#include "cobalt/dom/text.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

//////////////////////////////////////////////////////////////////////////
// Stubs
//////////////////////////////////////////////////////////////////////////

class StubCSSParser : public cssom::CSSParser {
  scoped_refptr<cssom::CSSStyleSheet> ParseStyleSheet(
      const std::string& file_name, const std::string& input) OVERRIDE {
    return cssom::CSSStyleSheet::Create();
  }
  scoped_refptr<cssom::CSSStyleSheet> ParseStyleSheetWithBeginLine(
      const std::string& file_name, const std::string& input,
      int begin_line) OVERRIDE {
    return cssom::CSSStyleSheet::Create();
  }
};

//////////////////////////////////////////////////////////////////////////
// DocumentTest
//////////////////////////////////////////////////////////////////////////

class DocumentTest : public ::testing::Test {
 protected:
  DocumentTest() : html_element_factory_(NULL, new StubCSSParser(), NULL) {}
  ~DocumentTest() OVERRIDE {}

  // testing::Test:
  void SetUp() OVERRIDE;
  void TearDown() OVERRIDE;

  HTMLElementFactory html_element_factory_;
};

void DocumentTest::SetUp() {
  EXPECT_TRUE(Stats::GetInstance()->CheckNoLeaks());
}

void DocumentTest::TearDown() {
  EXPECT_TRUE(Stats::GetInstance()->CheckNoLeaks());
}

//////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////

TEST_F(DocumentTest, Create) {
  scoped_refptr<Document> document = make_scoped_refptr(
      new Document(&html_element_factory_, Document::Options()));
  ASSERT_NE(NULL, document);

  EXPECT_EQ(Node::kDocumentNode, document->node_type());
  EXPECT_EQ("#document", document->node_name());

  GURL url("http://a valid url");
  document = make_scoped_refptr(
      new Document(&html_element_factory_, Document::Options(url)));
  EXPECT_EQ(url.spec(), document->url());
  EXPECT_EQ(url.spec(), document->document_uri());
  EXPECT_EQ(url, document->url_as_gurl());
}

TEST_F(DocumentTest, CreateElement) {
  scoped_refptr<Document> document = make_scoped_refptr(
      new Document(&html_element_factory_, Document::Options()));
  scoped_refptr<Element> element = document->CreateElement();

  EXPECT_EQ(Node::kElementNode, element->node_type());
  EXPECT_EQ("#element", element->node_name());

  // Make sure that the element is not attached to anything.
  EXPECT_EQ(NULL, element->owner_document());
  EXPECT_EQ(NULL, element->parent_node());
  EXPECT_EQ(NULL, element->first_child());
  EXPECT_EQ(NULL, element->last_child());
}

TEST_F(DocumentTest, CreateTextNode) {
  scoped_refptr<Document> document = make_scoped_refptr(
      new Document(&html_element_factory_, Document::Options()));
  scoped_refptr<Text> text = document->CreateTextNode("test_text");

  EXPECT_EQ(Node::kTextNode, text->node_type());
  EXPECT_EQ("#text", text->node_name());
  EXPECT_EQ("test_text", text->text_content());

  // Make sure that the text is not attached to anything.
  EXPECT_EQ(NULL, text->owner_document());
}

TEST_F(DocumentTest, ParentNodeAllExceptChilden) {
  scoped_refptr<Document> root = make_scoped_refptr(
      new Document(&html_element_factory_, Document::Options()));
  testing::TestParentNodeAllExceptChilden(root);
}

TEST_F(DocumentTest, ParentNodeChildren) {
  scoped_refptr<Document> root = make_scoped_refptr(
      new Document(&html_element_factory_, Document::Options()));
  testing::TestParentNodeChildren(root);
}

TEST_F(DocumentTest, GetElementsByClassName) {
  scoped_refptr<Document> root = make_scoped_refptr(
      new Document(&html_element_factory_, Document::Options()));
  testing::TestGetElementsByClassName(root);
}

TEST_F(DocumentTest, GetElementsByTagName) {
  scoped_refptr<Document> root = make_scoped_refptr(
      new Document(&html_element_factory_, Document::Options()));
  testing::TestGetElementsByTagName(root);
}

TEST_F(DocumentTest, GetElementById) {
  scoped_refptr<Document> root = make_scoped_refptr(
      new Document(&html_element_factory_, Document::Options()));

  // Construct a tree:
  // root
  //   a1
  //     b1
  //       c1
  //   a2
  //     d1
  scoped_refptr<Node> a1 = root->AppendChild(Element::Create());
  scoped_refptr<Node> a2 = root->AppendChild(Element::Create());
  scoped_refptr<Node> b1 = a1->AppendChild(Element::Create());
  scoped_refptr<Node> c1 = b1->AppendChild(Element::Create());
  scoped_refptr<Node> d1 = a2->AppendChild(Element::Create());

  EXPECT_EQ(NULL, root->GetElementById("id"));

  d1->AsElement()->set_id("id");
  EXPECT_EQ(d1, root->GetElementById("id"));

  c1->AsElement()->set_id("id");
  EXPECT_EQ(c1, root->GetElementById("id"));

  root->RemoveChild(a1);
  EXPECT_EQ(d1, root->GetElementById("id"));
}

TEST_F(DocumentTest, OwnerDocument) {
  // Construct a tree:
  // document
  //   element1
  //     element2
  scoped_refptr<Document> document = make_scoped_refptr(
      new Document(&html_element_factory_, Document::Options()));
  scoped_refptr<Node> element1 = Element::Create();
  scoped_refptr<Node> element2 = Element::Create();

  EXPECT_EQ(NULL, document->owner_document());
  EXPECT_EQ(NULL, element1->owner_document());

  element1->AppendChild(element2);
  document->AppendChild(element1);
  EXPECT_EQ(NULL, document->owner_document());
  EXPECT_EQ(document, element1->owner_document());
  EXPECT_EQ(document, element2->owner_document());

  document->RemoveChild(element1);
  EXPECT_EQ(NULL, element1->owner_document());
  EXPECT_EQ(NULL, element2->owner_document());
}

TEST_F(DocumentTest, Location) {
  scoped_refptr<Document> document = make_scoped_refptr(
      new Document(&html_element_factory_, Document::Options()));
  EXPECT_NE(scoped_refptr<Location>(), document->location());
}

TEST_F(DocumentTest, StyleSheets) {
  scoped_refptr<Document> document = make_scoped_refptr(
      new Document(&html_element_factory_, Document::Options()));

  scoped_refptr<HTMLElement> element1 =
      html_element_factory_.CreateHTMLElement(HTMLStyleElement::kTagName);
  element1->set_text_content("body { background-color: lightgray }");
  document->AppendChild(element1);

  scoped_refptr<HTMLElement> element2 =
      html_element_factory_.CreateHTMLElement(HTMLStyleElement::kTagName);
  element2->set_text_content("h1 { color: blue }");
  document->AppendChild(element2);

  scoped_refptr<HTMLElement> element3 =
      html_element_factory_.CreateHTMLElement(HTMLStyleElement::kTagName);
  element3->set_text_content("p { color: green }");
  document->AppendChild(element3);

  EXPECT_NE(scoped_refptr<cssom::StyleSheetList>(), document->style_sheets());
  EXPECT_EQ(3, document->style_sheets()->length());
  EXPECT_NE(scoped_refptr<cssom::CSSStyleSheet>(),
            document->style_sheets()->Item(0));
  EXPECT_NE(scoped_refptr<cssom::CSSStyleSheet>(),
            document->style_sheets()->Item(1));
  EXPECT_NE(scoped_refptr<cssom::CSSStyleSheet>(),
            document->style_sheets()->Item(2));
}

}  // namespace dom
}  // namespace cobalt
