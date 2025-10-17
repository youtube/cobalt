/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifdef WEBRTC_NETWORK_TESTER_TEST_ENABLED

#include <string>

#include "api/test/rtc_error_matchers.h"
#include "rtc_base/random.h"
#include "rtc_tools/network_tester/test_controller.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/wait_until.h"

namespace webrtc {

TEST(NetworkTesterTest, ServerClient) {
  // Use a unique port rather than a hard-coded one to avoid collision when
  // running the test in parallel in stress runs. Skipping all reserved ports.
  const int MIN_PORT = 49152;
  const int MAX_PORT = 65535;
  int port = webrtc::Random(webrtc::TimeMicros()).Rand(MIN_PORT, MAX_PORT);

  webrtc::AutoThread main_thread;

  TestController client(
      0, 0, webrtc::test::ResourcePath("network_tester/client_config", "dat"),
      webrtc::test::OutputPath() + "client_packet_log.dat");
  TestController server(
      port, port,
      webrtc::test::ResourcePath("network_tester/server_config", "dat"),
      webrtc::test::OutputPath() + "server_packet_log.dat");
  client.SendConnectTo("127.0.0.1", port);
  EXPECT_THAT(
      WaitUntil([&] { return server.IsTestDone() && client.IsTestDone(); },
                ::testing::IsTrue()),
      IsRtcOk());
}

}  // namespace webrtc

#endif  // WEBRTC_NETWORK_TESTER_TEST_ENABLED
