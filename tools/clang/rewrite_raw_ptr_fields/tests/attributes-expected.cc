// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"

class SomeClass;

// Based on Chromium's //base/thread_annotations.h
#define GUARDED_BY(lock) __attribute__((guarded_by(lock)))

class MyClass {
  MyClass(SomeClass& s) : ref_field(s), lock(0) {}
  // Expected rewrite: raw_ptr<SomeClass> ptr_field GUARDED_BY(lock);
  raw_ptr<SomeClass> ptr_field GUARDED_BY(lock);
  // Expected rewrite: const raw_ref<SomeClass> ref_field GUARDED_BY(lock);
  const raw_ref<SomeClass> ref_field GUARDED_BY(lock);
  int lock;
};
