// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service.h"

#include "base/file_util.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"

namespace net {

// Default values are taken from glibc resolv.h.
DnsConfig::DnsConfig()
  : ndots(1),
    timeout(base::TimeDelta::FromSeconds(5)),
    attempts(2),
    rotate(false),
    edns0(false) {}

DnsConfig::~DnsConfig() {}

bool DnsConfig::EqualsIgnoreHosts(const DnsConfig& d) const {
  return (nameservers == d.nameservers) &&
         (search == d.search) &&
         (ndots == d.ndots) &&
         (timeout == d.timeout) &&
         (attempts == d.attempts) &&
         (rotate == d.rotate) &&
         (edns0 == d.edns0);
}

bool DnsConfig::Equals(const DnsConfig& d) const {
  return EqualsIgnoreHosts(d) && (hosts == d.hosts);
}

DnsConfigService::DnsConfigService()
  : have_config_(false),
    have_hosts_(false) {}

DnsConfigService::~DnsConfigService() {}

void DnsConfigService::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observers_.AddObserver(observer);
  if (have_config_ && have_hosts_) {
    observer->OnConfigChanged(dns_config_);
  }
}

void DnsConfigService::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void DnsConfigService::OnConfigRead(const DnsConfig& config) {
  DCHECK(CalledOnValidThread());
  if (!config.EqualsIgnoreHosts(dns_config_)) {
    DnsConfig copy = config;
    copy.hosts.swap(dns_config_.hosts);
    dns_config_ = copy;
    have_config_ = true;
    if (have_hosts_) {
      FOR_EACH_OBSERVER(Observer, observers_, OnConfigChanged(dns_config_));
    }
  }
}

void DnsConfigService::OnHostsRead(const DnsHosts& hosts) {
  DCHECK(CalledOnValidThread());
  if (hosts != dns_config_.hosts || !have_hosts_) {
    dns_config_.hosts = hosts;
    have_hosts_ = true;
    if (have_config_) {
      FOR_EACH_OBSERVER(Observer, observers_, OnConfigChanged(dns_config_));
    }
  }
}

DnsHostsReader::DnsHostsReader(DnsConfigService* service)
  : service_(service),
    success_(false) {
  DCHECK(service);
}

// Reads the contents of the file at |path| into |str| if the total length is
// less than |size|.
static bool ReadFile(const FilePath& path, int64 size, std::string* str) {
  int64 sz;
  if (!file_util::GetFileSize(path, &sz) || sz > size)
    return false;
  return file_util::ReadFileToString(path, str);
}

void DnsHostsReader::DoRead() {
  success_ = false;
  std::string contents;
  const int64 kMaxHostsSize = 1 << 16;
  if (ReadFile(get_path(), kMaxHostsSize, &contents)) {
    success_ = true;
    ParseHosts(contents, &dns_hosts_);
  }
}

void DnsHostsReader::OnReadFinished() {
  if (success_)
    service_->OnHostsRead(dns_hosts_);
}

DnsHostsReader::~DnsHostsReader() {}

}  // namespace net

