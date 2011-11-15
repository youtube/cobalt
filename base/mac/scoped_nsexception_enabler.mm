// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/scoped_nsexception_enabler.h"

#import "base/lazy_instance.h"
#import "base/threading/thread_local.h"

// To make the |g_exceptionsAllowed| declaration readable.
using base::LazyInstance;
using base::LeakyLazyInstanceTraits;
using base::ThreadLocalBoolean;

namespace {

// Whether to allow NSExceptions to be raised on the current thread.
LazyInstance<ThreadLocalBoolean, LeakyLazyInstanceTraits<ThreadLocalBoolean> >
    g_exceptionsAllowed = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace base {
namespace mac {

bool GetNSExceptionsAllowed() {
  return g_exceptionsAllowed.Get().Get();
}

void SetNSExceptionsAllowed(bool allowed) {
  return g_exceptionsAllowed.Get().Set(allowed);
}

ScopedNSExceptionEnabler::ScopedNSExceptionEnabler() {
  was_enabled_ = GetNSExceptionsAllowed();
  SetNSExceptionsAllowed(true);
}

ScopedNSExceptionEnabler::~ScopedNSExceptionEnabler() {
  SetNSExceptionsAllowed(was_enabled_);
}

}  // namespace mac
}  // namespace base
