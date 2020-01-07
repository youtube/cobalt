//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ImmutableString: Wrapper for static or pool allocated char arrays, that are guaranteed to be
// valid and unchanged for the duration of the compilation.
//

#include "compiler/translator/ImmutableString.h"

namespace sh
{
CONSTEXPR ImmutableString kEmptyImmutableString("");
}  // namespace sh
