// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service.h"

#include "base/logging.h"
#include "net/base/ip_endpoint.h"

namespace net {

// Default values are taken from glibc resolv.h.
DnsConfig::DnsConfig()
    : append_to_multi_label_name(true),
      ndots(1),
      timeout(base::TimeDelta::FromSeconds(5)),
      attempts(2),
      rotate(false),
      edns0(false) {}

DnsConfig::~DnsConfig() {}

bool DnsConfig::Equals(const DnsConfig& d) const {
  return EqualsIgnoreHosts(d) && (hosts == d.hosts);
}

bool DnsConfig::EqualsIgnoreHosts(const DnsConfig& d) const {
  return (nameservers == d.nameservers) &&
         (search == d.search) &&
         (append_to_multi_label_name == d.append_to_multi_label_name) &&
         (ndots == d.ndots) &&
         (timeout == d.timeout) &&
         (attempts == d.attempts) &&
         (rotate == d.rotate) &&
         (edns0 == d.edns0);
}

void DnsConfig::CopyIgnoreHosts(const DnsConfig& d) {
  nameservers = d.nameservers;
  search = d.search;
  append_to_multi_label_name = d.append_to_multi_label_name;
  ndots = d.ndots;
  timeout = d.timeout;
  attempts = d.attempts;
  rotate = d.rotate;
  edns0 = d.edns0;
}


DnsConfigService::DnsConfigService()
    : have_config_(false),
      have_hosts_(false),
      need_update_(false) {}

DnsConfigService::~DnsConfigService() {}

void DnsConfigService::InvalidateConfig() {
  DCHECK(CalledOnValidThread());
  if (!have_config_)
    return;
  have_config_ = false;
  StartTimer();
}

void DnsConfigService::InvalidateHosts() {
  DCHECK(CalledOnValidThread());
  if (!have_hosts_)
    return;
  have_hosts_ = false;
  StartTimer();
}

void DnsConfigService::OnConfigRead(const DnsConfig& config) {
  DCHECK(CalledOnValidThread());
  DCHECK(config.IsValid());

  if (!config.EqualsIgnoreHosts(dns_config_)) {
    dns_config_.CopyIgnoreHosts(config);
    need_update_ = true;
  }

  have_config_ = true;
  if (have_hosts_)
    OnCompleteConfig();
}

void DnsConfigService::OnHostsRead(const DnsHosts& hosts) {
  DCHECK(CalledOnValidThread());

  if (hosts != dns_config_.hosts) {
    dns_config_.hosts = hosts;
    need_update_ = true;
  }

  have_hosts_ = true;
  if (have_config_)
    OnCompleteConfig();
}

void DnsConfigService::StartTimer() {
  DCHECK(CalledOnValidThread());
  timer_.Stop();
  const base::TimeDelta kTimeout = base::TimeDelta::FromMilliseconds(100);
  timer_.Start(FROM_HERE,
               kTimeout,
               this,
               &DnsConfigService::OnTimeout);
}

void DnsConfigService::OnTimeout() {
  DCHECK(CalledOnValidThread());
  // Indicate that even if there is no change in On*Read, we will need to
  // update the receiver when the config becomes complete.
  need_update_ = true;
  callback_.Run(DnsConfig());
}

void DnsConfigService::OnCompleteConfig() {
  timer_.Stop();
  if (need_update_) {
    need_update_ = false;
    callback_.Run(dns_config_);
  }
}

}  // namespace net

