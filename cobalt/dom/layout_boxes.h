/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef DOM_LAYOUT_BOXES_H_
#define DOM_LAYOUT_BOXES_H_

namespace cobalt {
namespace dom {

// This class gives the DOM an interface to the boxes generated from the element
// during layout. This allows incremental box generation by letting the DOM tree
// invalidate the boxes when an element changes.

class LayoutBoxes {
 public:
  virtual ~LayoutBoxes() {}

  // All classes implementing this interface have a unique Type value.
  enum Type {
    kLayoutLayoutBoxes = 0,
  };

  // Returns the type of layout boxes.
  virtual Type type() const = 0;

  // TODO(***REMOVED***): Add CSSOM view support, for methods such as
  // getBoundingClientRect() and clientWidth/clientHeight().
  // The required functionality wil be implemented in layout.

 protected:
  LayoutBoxes() {}
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_LAYOUT_BOXES_H_
