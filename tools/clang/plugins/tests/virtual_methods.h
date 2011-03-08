// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIRTUAL_METHODS_H_
#define VIRTUAL_METHODS_H_

// Should warn about virtual method usage.
class VirtualMethodsInHeaders {
 public:
  // Don't complain about these.
  virtual void MethodIsAbstract() = 0;
  virtual void MethodHasNoArguments();
  virtual void MethodHasEmptyDefaultImpl() {}

  // But complain about this:
  virtual bool ComplainAboutThis() { return true; }
};

// Complain on missing 'virtual' keyword in overrides.
class WarnOnMissingVirtual : public VirtualMethodsInHeaders {
 public:
  void MethodHasNoArguments();
};

#endif  // VIRTUAL_METHODS_H_
