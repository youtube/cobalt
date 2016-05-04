// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MATH_SIZE_BASE_H_
#define COBALT_MATH_SIZE_BASE_H_

namespace cobalt {
namespace math {

// A size has width and height values.
template <typename Class, typename Type>
class SizeBase {
 public:
  Type width() const { return width_; }
  Type height() const { return height_; }

  void SetSize(Type width, Type height) {
    set_width(width);
    set_height(height);
  }

  void Enlarge(Type width, Type height) {
    set_width(width_ + width);
    set_height(height_ + height);
  }

  void set_width(Type width) { width_ = width; }
  void set_height(Type height) { height_ = height; }

  void SetToMin(const Class& other) {
    width_ = width_ <= other.width_ ? width_ : other.width_;
    height_ = height_ <= other.height_ ? height_ : other.height_;
  }

  void SetToMax(const Class& other) {
    width_ = width_ >= other.width_ ? width_ : other.width_;
    height_ = height_ >= other.height_ ? height_ : other.height_;
  }

  bool IsEmpty() const { return (width_ == Type(0)) || (height_ == Type(0)); }

 protected:
  SizeBase() {}
  SizeBase(Type width, Type height) : width_(width), height_(height) {}

  // Destructor is intentionally made non virtual and protected.
  // Do not make this public.
  ~SizeBase() {}

 private:
  Type width_;
  Type height_;
};

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_SIZE_BASE_H_
