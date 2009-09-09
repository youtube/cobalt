// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DNS_UTIL_H_
#define NET_BASE_DNS_UTIL_H_

#include <string>

namespace net {

// DNSDomainFromDot - convert a domain string to DNS format. From DJB's
// public domain DNS library.
//
//   dotted: a string in dotted form: "www.google.com"
//   out: a result in DNS form: "\x03www\x06google\x03com\x00"
bool DNSDomainFromDot(const std::string& dotted, std::string* out);

// Returns true iff the given character is in the set of valid DNS label
// characters as given in RFC 3490, 4.1, 3(a)
bool IsSTD3ASCIIValidCharacter(char c);

}  // namespace net

#endif  // NET_BASE_DNS_UTIL_H_
