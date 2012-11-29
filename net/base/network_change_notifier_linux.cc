// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_linux.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/threading/thread.h"
#include "net/base/address_tracker_linux.h"
#include "net/dns/dns_config_service.h"

namespace net {

class NetworkChangeNotifierLinux::Thread : public base::Thread {
 public:
  Thread();
  virtual ~Thread();

  // Plumbing for NetworkChangeNotifier::GetCurrentConnectionType.
  // Safe to call from any thread.
  NetworkChangeNotifier::ConnectionType GetCurrentConnectionType() {
    return address_tracker_.GetCurrentConnectionType();
  }

  const internal::AddressTrackerLinux* address_tracker() const {
    return &address_tracker_;
  }

 protected:
  // base::Thread
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

 private:
  scoped_ptr<DnsConfigService> dns_config_service_;
  // Used to detect online/offline state and IP address changes.
  internal::AddressTrackerLinux address_tracker_;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};

NetworkChangeNotifierLinux::Thread::Thread()
    : base::Thread("NetworkChangeNotifier"),
      address_tracker_(
          base::Bind(&NetworkChangeNotifier::
                     NotifyObserversOfIPAddressChange),
          base::Bind(&NetworkChangeNotifier::
                     NotifyObserversOfConnectionTypeChange)) {
}

NetworkChangeNotifierLinux::Thread::~Thread() {
  DCHECK(!Thread::IsRunning());
}

void NetworkChangeNotifierLinux::Thread::Init() {
  address_tracker_.Init();
  dns_config_service_ = DnsConfigService::CreateSystemService();
  dns_config_service_->WatchConfig(
      base::Bind(&NetworkChangeNotifier::SetDnsConfig));
}

void NetworkChangeNotifierLinux::Thread::CleanUp() {
  dns_config_service_.reset();
}

NetworkChangeNotifierLinux* NetworkChangeNotifierLinux::Create() {
  return new NetworkChangeNotifierLinux();
}

NetworkChangeNotifierLinux::NetworkChangeNotifierLinux()
    : notifier_thread_(new Thread()) {
  // We create this notifier thread because the notification implementation
  // needs a MessageLoopForIO, and there's no guarantee that
  // MessageLoop::current() meets that criterion.
  base::Thread::Options thread_options(MessageLoop::TYPE_IO, 0);
  notifier_thread_->StartWithOptions(thread_options);
}

NetworkChangeNotifierLinux::~NetworkChangeNotifierLinux() {
  // Stopping from here allows us to sanity- check that the notifier
  // thread shut down properly.
  notifier_thread_->Stop();
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierLinux::GetCurrentConnectionType() const {
  return notifier_thread_->GetCurrentConnectionType();
}

const internal::AddressTrackerLinux*
NetworkChangeNotifierLinux::GetAddressTrackerInternal() const {
  return notifier_thread_->address_tracker();
}

}  // namespace net
