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

#include <limits>

namespace cobalt {
namespace dom {

DOMRectList::DOMRectList() {}
DOMRectList::~DOMRectList() {}

unsigned int DOMRectList::length() const {
  CHECK_LE(dom_rects_.size(), std::numeric_limits<unsigned int>::max());
  return static_cast<unsigned int>(dom_rects_.size());
}

scoped_refptr<DOMRect> DOMRectList::Item(unsigned int index) const {
  return index < dom_rects_.size() ? dom_rects_[index] : NULL;
}

void DOMRectList::AppendDOMRect(const scoped_refptr<DOMRect>& dom_rect) {
  dom_rects_.push_back(dom_rect);
}

}  // namespace dom
}  // namespace cobalt
