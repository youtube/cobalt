// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/nacl/ppapi_test_lib/test_interface.h"
#include "native_client/src/shared/platform/nacl_check.h"

namespace {

// This will crash a PPP_Messaging function.
void CrashViaCheckFailure() {
  printf("--- CrashViaCheckFailure\n");
  CHECK(false);
}

}  // namespace

void SetupTests() {
  RegisterTest("CrashViaCheckFailure", CrashViaCheckFailure);
}

void SetupPluginInterfaces() {
  // none
}
