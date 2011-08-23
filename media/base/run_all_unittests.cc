// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite.h"
#include "media/base/media.h"

int main(int argc, char** argv) {
  base::TestSuite suite(argc, argv);

  media::InitializeMediaLibraryForTesting();

  return suite.Run();
}
