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

#include "cobalt/dom/document.h"

#include <memory>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/dom/attr.h"
#include "cobalt/dom/comment.h"
#include "cobalt/dom/dom_implementation.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/html_head_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/html_style_element.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/dom/location.h"
#include "cobalt/dom/mouse_event.h"
#include "cobalt/dom/node_list.h"
#include "cobalt/dom/testing/fake_document.h"
#include "cobalt/dom/testing/html_collection_testing.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/dom/text.h"
#include "cobalt/dom/ui_event.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "cobalt/web/custom_event.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/message_event.h"
#include "cobalt/web/testing/gtest_workarounds.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
namespace {

using script::testing::MockExceptionState;
using ::testing::_;
using ::testing::SaveArg;
using ::testing::StrictMock;

//////////////////////////////////////////////////////////////////////////
// DocumentTest
//////////////////////////////////////////////////////////////////////////

class DocumentTest : public ::testing::Test {
 protected:
  DocumentTest();
  ~DocumentTest() override;

  const scoped_refptr<Document>& document() {
    return window_->window()->document();
  }

  std::unique_ptr<testing::StubWindow> window_;
  std::unique_ptr<HTMLElementContext> html_element_context_;
  std::unique_ptr<css_parser::Parser> css_parser_;
  std::unique_ptr<DomStatTracker> dom_stat_tracker_;
};

DocumentTest::DocumentTest()
    : window_(new testing::StubWindow),
      css_parser_(css_parser::Parser::Create()),
      dom_stat_tracker_(new DomStatTracker("DocumentTest")) {
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  window_->InitializeWindow();
  html_element_context_.reset(new HTMLElementContext(
      window_->web_context()->environment_settings(), NULL, NULL,
      css_parser_.get(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, dom_stat_tracker_.get(), "",
      base::kApplicationStateStarted, NULL, NULL));
}

DocumentTest::~DocumentTest() {
  window_.reset();
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
}

//////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////

TEST_F(DocumentTest, Create) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());
  ASSERT_TRUE(document);

  EXPECT_EQ(Node::kDocumentNode, document->node_type());
  EXPECT_EQ("#document", document->node_name());

  GURL url("http://a valid url");
  document = new testing::FakeDocument(html_element_context_.get(),
                                       Document::Options(url));
  EXPECT_EQ(url.spec(), document->url());
  EXPECT_EQ(url.spec(), document->document_uri());
  EXPECT_EQ(url, document->location()->url());
}

TEST_F(DocumentTest, MainDocumentIsNotXMLDocument) {
  EXPECT_FALSE(document()->IsXMLDocument());
}

TEST_F(DocumentTest, NewDocumentIsNotXMLDocument) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());
  EXPECT_FALSE(document->IsXMLDocument());
}

TEST_F(DocumentTest, MainDocumentHasBrowsingContext) {
  EXPECT_TRUE(document()->HasBrowsingContext());
}

TEST_F(DocumentTest, NewDocumentHasNoBrowsingContext) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());
  EXPECT_FALSE(document->HasBrowsingContext());
}

TEST_F(DocumentTest, DocumentElement) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());
  EXPECT_EQ(NULL, document->document_element().get());

  scoped_refptr<Text> text = new Text(document, "test_text");
  scoped_refptr<Element> element =
      new Element(document, base_token::Token("element"));
  document->AppendChild(text);
  document->AppendChild(element);
  EXPECT_EQ(element, document->document_element());
}

TEST_F(DocumentTest, CreateElement) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());
  scoped_refptr<Element> element = document->CreateElement("element");

  EXPECT_EQ(Node::kElementNode, element->node_type());
  EXPECT_EQ("ELEMENT", element->node_name());

  EXPECT_EQ(document, element->node_document());
  EXPECT_EQ(NULL, element->parent_node());
  EXPECT_EQ(NULL, element->first_child());
  EXPECT_EQ(NULL, element->last_child());

  element = document->CreateElement("ELEMENT");
  EXPECT_EQ("ELEMENT", element->node_name());
}

TEST_F(DocumentTest, CreateTextNode) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());
  scoped_refptr<Text> text = document->CreateTextNode("test_text");

  EXPECT_EQ(Node::kTextNode, text->node_type());
  EXPECT_EQ("#text", text->node_name());
  EXPECT_EQ("test_text", text->data());
  EXPECT_EQ(document, text->node_document());
}

TEST_F(DocumentTest, CreateComment) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());
  scoped_refptr<Comment> comment = document->CreateComment("test_comment");

  EXPECT_EQ(Node::kCommentNode, comment->node_type());
  EXPECT_EQ("#comment", comment->node_name());
  EXPECT_EQ("test_comment", comment->data());
  EXPECT_EQ(document, comment->node_document());
}

TEST_F(DocumentTest, CreateEventEvent) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<script::ScriptException> exception;
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());

  // Create an Event, the name is case insensitive.
  scoped_refptr<web::Event> event =
      document->CreateEvent("EvEnT", &exception_state);
  EXPECT_TRUE(event);
  EXPECT_FALSE(event->initialized_flag());

  event = document->CreateEvent("EvEnTs", &exception_state);
  EXPECT_TRUE(event);
  EXPECT_FALSE(event->initialized_flag());
  EXPECT_FALSE(dynamic_cast<UIEvent*>(event.get()));
  EXPECT_FALSE(dynamic_cast<KeyboardEvent*>(event.get()));
  EXPECT_FALSE(dynamic_cast<web::MessageEvent*>(event.get()));
  EXPECT_FALSE(dynamic_cast<MouseEvent*>(event.get()));

  event = document->CreateEvent("HtMlEvEnTs", &exception_state);
  EXPECT_TRUE(event);
  EXPECT_FALSE(event->initialized_flag());
}

TEST_F(DocumentTest, CreateEventCustomEvent) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<script::ScriptException> exception;
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());

  // Create an Event, the name is case insensitive.
  scoped_refptr<web::Event> event =
      document->CreateEvent("CuStOmEvEnT", &exception_state);
  EXPECT_TRUE(event);
  EXPECT_FALSE(event->initialized_flag());
  EXPECT_TRUE(base::polymorphic_downcast<web::CustomEvent*>(event.get()));
}

TEST_F(DocumentTest, CreateEventUIEvent) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<script::ScriptException> exception;
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());

  // Create an Event, the name is case insensitive.
  scoped_refptr<web::Event> event =
      document->CreateEvent("UiEvEnT", &exception_state);
  EXPECT_TRUE(event);
  EXPECT_FALSE(event->initialized_flag());
  EXPECT_TRUE(base::polymorphic_downcast<UIEvent*>(event.get()));

  event = document->CreateEvent("UiEvEnTs", &exception_state);
  EXPECT_TRUE(event);
  EXPECT_FALSE(event->initialized_flag());
  EXPECT_TRUE(base::polymorphic_downcast<UIEvent*>(event.get()));
}

TEST_F(DocumentTest, CreateEventKeyboardEvent) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<script::ScriptException> exception;
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());

  // Create an Event, the name is case insensitive.
  scoped_refptr<web::Event> event =
      document->CreateEvent("KeYbOaRdEvEnT", &exception_state);
  EXPECT_TRUE(event);
  EXPECT_FALSE(event->initialized_flag());
  EXPECT_TRUE(base::polymorphic_downcast<KeyboardEvent*>(event.get()));

  event = document->CreateEvent("KeYeVeNtS", &exception_state);
  EXPECT_TRUE(event);
  EXPECT_FALSE(event->initialized_flag());
  EXPECT_TRUE(base::polymorphic_downcast<KeyboardEvent*>(event.get()));
}

TEST_F(DocumentTest, CreateEventMessageEvent) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<script::ScriptException> exception;
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());

  // Create an Event, the name is case insensitive.
  scoped_refptr<web::Event> event =
      document->CreateEvent("MeSsAgEeVeNt", &exception_state);
  EXPECT_TRUE(event);
  EXPECT_FALSE(event->initialized_flag());
  EXPECT_TRUE(base::polymorphic_downcast<web::MessageEvent*>(event.get()));
}

TEST_F(DocumentTest, CreateEventMouseEvent) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<script::ScriptException> exception;
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());

  // Create an Event, the name is case insensitive.
  scoped_refptr<web::Event> event =
      document->CreateEvent("MoUsEeVeNt", &exception_state);
  EXPECT_TRUE(event);
  EXPECT_FALSE(event->initialized_flag());
  EXPECT_TRUE(base::polymorphic_downcast<MouseEvent*>(event.get()));

  event = document->CreateEvent("MoUsEeVeNtS", &exception_state);
  EXPECT_TRUE(event);
  EXPECT_FALSE(event->initialized_flag());
  EXPECT_TRUE(base::polymorphic_downcast<MouseEvent*>(event.get()));
}

TEST_F(DocumentTest, CreateEventEventNotSupported) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<script::ScriptException> exception;
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());

  EXPECT_CALL(exception_state, SetException(_))
      .WillOnce(SaveArg<0>(&exception));
  scoped_refptr<web::Event> event =
      document->CreateEvent("Event Not Supported", &exception_state);

  EXPECT_FALSE(event);
  ASSERT_TRUE(exception);
  EXPECT_EQ(
      web::DOMException::kNotSupportedErr,
      base::polymorphic_downcast<web::DOMException*>(exception.get())->code());
}

TEST_F(DocumentTest, GetElementsByClassName) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());
  testing::TestGetElementsByClassName(document);
}

TEST_F(DocumentTest, GetElementsByTagName) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());
  testing::TestGetElementsByTagName(document);
}

TEST_F(DocumentTest, GetElementById) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());

  // Construct a tree:
  // document
  //   a1
  //     b1
  //       c1
  //   a2
  //     d1
  scoped_refptr<Node> a1 =
      document->AppendChild(new Element(document, base_token::Token("a1")));
  scoped_refptr<Node> a2 =
      document->AppendChild(new Element(document, base_token::Token("a2")));
  scoped_refptr<Node> b1 =
      a1->AppendChild(new Element(document, base_token::Token("b1")));
  scoped_refptr<Node> c1 =
      b1->AppendChild(new Element(document, base_token::Token("c1")));
  scoped_refptr<Node> d1 =
      a2->AppendChild(new Element(document, base_token::Token("d1")));

  EXPECT_EQ(NULL, document->GetElementById("id").get());

  d1->AsElement()->set_id("id");
  EXPECT_EQ(d1, document->GetElementById("id"));

  c1->AsElement()->set_id("id");
  EXPECT_EQ(c1, document->GetElementById("id"));

  document->RemoveChild(a1);
  EXPECT_EQ(d1, document->GetElementById("id"));
}

TEST_F(DocumentTest, Implementation) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());
  EXPECT_TRUE(document->implementation());
}

TEST_F(DocumentTest, Location) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());
  EXPECT_TRUE(document->location());
}

TEST_F(DocumentTest, StyleSheets) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());

  scoped_refptr<HTMLElement> element1 =
      html_element_context_->html_element_factory()->CreateHTMLElement(
          document, base_token::Token(HTMLStyleElement::kTagName));
  element1->set_text_content(std::string("body { background-color: #D3D3D3 }"));
  document->AppendChild(element1);

  scoped_refptr<HTMLElement> element2 =
      html_element_context_->html_element_factory()->CreateHTMLElement(
          document, base_token::Token(HTMLStyleElement::kTagName));
  element2->set_text_content(std::string("h1 { color: #00F }"));
  document->AppendChild(element2);

  scoped_refptr<HTMLElement> element3 =
      html_element_context_->html_element_factory()->CreateHTMLElement(
          document, base_token::Token(HTMLStyleElement::kTagName));
  element3->set_text_content(std::string("p { color: #008000 }"));
  document->AppendChild(element3);

  EXPECT_TRUE(document->style_sheets());
  EXPECT_EQ(3, document->style_sheets()->length());
  EXPECT_TRUE(document->style_sheets()->Item(0));
  EXPECT_TRUE(document->style_sheets()->Item(1));
  EXPECT_TRUE(document->style_sheets()->Item(2));

  // Each style sheet should represent the one from the corresponding style
  // element.
  EXPECT_EQ(document->style_sheets()->Item(0),
            element1->AsHTMLStyleElement()->sheet());
  EXPECT_EQ(document->style_sheets()->Item(1),
            element2->AsHTMLStyleElement()->sheet());
  EXPECT_EQ(document->style_sheets()->Item(2),
            element3->AsHTMLStyleElement()->sheet());

  // Each style sheet should be unique.
  EXPECT_NE(document->style_sheets()->Item(0),
            document->style_sheets()->Item(1));
  EXPECT_NE(document->style_sheets()->Item(0),
            document->style_sheets()->Item(2));
  EXPECT_NE(document->style_sheets()->Item(1),
            document->style_sheets()->Item(2));
}

TEST_F(DocumentTest, StyleSheetsAddedToFront) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());

  scoped_refptr<HTMLElement> element1 =
      html_element_context_->html_element_factory()->CreateHTMLElement(
          document, base_token::Token(HTMLStyleElement::kTagName));
  element1->set_text_content(std::string("body { background-color: #D3D3D3 }"));
  document->AppendChild(element1);

  scoped_refptr<HTMLElement> element2 =
      html_element_context_->html_element_factory()->CreateHTMLElement(
          document, base_token::Token(HTMLStyleElement::kTagName));
  element2->set_text_content(std::string("h1 { color: #00F }"));
  document->InsertBefore(element2, element1);

  scoped_refptr<HTMLElement> element3 =
      html_element_context_->html_element_factory()->CreateHTMLElement(
          document, base_token::Token(HTMLStyleElement::kTagName));
  element3->set_text_content(std::string("p { color: #008000 }"));
  document->InsertBefore(element3, element2);

  EXPECT_TRUE(document->style_sheets());
  EXPECT_EQ(3, document->style_sheets()->length());
  EXPECT_TRUE(document->style_sheets()->Item(0));
  EXPECT_TRUE(document->style_sheets()->Item(1));
  EXPECT_TRUE(document->style_sheets()->Item(2));

  // Each style sheet should represent the one from the corresponding style
  // element.
  EXPECT_EQ(document->style_sheets()->Item(0),
            element3->AsHTMLStyleElement()->sheet());
  EXPECT_EQ(document->style_sheets()->Item(1),
            element2->AsHTMLStyleElement()->sheet());
  EXPECT_EQ(document->style_sheets()->Item(2),
            element1->AsHTMLStyleElement()->sheet());

  // Each style sheet should be unique.
  EXPECT_NE(document->style_sheets()->Item(0),
            document->style_sheets()->Item(1));
  EXPECT_NE(document->style_sheets()->Item(0),
            document->style_sheets()->Item(2));
  EXPECT_NE(document->style_sheets()->Item(1),
            document->style_sheets()->Item(2));
}

TEST_F(DocumentTest, HtmlElement) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());
  EXPECT_FALSE(document->html());

  scoped_refptr<Node> div =
      document->AppendChild(document->CreateElement("div"));
  EXPECT_FALSE(document->html());

  document->RemoveChild(div);
  scoped_refptr<Node> html =
      document->AppendChild(document->CreateElement("html"));
  EXPECT_EQ(html, document->html());
}

TEST_F(DocumentTest, HeadElement) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());
  EXPECT_FALSE(document->head());

  scoped_refptr<Node> html =
      document->AppendChild(document->CreateElement("html"));
  EXPECT_FALSE(document->head());

  scoped_refptr<Node> div = html->AppendChild(document->CreateElement("div"));
  EXPECT_FALSE(document->head());

  scoped_refptr<Node> head1 =
      html->AppendChild(document->CreateElement("head"));
  scoped_refptr<Node> head2 =
      html->AppendChild(document->CreateElement("head"));
  EXPECT_EQ(head1, document->head());
}

TEST_F(DocumentTest, BodyElement) {
  scoped_refptr<Document> document =
      new testing::FakeDocument(html_element_context_.get());
  EXPECT_FALSE(document->body());

  scoped_refptr<Node> html =
      document->AppendChild(document->CreateElement("html"));
  EXPECT_FALSE(document->body());

  scoped_refptr<Node> div = html->AppendChild(document->CreateElement("div"));
  EXPECT_FALSE(document->body());

  scoped_refptr<Node> body1 =
      html->AppendChild(document->CreateElement("body"));
  scoped_refptr<Node> body2 =
      html->AppendChild(document->CreateElement("body"));
  EXPECT_EQ(body1, document->body());
}

}  // namespace
}  // namespace dom
}  // namespace cobalt
