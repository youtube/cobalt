// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#if CHAR_MIN < 0
    if (domain[i] < 0)
      return "";
#endif
    if (domain[i] > 63)
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

bool DnsResponseBuffer::U8(uint8* v) {
  if (len_ < 1)
    return false;
  *v = *p_;
  p_++;
  len_--;
  return true;
}

bool DnsResponseBuffer::U16(uint16* v) {
  if (len_ < 2)
    return false;
  *v = static_cast<uint16>(p_[0]) << 8 |
       static_cast<uint16>(p_[1]);
  p_ += 2;
  len_ -= 2;
  return true;
}

bool DnsResponseBuffer::U32(uint32* v) {
  if (len_ < 4)
    return false;
  *v = static_cast<uint32>(p_[0]) << 24 |
       static_cast<uint32>(p_[1]) << 16 |
       static_cast<uint32>(p_[2]) << 8 |
       static_cast<uint32>(p_[3]);
  p_ += 4;
  len_ -= 4;
  return true;
}

bool DnsResponseBuffer::Skip(unsigned n) {
  if (len_ < n)
    return false;
  p_ += n;
  len_ -= n;
  return true;
}

bool DnsResponseBuffer::Block(base::StringPiece* out, unsigned len) {
  if (len_ < len)
    return false;
  *out = base::StringPiece(reinterpret_cast<const char*>(p_), len);
  p_ += len;
  len_ -= len;
  return true;
}

// DNSName parses a (possibly compressed) DNS name from the packet. If |name|
// is not NULL, then the name is written into it. See RFC 1035 section 4.1.4.
bool DnsResponseBuffer::DNSName(std::string* name) {
  unsigned jumps = 0;
  const uint8* p = p_;
  unsigned len = len_;

  if (name)
    name->clear();

  for (;;) {
    if (len < 1)
      return false;
    uint8 d = *p;
    p++;
    len--;

    // The two couple of bits of the length give the type of the length. It's
    // either a direct length or a pointer to the remainder of the name.
    if ((d & 0xc0) == 0xc0) {
      // This limit matches the depth limit in djbdns.
      if (jumps > 100)
        return false;
      if (len < 1)
        return false;
      uint16 offset = static_cast<uint16>(d) << 8 |
                      static_cast<uint16>(p[0]);
      offset &= 0x3ff;
      p++;
      len--;

      if (jumps == 0) {
        p_ = p;
        len_ = len;
      }
      jumps++;

      if (offset >= packet_len_)
        return false;
      p = &packet_[offset];
      len = packet_len_ - offset;
    } else if ((d & 0xc0) == 0) {
      uint8 label_len = d;
      if (len < label_len)
        return false;
      if (name && label_len) {
        if (!name->empty())
          name->append(".");
        name->append(reinterpret_cast<const char*>(p), label_len);
      }
      p += label_len;
      len -= label_len;

      if (jumps == 0) {
        p_ = p;
        len_ = len;
      }

      if (label_len == 0)
        break;
    } else {
      return false;
    }
  }

  return true;
}

}  // namespace net
