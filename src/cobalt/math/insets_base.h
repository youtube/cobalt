// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MATH_INSETS_BASE_H_
#define COBALT_MATH_INSETS_BASE_H_

namespace cobalt {
namespace math {

// An insets represents the borders of a container (the space the container must
// leave at each of its edges).
template <typename Class, typename Type>
class InsetsBase {
 public:
  Type left() const { return left_; }
  void set_left(Type left) { left_ = left; }

  Type top() const { return top_; }
  void set_top(Type top) { top_ = top; }

  Type right() const { return right_; }
  void set_right(Type right) { right_ = right; }

  Type bottom() const { return bottom_; }
  void set_bottom(Type bottom) { bottom_ = bottom; }

  void SetInsets(Type left, Type top, Type right, Type bottom) {
    left_ = left;
    top_ = top;
    right_ = right;
    bottom_ = bottom;
  }

  bool zero() const {
    return left_ == Type(0) && top_ == Type(0) && right_ == Type(0) &&
           bottom_ == Type(0);
  }

  bool operator==(const Class& insets) const {
    return left_ == insets.left_ && top_ == insets.top_ &&
           right_ == insets.right_ && bottom_ == insets.bottom_;
  }

  bool operator!=(const Class& insets) const { return !(*this == insets); }

  void operator+=(const Class& insets) {
    left_ += insets.left_;
    top_ += insets.top_;
    right_ += insets.right_;
    bottom_ += insets.bottom_;
  }

  Class operator-() const { return Class(-left_, -top_, -right_, -bottom_); }

 protected:
  InsetsBase() {}
  InsetsBase(Type left, Type top, Type right, Type bottom)
      : left_(left), top_(top), right_(right), bottom_(bottom) {}

  // Destructor is intentionally made non virtual and protected.
  // Do not make this public.
  ~InsetsBase() {}

 private:
  Type left_;
  Type top_;
  Type right_;
  Type bottom_;
};

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_INSETS_BASE_H_
