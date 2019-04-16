// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/math/size.h"

#include "base/strings/stringprintf.h"

namespace cobalt {
namespace math {

template class SizeBase<Size, int>;

std::string Size::ToString() const {
  return base::StringPrintf("%dx%d", width(), height());
}

}  // namespace math
}  // namespace cobalt
