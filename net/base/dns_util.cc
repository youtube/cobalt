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
  name[namelen++] = 0;

  *out = name;
  return true;
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

}  // namespace net
