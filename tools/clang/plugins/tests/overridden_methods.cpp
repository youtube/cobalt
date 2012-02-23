// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "overridden_methods.h"

// Fill in the implementations
void DerivedClass::SomeMethod() {}
void DerivedClass::SomeOtherMethod() {}
void DerivedClass::WebKitModifiedSomething() {}

int main() {
  DerivedClass something;
}
