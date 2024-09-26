// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_ASH_H_
#define REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_ASH_H_

#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "base/values.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/mojom/remote_support.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {
class NativeMessageHost;
}

namespace remoting {

class ChromotingHostContext;
class PolicyWatcher;
struct ChromeOsEnterpriseParams;

// This class wraps the It2MeNativeMessageHost instance used on other platforms
// and provides a way to interact with it using Mojo IPC.  This instance
// receives messages from the wrapped NMH by observing it via the Client
// interface.  The messages and events are then converted from JSON to Mojo and
// forwarded to the It2MeNativeMessageHostLacros instance over IPC.
// All interactions with it must occur on the sequence it was created on.
class It2MeNativeMessageHostAsh : public extensions::NativeMessageHost::Client {
 public:
  It2MeNativeMessageHostAsh();
  It2MeNativeMessageHostAsh(const It2MeNativeMessageHostAsh&) = delete;
  It2MeNativeMessageHostAsh& operator=(const It2MeNativeMessageHostAsh&) =
      delete;
  ~It2MeNativeMessageHostAsh() override;

  // Creates a new NMH instance, creates a new SupportHostObserver remote and
  // returns the pending_remote.  Start() must be called before the first call
  // to |Connect()|.
  mojo::PendingReceiver<mojom::SupportHostObserver> Start(
      std::unique_ptr<ChromotingHostContext> context,
      std::unique_ptr<PolicyWatcher> policy_watcher);

  // extensions::NativeMessageHost::Client.
  void PostMessageFromNativeHost(const std::string& message) override;
  void CloseChannel(const std::string& error_message) override;

  // Begins the connection process using the wrapped native message host.
  // |connected_callback| is run after the connection process has completed.
  void Connect(
      mojom::SupportSessionParamsPtr params,
      const absl::optional<ChromeOsEnterpriseParams>& enterprise_params,
      base::OnceClosure connected_callback,
      base::OnceClosure disconnected_callback);
  // Disconnects an active session if one exists.
  void Disconnect();

 private:
  // Handlers for messages received from the wrapped native message host.
  void HandleConnectResponse();
  void HandleDisconnectResponse();
  void HandleHostStateChangeMessage(base::Value::Dict message);
  void HandleNatPolicyChangedMessage(base::Value::Dict message);
  void HandlePolicyErrorMessage(base::Value::Dict message);
  void HandleErrorMessage(base::Value::Dict message);

  SEQUENCE_CHECKER(sequence_checker_);

  base::OnceClosure connected_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::OnceClosure disconnected_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<extensions::NativeMessageHost> native_message_host_
      GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Remote<mojom::SupportHostObserver> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_ASH_H_
