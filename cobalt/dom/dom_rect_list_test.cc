// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/dom_rect_list.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

TEST(DOMRectListTest, ItemAccess) {
  scoped_refptr<DOMRectList> dom_rect_list = new DOMRectList();
  ASSERT_EQ(0, dom_rect_list->length());
  ASSERT_FALSE(dom_rect_list->Item(0));

  scoped_refptr<DOMRect> dom_rect(new DOMRect());
  dom_rect_list->AppendDOMRect(dom_rect);
  ASSERT_EQ(1, dom_rect_list->length());
  ASSERT_EQ(dom_rect, dom_rect_list->Item(0));
  ASSERT_FALSE(dom_rect_list->Item(1));
}

TEST(DOMRectListTest, AppendDOMRectShouldTakeDOMRect) {
  scoped_refptr<DOMRectList> dom_rect_list = new DOMRectList();
  scoped_refptr<DOMRect> dom_rect = new DOMRect();
  dom_rect_list->AppendDOMRect(dom_rect);

  ASSERT_EQ(1, dom_rect_list->length());
  ASSERT_EQ(dom_rect, dom_rect_list->Item(0));
  ASSERT_FALSE(dom_rect_list->Item(1).get());
}

}  // namespace dom
}  // namespace cobalt
