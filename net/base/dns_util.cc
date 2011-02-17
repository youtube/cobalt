// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/dns_util.h"

#include <cstring>

namespace net {

// Based on DJB's public domain code.
bool DNSDomainFromDot(const std::string& dotted, std::string* out) {
  const char* buf = dotted.data();
  unsigned n = dotted.size();
  char label[63];
  unsigned int labellen = 0; /* <= sizeof label */
  char name[255];
  unsigned int namelen = 0; /* <= sizeof name */
  char ch;

  for (;;) {
    if (!n)
      break;
    ch = *buf++;
    --n;
    if (ch == '.') {
      if (labellen) {
        if (namelen + labellen + 1 > sizeof name)
          return false;
        name[namelen++] = labellen;
        memcpy(name + namelen, label, labellen);
        namelen += labellen;
        labellen = 0;
      }
      continue;
    }
    if (labellen >= sizeof label)
      return false;
    label[labellen++] = ch;
  }

  if (labellen) {
    if (namelen + labellen + 1 > sizeof name)
      return false;
    name[namelen++] = labellen;
    memcpy(name + namelen, label, labellen);
    namelen += labellen;
    labellen = 0;
  }

  if (namelen + 1 > sizeof name)
    return false;
  name[namelen++] = 0;  // This is the root label (of length 0).

  *out = std::string(name, namelen);
  return true;
}

std::string DNSDomainToString(const std::string& domain) {
  std::string ret;

  for (unsigned i = 0; i < domain.size() && domain[i]; i += domain[i] + 1) {
    if (domain[i] < 0 || domain[i] > 63)
      return "";

    if (i)
      ret += ".";

    if (static_cast<unsigned>(domain[i]) + i + 1 > domain.size())
      return "";

    ret += domain.substr(i + 1, domain[i]);
  }
  return ret;
}

bool IsSTD3ASCIIValidCharacter(char c) {
  if (c <= 0x2c)
    return false;
  if (c >= 0x7b)
    return false;
  if (c >= 0x2e && c <= 0x2f)
    return false;
  if (c >= 0x3a && c <= 0x40)
    return false;
  if (c >= 0x5b && c <= 0x60)
    return false;
  return true;
}

std::string TrimEndingDot(const std::string& host) {
  std::string host_trimmed = host;
  size_t len = host_trimmed.length();
  if (len > 1 && host_trimmed[len - 1] == '.')
    host_trimmed.erase(len - 1);
  return host_trimmed;
}

}  // namespace net
