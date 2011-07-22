// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_test_util.h"

#include "base/message_loop.h"

namespace net {

TestPrng::TestPrng(const std::deque<int>& numbers) : numbers_(numbers) {
}

TestPrng::~TestPrng() {
}

int TestPrng::GetNext(int min, int max) {
  DCHECK(!numbers_.empty());
  int rv = numbers_.front();
  numbers_.pop_front();
  DCHECK(rv >= min && rv <= max);
  return rv;
}

bool ConvertStringsToIPAddressList(
    const char* const ip_strings[], size_t size, IPAddressList* address_list) {
  DCHECK(address_list);
  IPAddressList ip_addresses;
  for (size_t i = 0; i < size; ++i) {
    IPAddressNumber ip;
    if (!ParseIPLiteralToNumber(ip_strings[i], &ip))
      return false;
    ip_addresses.push_back(ip);
  }
  address_list->swap(ip_addresses);
  return true;
}

bool CreateDnsAddress(
    const char* ip_string, uint16 port, IPEndPoint* endpoint) {
  DCHECK(endpoint);
  IPAddressNumber ip_address;
  if (!ParseIPLiteralToNumber(ip_string, &ip_address))
    return false;
  *endpoint = IPEndPoint(ip_address, port);
  return true;
}

}  // namespace net
