// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/firewall_hole/firewall_hole.h"

#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"

namespace chromeos {

namespace {

const char* PortTypeToString(FirewallHole::PortType type) {
  switch (type) {
    case FirewallHole::PortType::kTcp:
      return "TCP";
    case FirewallHole::PortType::kUdp:
      return "UDP";
  }
  NOTREACHED();
  return nullptr;
}

void PortReleased(FirewallHole::PortType type,
                  uint16_t port,
                  const std::string& interface,
                  base::ScopedFD lifeline_fd,
                  bool success) {
  if (!success) {
    LOG(WARNING) << "Failed to release firewall hole for "
                 << PortTypeToString(type) << " port " << port << " on "
                 << interface << ".";
  }
}

}  // namespace

// static
void FirewallHole::Open(PortType type,
                        uint16_t port,
                        const std::string& interface,
                        OpenCallback callback) {
  int lifeline[2] = {-1, -1};
  if (pipe2(lifeline, O_CLOEXEC) < 0) {
    PLOG(ERROR) << "Failed to create a lifeline pipe";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    return;
  }
  base::ScopedFD lifeline_local(lifeline[1]);
  base::ScopedFD lifeline_remote(lifeline[0]);

  base::OnceCallback<void(bool)> access_granted_closure =
      base::BindOnce(&FirewallHole::PortAccessGranted, type, port, interface,
                     std::move(lifeline_local), std::move(callback));

  chromeos::PermissionBrokerClient* client =
      chromeos::PermissionBrokerClient::Get();
  DCHECK(client) << "Could not get permission broker client.";

  switch (type) {
    case PortType::kTcp:
      client->RequestTcpPortAccess(port, interface, lifeline_remote.get(),
                                   std::move(access_granted_closure));
      return;
    case PortType::kUdp:
      client->RequestUdpPortAccess(port, interface, lifeline_remote.get(),
                                   std::move(access_granted_closure));
      return;
  }
}

FirewallHole::~FirewallHole() {
  base::OnceCallback<void(bool)> port_released_closure = base::BindOnce(
      &PortReleased, type_, port_, interface_, std::move(lifeline_fd_));

  chromeos::PermissionBrokerClient* client =
      chromeos::PermissionBrokerClient::Get();
  // See crbug.com/1429368: sometimes the shutdown routine for
  // PermissionBrokerClient takes place before firewall holes are released.
  if (!client) {
    LOG(ERROR) << "Could not get permission broker client.";
    return;
  }

  switch (type_) {
    case PortType::kTcp:
      client->ReleaseTcpPort(port_, interface_,
                             std::move(port_released_closure));
      return;
    case PortType::kUdp:
      client->ReleaseUdpPort(port_, interface_,
                             std::move(port_released_closure));
      return;
  }
}

void FirewallHole::PortAccessGranted(PortType type,
                                     uint16_t port,
                                     const std::string& interface,
                                     base::ScopedFD lifeline_fd,
                                     FirewallHole::OpenCallback callback,
                                     bool success) {
  if (success) {
    std::move(callback).Run(base::WrapUnique(
        new FirewallHole(type, port, interface, std::move(lifeline_fd))));
  } else {
    std::move(callback).Run(nullptr);
  }
}

FirewallHole::FirewallHole(PortType type,
                           uint16_t port,
                           const std::string& interface,
                           base::ScopedFD lifeline_fd)
    : type_(type),
      port_(port),
      interface_(interface),
      lifeline_fd_(std::move(lifeline_fd)) {}

}  // namespace chromeos
