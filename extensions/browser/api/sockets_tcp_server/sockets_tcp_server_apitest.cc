// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "extensions/browser/api/sockets_tcp_server/sockets_tcp_server_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

const int kPort = 8888;

class SocketsTcpServerApiTest : public ShellApiTest {};

IN_PROC_BROWSER_TEST_F(SocketsTcpServerApiTest, SocketTCPCreateGood) {
  scoped_refptr<api::SocketsTcpServerCreateFunction> socket_create_function(
      new api::SocketsTcpServerCreateFunction());
  scoped_refptr<const Extension> empty_extension(
      ExtensionBuilder("Test").Build());

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  absl::optional<base::Value> result(
      api_test_utils::RunFunctionAndReturnSingleResult(
          socket_create_function.get(), "[]", browser_context()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());
  absl::optional<int> socket_id = result->GetDict().FindInt("socketId");
  ASSERT_TRUE(socket_id);
  ASSERT_GT(*socket_id, 0);
}

IN_PROC_BROWSER_TEST_F(SocketsTcpServerApiTest, SocketTCPServerExtension) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());
  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadApp("sockets_tcp_server/api"));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("tcp_server:127.0.0.1:%d", kPort));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Flaky. http://crbug.com/561474
IN_PROC_BROWSER_TEST_F(SocketsTcpServerApiTest,
                       DISABLED_SocketTCPServerUnbindOnUnload) {
  std::string path("sockets_tcp_server/unload");
  ResultCatcher catcher;
  const Extension* extension = LoadApp(path);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  UnloadApp(extension);

  ASSERT_TRUE(LoadApp(path)) << message_;
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
