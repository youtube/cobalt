// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/named_node_map.h"

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/attr.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/html_element_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class NamedNodeMapTest : public ::testing::Test {
 protected:
  NamedNodeMapTest();
  ~NamedNodeMapTest() override;

  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
  scoped_refptr<Element> element_;
};

NamedNodeMapTest::NamedNodeMapTest() {
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  document_ = new Document(&html_element_context_);
  element_ = new Element(document_, base::Token("element"));
}

NamedNodeMapTest::~NamedNodeMapTest() {
  element_ = NULL;
  document_ = NULL;
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
}

TEST_F(NamedNodeMapTest, EmptyNamedNodeMap) {
  scoped_refptr<NamedNodeMap> named_node_map = new NamedNodeMap(element_);
  EXPECT_EQ(0, named_node_map->length());
  EXPECT_EQ(scoped_refptr<Attr>(NULL), named_node_map->Item(0));
  EXPECT_EQ(scoped_refptr<Attr>(NULL), named_node_map->GetNamedItem("name"));
}

TEST_F(NamedNodeMapTest, ShouldBeAlive) {
  scoped_refptr<NamedNodeMap> named_node_map = new NamedNodeMap(element_);
  EXPECT_EQ(0, named_node_map->length());

  element_->SetAttribute("name", "value");
  ASSERT_EQ(1, named_node_map->length());
  EXPECT_EQ("name", named_node_map->Item(0)->name());
  EXPECT_EQ("value", named_node_map->Item(0)->value());
  EXPECT_EQ("name", named_node_map->GetNamedItem("name")->name());
  EXPECT_EQ("value", named_node_map->GetNamedItem("name")->value());
  EXPECT_EQ(scoped_refptr<Attr>(NULL), named_node_map->Item(1));
}

TEST_F(NamedNodeMapTest, CanSetNamedItem) {
  scoped_refptr<NamedNodeMap> named_node_map = new NamedNodeMap(element_);
  EXPECT_EQ(0, named_node_map->length());

  named_node_map->SetNamedItem(new Attr("name", "value", named_node_map));
  ASSERT_EQ(1, named_node_map->length());
  EXPECT_EQ("name", named_node_map->Item(0)->name());
  EXPECT_EQ("value", named_node_map->Item(0)->value());
  EXPECT_EQ("name", named_node_map->GetNamedItem("name")->name());
  EXPECT_EQ("value", named_node_map->GetNamedItem("name")->value());
  EXPECT_EQ(scoped_refptr<Attr>(NULL), named_node_map->Item(1));
}

TEST_F(NamedNodeMapTest, CanRemoveNamedItem) {
  scoped_refptr<NamedNodeMap> named_node_map = new NamedNodeMap(element_);
  element_->SetAttribute("name", "value");
  ASSERT_EQ(1, named_node_map->length());

  named_node_map->RemoveNamedItem("name");
  EXPECT_EQ(0, named_node_map->length());
}

}  // namespace dom
}  // namespace cobalt
