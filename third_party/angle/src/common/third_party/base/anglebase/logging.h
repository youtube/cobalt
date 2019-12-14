//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// logging.h: Compatiblity hacks for importing Chromium's base/numerics.

<<<<<<< HEAD:third_party/angle/src/common/third_party/numerics/base_copy/numerics_logging.h
#ifndef BASE_COPY_LOGGING_H_
#define BASE_COPY_LOGGING_H_
=======
#ifndef ANGLEBASE_LOGGING_H_
#define ANGLEBASE_LOGGING_H_
>>>>>>> 1ba4cc530e9156a73f50daff4affa367dedd5a8a:third_party/angle/src/common/third_party/base/anglebase/logging.h

#include "common/debug.h"

#ifndef DCHECK
#    define DCHECK(X) ASSERT(X)
#endif

#ifndef CHECK
#    define CHECK(X) ASSERT(X)
#endif

// Unfortunately ANGLE relies on ASSERT being an empty statement, which these libs don't respect.
#ifndef NOTREACHED
#    define NOTREACHED() ({ UNREACHABLE(); })
#endif

<<<<<<< HEAD:third_party/angle/src/common/third_party/numerics/base_copy/numerics_logging.h
#endif  // BASE_COPY_LOGGING_H_
=======
#endif  // ANGLEBASE_LOGGING_H_
>>>>>>> 1ba4cc530e9156a73f50daff4affa367dedd5a8a:third_party/angle/src/common/third_party/base/anglebase/logging.h
