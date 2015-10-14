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

#ifndef LAYOUT_LAYOUT_BOXES_H_
#define LAYOUT_LAYOUT_BOXES_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/layout_boxes.h"

namespace cobalt {
namespace layout {

class ContainerBox;

class LayoutBoxes : public dom::LayoutBoxes {
 public:
  explicit LayoutBoxes(const scoped_refptr<ContainerBox>& container_box);
  ~LayoutBoxes() OVERRIDE;

  // From: dom:LayoutBoxes
  //
  Type type() const OVERRIDE;

  // Other

  const scoped_refptr<ContainerBox>& container_box() { return container_box_; }

 private:
  const scoped_refptr<ContainerBox> container_box_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_LAYOUT_BOXES_H_
