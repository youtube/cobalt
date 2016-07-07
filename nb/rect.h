/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef NB_RECT_H_
#define NB_RECT_H_

namespace nb {

// A simple Rect structure to simplify repetitive definitions of a rectangle
// in various code.
template <typename T>
struct Rect {
  Rect(T left, T top, T width, T height)
      : left(left), top(top), width(width), height(height) {}

  void Set(T left_p, T top_p, T width_p, T height_p) {
    left = left_p;
    top = top_p;
    width = width_p;
    height = height_p;
  }

  T left;
  T top;
  T width;
  T height;
};

}  // namespace nb

#endif  // NB_RECT_H_
