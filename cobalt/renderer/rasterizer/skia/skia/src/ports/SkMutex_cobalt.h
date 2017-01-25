/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKMUTEX_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKMUTEX_COBALT_H_

#include "base/synchronization/lock.h"

// In Cobalt, like Windows, SkBaseMutex and SkMutex are the same thing,
// because there is no design allowing us to get rid of static initializers.
// We preserve the same inheritance pattern as other Skia platforms
// so that we can forward-declare cleanly.
class SkBaseMutex {
 public:
  enum Leaky { kLeaky };
  SkBaseMutex() : lock_(*new (buffer_) base::Lock), leaky_(false) {}
  // Objects constructed using this ctor will not destroy its underlying Lock
  // during destructing.  This is because many SkBaseMutex are statically
  // constructed along with other static objects in Skia.  As the order of
  // static initialization is undeterminated it is possible that a mutex is
  // destroyed before the object that relies on it has been destroyed, which
  // leads to crash on exit.
  explicit SkBaseMutex(Leaky)
      : lock_(*new (buffer_) base::Lock), leaky_(true) {}
  ~SkBaseMutex() {
    if (!leaky_) {
      lock_.~Lock();
    }
  }

  void acquire() { lock_.Acquire(); }

  void release() { lock_.Release(); }

  void assertHeld() { lock_.AssertAcquired(); }

 private:
  SkBaseMutex(const SkBaseMutex&);
  SkBaseMutex& operator=(const SkBaseMutex&);

  uint8_t buffer_[sizeof(base::Lock)];
  base::Lock& lock_;
  bool leaky_;
};

class SkMutex : public SkBaseMutex {};

// On Cobalt we provide no means of POD initializing a mutex.
// As a result, it is illegal to SK_DECLARE_STATIC_MUTEX in a function.
#define SK_DECLARE_STATIC_MUTEX(name) \
  namespace {}                        \
  static SkBaseMutex name(SkBaseMutex::kLeaky)  // NOLINT(build/namespaces)

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKMUTEX_COBALT_H_
