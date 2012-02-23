// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "base/metrics/histogram.h"
#include "crypto/nss_util.h"
#include "net/base/net_test_suite.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/spdy/spdy_session.h"

using net::internal::ClientSocketPoolBaseHelper;
using net::SpdySession;

int main(int argc, char** argv) {
  // Record histograms, so we can get histograms data in tests.
  base::StatisticsRecorder recorder;
  NetTestSuite test_suite(argc, argv);
  ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(false);
  SpdySession::set_enable_ping_based_connection_checking(false);

#if defined(OS_WIN)
  // We want to be sure to init NSPR on the main thread.
  crypto::EnsureNSPRInit();
#endif

  return test_suite.Run();
}
