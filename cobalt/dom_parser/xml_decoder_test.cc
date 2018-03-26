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

#include "cobalt/dom_parser/xml_decoder.h"

#include "base/callback.h"
#include "cobalt/dom/attr.h"
#include "cobalt/dom/cdata_section.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/text.h"
#include "cobalt/dom/xml_document.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom_parser {

const int kDOMMaxElementDepth = 32;

class MockErrorCallback : public base::Callback<void(const std::string&)> {
 public:
  MOCK_METHOD1(Run, void(const std::string&));
};

class XMLDecoderTest : public ::testing::Test {
 protected:
  XMLDecoderTest();
  ~XMLDecoderTest() override {}

  dom::HTMLElementContext html_element_context_;
  scoped_refptr<dom::XMLDocument> document_;
  base::SourceLocation source_location_;
  MockErrorCallback mock_error_callback_;
  scoped_ptr<XMLDecoder> xml_decoder_;
};

XMLDecoderTest::XMLDecoderTest()
    : document_(new dom::XMLDocument(&html_element_context_)),
      source_location_(base::SourceLocation("[object XMLDecoderTest]", 1, 1)) {}

TEST_F(XMLDecoderTest, ShouldNotAddImpliedTags) {
  const std::string input = "<ELEMENT></ELEMENT>";
  xml_decoder_.reset(new XMLDecoder(
      document_, document_, NULL, kDOMMaxElementDepth, source_location_,
      base::Closure(), base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&mock_error_callback_))));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("ELEMENT", element->local_name());
  EXPECT_FALSE(element->HasChildNodes());
}

TEST_F(XMLDecoderTest, CanParseCDATASection) {
  // Libxml requires that a CDATA must be inside an element.
  const std::string input =
      "<ELEMENT>"
      "<![CDATA["
      "<tag> <!--comment--> <![CDATA[ & &amp; ]]&gt;"
      "]]>"
      "<![CDATA["
      "another CDATA section"
      "]]>"
      "</ELEMENT>";
  xml_decoder_.reset(new XMLDecoder(
      document_, document_, NULL, kDOMMaxElementDepth, source_location_,
      base::Closure(), base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&mock_error_callback_))));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("ELEMENT", element->local_name());

  dom::CDATASection* cdata_section1 = element->first_child()->AsCDATASection();
  ASSERT_TRUE(cdata_section1);
  EXPECT_EQ("<tag> <!--comment--> <![CDATA[ & &amp; ]]&gt;",
            cdata_section1->data());

  dom::CDATASection* cdata_section2 = element->last_child()->AsCDATASection();
  ASSERT_TRUE(cdata_section2);
  EXPECT_EQ("another CDATA section", cdata_section2->data());
}

TEST_F(XMLDecoderTest, CanParseAttributesWithValue) {
  const std::string input = "<ELEMENT a=\"1\" b=\"2\"></ELEMENT>";
  xml_decoder_.reset(new XMLDecoder(
      document_, document_, NULL, kDOMMaxElementDepth, source_location_,
      base::Closure(), base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&mock_error_callback_))));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("ELEMENT", element->local_name());
  EXPECT_TRUE(element->HasAttributes());
  EXPECT_EQ(2, element->attributes()->length());
  ASSERT_NE(nullptr, element->attributes()->GetNamedItem("a"));
  EXPECT_EQ("a", element->attributes()->GetNamedItem("a")->name());
  EXPECT_EQ("1", element->attributes()->GetNamedItem("a")->value());
  ASSERT_NE(nullptr, element->attributes()->GetNamedItem("b"));
  EXPECT_EQ("b", element->attributes()->GetNamedItem("b")->name());
  EXPECT_EQ("2", element->attributes()->GetNamedItem("b")->value());
}

TEST_F(XMLDecoderTest, TagNamesShouldBeCaseSensitive) {
  const std::string input = "<ELEMENT><element></element></ELEMENT>";
  xml_decoder_.reset(new XMLDecoder(
      document_, document_, NULL, kDOMMaxElementDepth, source_location_,
      base::Closure(), base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&mock_error_callback_))));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("ELEMENT", element->local_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("element", element->local_name());
}

TEST_F(XMLDecoderTest, AttributesShouldBeCaseSensitive) {
  const std::string input = "<ELEMENT A=\"1\" a=\"2\"></ELEMENT>";
  xml_decoder_.reset(new XMLDecoder(
      document_, document_, NULL, kDOMMaxElementDepth, source_location_,
      base::Closure(), base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&mock_error_callback_))));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("ELEMENT", element->local_name());
  EXPECT_TRUE(element->HasAttributes());
  EXPECT_EQ(2, element->attributes()->length());
  ASSERT_NE(nullptr, element->attributes()->GetNamedItem("A"));
  EXPECT_EQ("A", element->attributes()->GetNamedItem("A")->name());
  EXPECT_EQ("1", element->attributes()->GetNamedItem("A")->value());
  ASSERT_NE(nullptr, element->attributes()->GetNamedItem("a"));
  EXPECT_EQ("a", element->attributes()->GetNamedItem("a")->name());
  EXPECT_EQ("2", element->attributes()->GetNamedItem("a")->value());
}

TEST_F(XMLDecoderTest, CanDealWithFileAttack) {
  const std::string input =
      "<!DOCTYPE doc [ <!ENTITY ent SYSTEM \"file:///dev/tty\"> ]>"
      "<element>&ent;</element>";
  xml_decoder_.reset(new XMLDecoder(
      document_, document_, NULL, kDOMMaxElementDepth, source_location_,
      base::Closure(), base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&mock_error_callback_))));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("element", element->local_name());
}

TEST_F(XMLDecoderTest, CanDealWithHTMLAttack) {
  const std::string input =
      "<!DOCTYPE doc [ <!ENTITY ent SYSTEM \"file:///dev/tty\"> ]>"
      "<element>&ent;</element>";
  xml_decoder_.reset(new XMLDecoder(
      document_, document_, NULL, kDOMMaxElementDepth, source_location_,
      base::Closure(), base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&mock_error_callback_))));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("element", element->local_name());
}

TEST_F(XMLDecoderTest, CanDealWithAttack) {
  const std::string input =
      "<!DOCTYPE doc [ <!ENTITY % ent SYSTEM \"http://www.google.com/\"> ]>"
      "<element>&ent;</element>";
  xml_decoder_.reset(new XMLDecoder(
      document_, document_, NULL, kDOMMaxElementDepth, source_location_,
      base::Closure(), base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&mock_error_callback_))));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("element", element->local_name());
}

TEST_F(XMLDecoderTest, CanDealWithPEAttack) {
  const std::string input =
      "<!DOCTYPE doc [ <!ENTITY % ent SYSTEM \"file:///dev/tty\"> ]>"
      "<element>hey</element>";
  xml_decoder_.reset(new XMLDecoder(
      document_, document_, NULL, kDOMMaxElementDepth, source_location_,
      base::Closure(), base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&mock_error_callback_))));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("element", element->local_name());
}

TEST_F(XMLDecoderTest, CanDealWithDTDAttack) {
  const std::string input =
      "<!DOCTYPE doc SYSTEM \"file:///dev/tty\">"
      "<element>hey</element>";
  xml_decoder_.reset(new XMLDecoder(
      document_, document_, NULL, kDOMMaxElementDepth, source_location_,
      base::Closure(), base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&mock_error_callback_))));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("element", element->local_name());
}

TEST_F(XMLDecoderTest, CanDealWithLaughsAttack) {
  const std::string input =
      "<!DOCTYPE doc ["
      "<!ENTITY ha \"Ha !\">"
      "<!ENTITY ha2 \"&ha; &ha; &ha; &ha; &ha;\">"
      "<!ENTITY ha3 \"&ha2; &ha2; &ha2; &ha2; &ha2;\">"
      "<!ENTITY ha4 \"&ha3; &ha3; &ha3; &ha3; &ha3;\">"
      "<!ENTITY ha5 \"&ha4; &ha4; &ha4; &ha4; &ha4;\">"
      "<!ENTITY ha6 \"&ha5; &ha5; &ha5; &ha5; &ha5;\">"
      "<!ENTITY ha7 \"&ha6; &ha6; &ha6; &ha6; &ha6;\">"
      "<!ENTITY ha8 \"&ha7; &ha7; &ha7; &ha7; &ha7;\">"
      "<!ENTITY ha9 \"&ha8; &ha8; &ha8; &ha8; &ha8;\">"
      "<!ENTITY ha10 \"&ha9; &ha9; &ha9; &ha9; &ha9;\">"
      "<!ENTITY ha11 \"&ha10; &ha10; &ha10; &ha10; &ha10;\">"
      "<!ENTITY ha12 \"&ha11; &ha11; &ha11; &ha11; &ha11;\">"
      "<!ENTITY ha13 \"&ha12; &ha12; &ha12; &ha12; &ha12;\">"
      "<!ENTITY ha14 \"&ha13; &ha13; &ha13; &ha13; &ha13;\">"
      "<!ENTITY ha15 \"&ha14; &ha14; &ha14; &ha14; &ha14;\">"
      "]>"
      "<element>&ha15;</element>";
  xml_decoder_.reset(new XMLDecoder(
      document_, document_, NULL, kDOMMaxElementDepth, source_location_,
      base::Closure(), base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&mock_error_callback_))));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("element", element->local_name());
}

TEST_F(XMLDecoderTest, CanDealWithXIncludeAttack) {
  const std::string input =
      "<?xml version=\"1.0\"?>"
      "<root xmlns:xi=\"http://www.w3.org/2001/XInclude\">"
      "<child>"
      "<name><xi:include href=\"file:///dev/tty\" parse=\"text\"/></name>"
      "</child>"
      "</root>";
  xml_decoder_.reset(new XMLDecoder(
      document_, document_, NULL, kDOMMaxElementDepth, source_location_,
      base::Closure(), base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&mock_error_callback_))));
  xml_decoder_->DecodeChunk(input.c_str(), input.length());
  xml_decoder_->Finish();

  dom::Element* element = document_->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("root", element->local_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("child", element->local_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("name", element->local_name());

  element = element->first_element_child();
  ASSERT_TRUE(element);
  EXPECT_EQ("xi:include", element->local_name());
}

}  // namespace dom_parser
}  // namespace cobalt
