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

#ifndef COBALT_DOM_DOM_RECT_LIST_H_
#define COBALT_DOM_DOM_RECT_LIST_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/dom_rect.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The DOMRectList interface represents an ordered collection of DOMRect
// objects.
//   https://www.w3.org/TR/2014/CR-geometry-1-20141125/#DOMRectList
class DOMRectList : public script::Wrappable {
 public:
  DOMRectList();
  virtual ~DOMRectList();

  // Web API: DOMRectList
  //
  // The length attribute must return the total number of DOMRect objects
  // associated with the object.
  unsigned int length() const;

  // The item(index) method, when invoked, must return a null value when index
  // is greater than or equal to the number of DOMRect objects associated with
  // the DOMRectList. Otherwise, the DOMRect object at index must be returned.
  // Indices are zero-based.
  scoped_refptr<DOMRect> Item(unsigned int index) const;

  // Custom, not in any spec.
  //

  void AppendDOMRect(const scoped_refptr<DOMRect>& dom_rect);

  DEFINE_WRAPPABLE_TYPE(DOMRectList);

 private:
  typedef std::vector<scoped_refptr<DOMRect> > DOMRects;

  DOMRects dom_rects_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOM_RECT_LIST_H_
