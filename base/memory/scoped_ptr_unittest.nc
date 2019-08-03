// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"

namespace {

class Parent {
};

class Child : public Parent {
};

class RefCountedClass : public base::RefCountedThreadSafe<RefCountedClass> {
};

}  // namespace

#if defined(NCTEST_NO_PASSAS_DOWNCAST)  // [r"invalid conversion from"]

scoped_ptr<Child> DowncastUsingPassAs(scoped_ptr<Parent> object) {
  return object.PassAs<Child>();
}

#elif defined(NCTEST_NO_REF_COUNTED_SCOPED_PTR)  // [r"creating array with negative size"]

// scoped_ptr<> should not work for ref-counted objects.
void WontCompile() {
  scoped_ptr<RefCountedClass> x;
}

#endif
