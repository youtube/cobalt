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

#include "cobalt/dom/dom_token_list.h"

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/html_element_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class DOMTokenListTest : public ::testing::Test {
 protected:
  DOMTokenListTest();
  ~DOMTokenListTest() override;

  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
};

DOMTokenListTest::DOMTokenListTest() {
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  document_ = new Document(&html_element_context_);
}

DOMTokenListTest::~DOMTokenListTest() {
  document_ = NULL;
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
}

TEST_F(DOMTokenListTest, DOMTokenListShouldBeAlive) {
  scoped_refptr<Element> element =
      new Element(document_, base::Token("element"));
  scoped_refptr<DOMTokenList> dom_token_list =
      new DOMTokenList(element, "class");
  EXPECT_EQ(0, dom_token_list->length());

  element->SetAttribute("class", "a b c");
  ASSERT_EQ(3, dom_token_list->length());
  EXPECT_EQ("a", dom_token_list->Item(0).value());
  EXPECT_EQ("b", dom_token_list->Item(1).value());
  EXPECT_EQ("c", dom_token_list->Item(2).value());
}

TEST_F(DOMTokenListTest, DOMTokenListContains) {
  scoped_refptr<Element> element =
      new Element(document_, base::Token("element"));
  scoped_refptr<DOMTokenList> dom_token_list =
      new DOMTokenList(element, "class");
  element->SetAttribute("class", "a b c");
  EXPECT_TRUE(dom_token_list->Contains("a"));
  EXPECT_TRUE(dom_token_list->Contains("b"));
  EXPECT_TRUE(dom_token_list->Contains("c"));
  EXPECT_FALSE(dom_token_list->Contains("d"));
}

TEST_F(DOMTokenListTest, DOMTokenListAdd) {
  scoped_refptr<Element> element =
      new Element(document_, base::Token("element"));
  scoped_refptr<DOMTokenList> dom_token_list =
      new DOMTokenList(element, "class");
  element->SetAttribute("class", "a b c");
  std::vector<std::string> vs;
  vs.push_back("d");
  dom_token_list->Add(vs);
  EXPECT_TRUE(dom_token_list->Contains("a"));
  EXPECT_TRUE(dom_token_list->Contains("b"));
  EXPECT_TRUE(dom_token_list->Contains("c"));
  EXPECT_TRUE(dom_token_list->Contains("d"));
  EXPECT_EQ("a b c d", element->GetAttribute("class").value());
}

TEST_F(DOMTokenListTest, DOMTokenListRemove) {
  scoped_refptr<Element> element =
      new Element(document_, base::Token("element"));
  scoped_refptr<DOMTokenList> dom_token_list =
      new DOMTokenList(element, "class");
  element->SetAttribute("class", "a b c");
  std::vector<std::string> vs;
  vs.push_back("c");
  dom_token_list->Remove(vs);
  EXPECT_TRUE(dom_token_list->Contains("a"));
  EXPECT_TRUE(dom_token_list->Contains("b"));
  EXPECT_FALSE(dom_token_list->Contains("c"));
  EXPECT_EQ("a b", element->GetAttribute("class").value());
}

TEST_F(DOMTokenListTest, DOMTokenListAnonymousStringifier) {
  scoped_refptr<Element> element =
      new Element(document_, base::Token("element"));
  scoped_refptr<DOMTokenList> dom_token_list =
      new DOMTokenList(element, "class");
  std::vector<std::string> vs;
  vs.push_back("a");
  vs.push_back("b");
  vs.push_back("c");
  dom_token_list->Add(vs);
  EXPECT_EQ("a b c", dom_token_list->AnonymousStringifier());
}

}  // namespace dom
}  // namespace cobalt
