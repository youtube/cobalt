// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TEST_DATA_DIRECTORY_H_
#define NET_BASE_TEST_DATA_DIRECTORY_H_

#include "base/file_path.h"

namespace net {

// Returns the FilePath object representing the absolute path in the source
// tree that contains certificates for testing.
FilePath GetTestCertsDirectory();

}  // namespace net

#endif  // NET_BASE_TEST_DATA_DIRECTORY_H_
