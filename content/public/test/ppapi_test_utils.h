// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_PPAPI_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_PPAPI_TEST_UTILS_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/udp_socket.mojom.h"

namespace base {
class CommandLine;
}

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

// This file specifies utility functions used in Pepper testing in
// browser_tests and content_browsertests.
namespace ppapi {

// Registers the PPAPI test plugin to application/x-ppapi-tests. Returns true
// on success, and false otherwise.
[[nodiscard]] bool RegisterTestPlugin(base::CommandLine* command_line);

// Registers the PPAPI test plugin with some some extra parameters. Returns true
// on success and false otherwise.
[[nodiscard]] bool RegisterTestPluginWithExtraParameters(
    base::CommandLine* command_line,
    const base::FilePath::StringType& extra_registration_parameters);

// Registers the Blink test plugin to application/x-blink-test-plugin.
[[nodiscard]] bool RegisterBlinkTestPlugin(base::CommandLine* command_line);

using CreateUDPSocketCallback = base::RepeatingCallback<void(
    network::mojom::NetworkContext* network_context,
    mojo::PendingReceiver<network::mojom::UDPSocket> socket_receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> socket_listener)>;

// Sets a NetworkContext to be used by the Pepper TCP classes for testing.
// Passed in NetworkContext must remain valid until the method is called again
// with a nullptr, to clear the callback.
void SetPepperTCPNetworkContextForTesting(
    network::mojom::NetworkContext* network_context);

// Sets callback to be invoked when creating a UDPSocket for use by pepper.
// Passed in callback must remain valid until the method is called again with
// a nullptr, to clear the callback.
void SetPepperUDPSocketCallackForTesting(
    const CreateUDPSocketCallback* create_udp_socket_callback);

}  // namespace ppapi

#endif  // CONTENT_PUBLIC_TEST_PPAPI_TEST_UTILS_H_
