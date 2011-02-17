// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DNS_UTIL_H_
#define NET_BASE_DNS_UTIL_H_
#pragma once

#include <string>

#include "base/basictypes.h"

namespace net {

// DNSDomainFromDot - convert a domain string to DNS format. From DJB's
// public domain DNS library.
//
//   dotted: a string in dotted form: "www.google.com"
//   out: a result in DNS form: "\x03www\x06google\x03com\x00"
bool DNSDomainFromDot(const std::string& dotted, std::string* out);

// DNSDomainToString coverts a domain in DNS format to a dotted string.
std::string DNSDomainToString(const std::string& domain);

// Returns true iff the given character is in the set of valid DNS label
// characters as given in RFC 3490, 4.1, 3(a)
bool IsSTD3ASCIIValidCharacter(char c);

// Returns the hostname by trimming the ending dot, if one exists.
std::string TrimEndingDot(const std::string& host);

// DNS resource record types. See
// http://www.iana.org/assignments/dns-parameters
// WARNING: if you're adding any new values here you may need to add them to
// dnsrr_resolver.cc:DnsRRIsParsedByWindows.

static const uint16 kDNS_CNAME = 5;
static const uint16 kDNS_TXT = 16;
static const uint16 kDNS_CERT = 37;
static const uint16 kDNS_DS = 43;
static const uint16 kDNS_RRSIG = 46;
static const uint16 kDNS_DNSKEY = 48;
static const uint16 kDNS_ANY = 0xff;
static const uint16 kDNS_CAA = 13172;  // temporary, not IANA
static const uint16 kDNS_TESTING = 0xfffe;  // in private use area.

// http://www.iana.org/assignments/dns-sec-alg-numbers/dns-sec-alg-numbers.xhtml
static const uint8 kDNSSEC_RSA_SHA1 = 5;
static const uint8 kDNSSEC_RSA_SHA1_NSEC3 = 7;
static const uint8 kDNSSEC_RSA_SHA256 = 8;

// RFC 4509
static const uint8 kDNSSEC_SHA1 = 1;
static const uint8 kDNSSEC_SHA256 = 2;

}  // namespace net

#endif  // NET_BASE_DNS_UTIL_H_
