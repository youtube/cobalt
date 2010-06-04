// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_gssapi_posix.h"

#include "base/native_library.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(HttpAuthGSSAPIPOSIXTest, GSSAPIStartup) {
  // TODO(ahendrickson): Manipulate the libraries and paths to test each of the
  // libraries we expect, and also whether or not they have the interface
  // functions we want.
  base::NativeLibrary lib = GSSAPISharedLibrary::LoadSharedObject();
  bool has_library = (lib != NULL);
  if (has_library) {
    base::UnloadNativeLibrary(lib);
  }
  GSSAPILibrary* gssapi = GSSAPILibrary::GetDefault();
  EXPECT_EQ(has_library, gssapi->Init());
}

}  // namespace net
