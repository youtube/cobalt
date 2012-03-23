// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_HOSTS_H_
#define NET_DNS_DNS_HOSTS_H_
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "net/base/address_family.h"
#include "net/base/net_export.h"
#include "net/base/net_util.h"  // can't forward-declare IPAddressNumber
#include "net/dns/serial_worker.h"

namespace net {

// Parsed results of a Hosts file.
//
// Although Hosts files map IP address to a list of domain names, for name
// resolution the desired mapping direction is: domain name to IP address.
// When parsing Hosts, we apply the "first hit" rule as Windows and glibc do.
// With a Hosts file of:
// 300.300.300.300 localhost # bad ip
// 127.0.0.1 localhost
// 10.0.0.1 localhost
// The expected resolution of localhost is 127.0.0.1.
typedef std::pair<std::string, AddressFamily> DnsHostsKey;
typedef std::map<DnsHostsKey, IPAddressNumber> DnsHosts;

// Parses |contents| (as read from /etc/hosts or equivalent) and stores results
// in |dns_hosts|. Invalid lines are ignored (as in most implementations).
void NET_EXPORT_PRIVATE ParseHosts(const std::string& contents,
                                   DnsHosts* dns_hosts);

// A SerialWorker that reads a HOSTS file and runs Callback.
// Call WorkNow() to indicate file needs to be re-read.
// Call Cancel() to disable the callback.
class NET_EXPORT_PRIVATE DnsHostsReader
    : NON_EXPORTED_BASE(public SerialWorker) {
 public:
  typedef base::Callback<void(const DnsHosts& hosts)> CallbackType;

  DnsHostsReader(const FilePath& path, const CallbackType& callback);

  const FilePath& path() const { return path_; }

 protected:
  explicit DnsHostsReader(const FilePath& path);
  virtual ~DnsHostsReader();

  virtual void DoWork() OVERRIDE;
  virtual void OnWorkFinished() OVERRIDE;

  FilePath path_;
  CallbackType callback_;
  // Written in DoWork, read in OnWorkFinished, no locking necessary.
  DnsHosts dns_hosts_;
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(DnsHostsReader);
};


}  // namespace net

#endif  // NET_DNS_DNS_HOSTS_H_

