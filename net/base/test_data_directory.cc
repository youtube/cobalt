// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_data_directory.h"

#include "base/base_paths.h"
#include "base/path_service.h"

namespace net {

FilePath GetTestCertsDirectory() {
  FilePath certs_dir;
  PathService::Get(base::DIR_TEST_DATA, &certs_dir);
  return certs_dir.Append(FILE_PATH_LITERAL("net/data/ssl/certificates"));
}

FilePath GetWebSocketTestDataDirectory() {
  FilePath data_dir(FILE_PATH_LITERAL("net/data/websocket"));
  return data_dir;
}

}  // namespace net
