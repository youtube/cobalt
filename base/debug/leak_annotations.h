// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_LEAK_ANNOTATIONS_H_
#define BASE_DEBUG_LEAK_ANNOTATIONS_H_

#include "build/build_config.h"

// By default, Leak Sanitizer and Address Sanitizer is expected
// to exist together. However, this is not true for all
// platforms (e.g. PS4).
// HAS_LEAK_SANTIZIER=0 explicitly removes the Leak Sanitizer from code.
#ifdef ADDRESS_SANITIZER
#ifndef HAS_LEAK_SANITIZER
// Default is that Leak Sanitizer exists whenever Address Sanitizer does.
#define HAS_LEAK_SANITIZER 1
#endif
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_NACL) && \
    defined(USE_HEAPCHECKER)

#include "third_party/tcmalloc/chromium/src/gperftools/heap-checker.h"

// Annotate a program scope as having memory leaks. Tcmalloc's heap leak
// checker will ignore them. Note that these annotations may mask real bugs
// and should not be used in the production code.
#define ANNOTATE_SCOPED_MEMORY_LEAK \
    HeapLeakChecker::Disabler heap_leak_checker_disabler

// Annotate an object pointer as referencing a leaky object. This object and all
// the heap objects referenced by it will be ignored by the heap checker.
//
// X should be referencing an active allocated object. If it is not, the
// annotation will be ignored.
// No object should be annotated with ANNOTATE_SCOPED_MEMORY_LEAK twice.
// Once an object is annotated with ANNOTATE_SCOPED_MEMORY_LEAK, it cannot be
// deleted.
#define ANNOTATE_LEAKING_OBJECT_PTR(X) \
    HeapLeakChecker::IgnoreObject(X)

#elif defined(ADDRESS_SANITIZER) && HAS_LEAK_SANITIZER
#include <sanitizer/lsan_interface.h>

class ScopedLeakSanitizerDisabler {
 public:
  ScopedLeakSanitizerDisabler() { __lsan_disable(); }
  ~ScopedLeakSanitizerDisabler() { __lsan_enable(); }
 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedLeakSanitizerDisabler);
};

#define ANNOTATE_SCOPED_MEMORY_LEAK \
    ScopedLeakSanitizerDisabler leak_sanitizer_disabler; static_cast<void>(0)

#define ANNOTATE_LEAKING_OBJECT_PTR(X) __lsan_ignore_object(X);

#else

// If tcmalloc is not used, the annotations should be no-ops.
#define ANNOTATE_SCOPED_MEMORY_LEAK ((void)0)
#define ANNOTATE_LEAKING_OBJECT_PTR(X) ((void)0)

#endif

#endif  // BASE_DEBUG_LEAK_ANNOTATIONS_H_
