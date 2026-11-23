// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_PORTS_NODE_DELEGATE_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_PORTS_NODE_DELEGATE_H_

#include <stddef.h>

#include "chromeos/ash/components/mojo_proxy/mojo_core/core/ports/event.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/ports/name.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/ports/port_ref.h"

namespace mojo_legacy {
namespace core {
namespace ports {

class NodeDelegate {
 public:
  virtual ~NodeDelegate() = default;

  // Forward an event (possibly asynchronously) to the specified node.
  virtual void ForwardEvent(const NodeName& node, ScopedEvent event) = 0;

  // Broadcast an event to all nodes.
  virtual void BroadcastEvent(ScopedEvent event) = 0;

  // Indicates that the port's status has changed recently. Use Node::GetStatus
  // to query the latest status of the port. Note, this event could be spurious
  // if another thread is simultaneously modifying the status of the port.
  virtual void PortStatusChanged(const PortRef& port_ref) = 0;
};

}  // namespace ports
}  // namespace core
}  // namespace mojo_legacy

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_PORTS_NODE_DELEGATE_H_
