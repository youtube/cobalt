// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_STATUS_OBSERVER_H_
#define REMOTING_HOST_HOST_STATUS_OBSERVER_H_

#include <string>

#include "remoting/base/buildflags.h"

#if BUILDFLAG(REMOTING_MULTI_PROCESS)
#include "remoting/host/mojom/remoting_host.mojom.h"
#endif

namespace remoting {

namespace protocol {
struct TransportRoute;
}  // namespace protocol

// Interface for host status observer. All methods are invoked on the
// network thread. Observers must not tear-down ChromotingHost state
// on receipt of these callbacks; they are purely informational.
// HostStatusObserver conditionally inherits from mojom::HostStatusObserver.
// When multi-process is supported, it inherits from Mojo, so its methods
// override Mojo's virtual methods (meaning they must have `override` and must
// NOT have `virtual` to satisfy the Chromium style linter).
// When multi-process is not supported (e.g. Mac), it does not inherit from
// Mojo, so its methods must be `virtual` (and NOT `override`) to allow C++
// subclasses to override them.
// The macros below handle this conditional signature to avoid style violations
// and code duplication.
#if BUILDFLAG(REMOTING_MULTI_PROCESS)
#define STATUS_OBSERVER_VIRTUAL
#define STATUS_OBSERVER_OVERRIDE override
class HostStatusObserver : public mojom::HostStatusObserver {
#else
#define STATUS_OBSERVER_VIRTUAL virtual
#define STATUS_OBSERVER_OVERRIDE
class HostStatusObserver {
#endif
 public:
  HostStatusObserver() = default;
  STATUS_OBSERVER_VIRTUAL ~HostStatusObserver()
      STATUS_OBSERVER_OVERRIDE = default;

  // Called when an unauthorized user attempts to connect to the host.
  STATUS_OBSERVER_VIRTUAL void OnClientAccessDenied(
      const std::string& signaling_id) STATUS_OBSERVER_OVERRIDE {}

  // Called when a new client is authenticated.
  STATUS_OBSERVER_VIRTUAL void OnClientAuthenticated(
      const std::string& signaling_id) STATUS_OBSERVER_OVERRIDE {}

  // Called when all channels for an authenticated client are connected.
  STATUS_OBSERVER_VIRTUAL void OnClientConnected(
      const std::string& signaling_id) STATUS_OBSERVER_OVERRIDE {}

  // Called when an authenticated client is disconnected.
  STATUS_OBSERVER_VIRTUAL void OnClientDisconnected(
      const std::string& signaling_id) STATUS_OBSERVER_OVERRIDE {}

  // Called on notification of a route change event, when a channel is
  // connected.
  STATUS_OBSERVER_VIRTUAL void OnClientRouteChange(
      const std::string& signaling_id,
      const std::string& channel_name,
      const protocol::TransportRoute& route) STATUS_OBSERVER_OVERRIDE {}

  // Called when the host is started for an account.
  STATUS_OBSERVER_VIRTUAL void OnHostStarted(const std::string& owner_email)
      STATUS_OBSERVER_OVERRIDE {}

  // Called when the host shuts down.
  STATUS_OBSERVER_VIRTUAL void OnHostShutdown() STATUS_OBSERVER_OVERRIDE {}
};

#undef STATUS_OBSERVER_VIRTUAL
#undef STATUS_OBSERVER_OVERRIDE

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_STATUS_OBSERVER_H_
