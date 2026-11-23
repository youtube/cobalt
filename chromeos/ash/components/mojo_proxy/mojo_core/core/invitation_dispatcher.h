// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_INVITATION_DISPATCHER_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_INVITATION_DISPATCHER_H_

#include <stdint.h>

#include <string_view>

#include "base/containers/flat_map.h"
#include "base/synchronization/lock.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/dispatcher.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/ports/port_ref.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/system_impl_export.h"

namespace mojo_legacy {
namespace core {

class MOJO_LEGACY_SYSTEM_IMPL_EXPORT InvitationDispatcher : public Dispatcher {
 public:
  InvitationDispatcher();

  InvitationDispatcher(const InvitationDispatcher&) = delete;
  InvitationDispatcher& operator=(const InvitationDispatcher&) = delete;

  // Dispatcher:
  Type GetType() const override;
  MojoResult Close() override;
  MojoResult AttachMessagePipe(std::string_view name,
                               ports::PortRef remote_peer_port) override;
  MojoResult ExtractMessagePipe(std::string_view name,
                                MojoHandle* message_pipe_handle) override;

  using PortMapping = base::flat_map<std::string, ports::PortRef>;
  PortMapping TakeAttachedPorts();

 private:
  ~InvitationDispatcher() override;

  base::Lock lock_;
  bool is_closed_ = false;
  PortMapping attached_ports_;
};

}  // namespace core
}  // namespace mojo_legacy

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_INVITATION_DISPATCHER_H_
