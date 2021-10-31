// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MATH_SIZE_CONVERSIONS_H_
#define COBALT_MATH_SIZE_CONVERSIONS_H_

#include "cobalt/math/size.h"
#include "cobalt/math/size_f.h"

namespace cobalt {
namespace math {

// Returns a Size with each component from the input SizeF floored.
Size ToFlooredSize(const SizeF& size);

// Returns a Size with each component from the input SizeF ceiled.
Size ToCeiledSize(const SizeF& size);

// Returns a Size with each component from the input SizeF rounded.
Size ToRoundedSize(const SizeF& size);

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_SIZE_CONVERSIONS_H_
