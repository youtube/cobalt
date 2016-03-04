// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/object_watcher_shell.h"
#include "base/test/main_hook.h"
#include "build/build_config.h"
#include "crypto/nss_util.h"
#include "net/base/net_test_suite.h"

#if !__LB_ENABLE_NATIVE_HTTP_STACK__
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/ssl_server_socket.h"
#include "net/spdy/spdy_session.h"
#endif

#if defined(OS_ANDROID)  || defined(__LB_ANDROID__)
#include "base/android/jni_android.h"
#include "net/android/net_jni_registrar.h"
#endif

#if !__LB_ENABLE_NATIVE_HTTP_STACK__
using net::internal::ClientSocketPoolBaseHelper;
using net::SpdySession;
#endif

int test_main(int argc, char** argv) {
  MainHook hook(test_main, argc, argv);

#if !defined(OS_STARBOARD)
  scoped_ptr<base::ObjectWatchMultiplexer> watcher(
      new base::ObjectWatchMultiplexer());
#endif

  // Record histograms, so we can get histograms data in tests.
  base::StatisticsRecorder::Initialize();

#if defined(OS_ANDROID) || defined(__LB_ANDROID__)
  // Register JNI bindings for android. Doing it early as the test suite setup
  // may initiate a call to Java.
  net::android::RegisterJni(base::android::AttachCurrentThread());
#endif

  NetTestSuite test_suite(argc, argv);
#if !__LB_ENABLE_NATIVE_HTTP_STACK__
  ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(false);
#endif
#if defined(OS_WIN)
  // We want to be sure to init NSPR on the main thread.
  crypto::EnsureNSPRInit();
#endif

#if !__LB_ENABLE_NATIVE_HTTP_STACK__
  // Enable support for SSL server sockets, which must be done while
  // single-threaded.
  net::EnableSSLServerSockets();
#endif

  return test_suite.Run();
}

#if !defined(OS_STARBOARD)
int main(int argc, char** argv) {
  return test_main(argc, argv);
}
#else
#include "starboard/event.h"
#include "starboard/system.h"
void SbEventHandle(const SbEvent* event) {
  if (event->type == kSbEventTypeStart) {
    SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
    SbSystemRequestStop(test_main(data->argument_count, data->argument_values));
  }
}
#endif
