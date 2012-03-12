// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_TEST_SERVER_H_
#define NET_TEST_TEST_SERVER_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "net/test/remote_test_server.h"
#else
#include "net/test/local_test_server.h"
#endif

namespace net {

#if defined(OS_ANDROID)
typedef RemoteTestServer TestServer;
#else
typedef LocalTestServer TestServer;
#endif

}  // namespace net

#endif  // NET_TEST_TEST_SERVER_H_

