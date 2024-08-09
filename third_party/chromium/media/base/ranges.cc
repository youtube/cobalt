// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/base/ranges.h"

namespace media_m96 {

template<>
void Ranges<base::TimeDelta>::DCheckLT(const base::TimeDelta& lhs,
                                       const base::TimeDelta& rhs) const {
  DCHECK(lhs < rhs) << lhs.ToInternalValue() << " < " << rhs.ToInternalValue();
}

}  // namespace media_m96
