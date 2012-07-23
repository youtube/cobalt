// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_hosts.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"

namespace net {

void ParseHosts(const std::string& contents, DnsHosts* dns_hosts) {
  CHECK(dns_hosts);
  DnsHosts& hosts = *dns_hosts;
  // Split into lines. Accept CR for Windows.
  StringTokenizer contents_lines(contents, "\n\r");
  while (contents_lines.GetNext()) {
    // Ignore comments after '#'.
    std::string line = contents_lines.token();
    StringTokenizer line_parts(line, "#");
    line_parts.set_options(StringTokenizer::RETURN_DELIMS);

    if (line_parts.GetNext() && !line_parts.token_is_delim()) {
      // Split and trim whitespace.
      std::string part = line_parts.token();
      StringTokenizer tokens(part, " \t");

      if (tokens.GetNext()) {
        IPAddressNumber ip;
        // TODO(szym): handle %iface notation on mac
        if (!ParseIPLiteralToNumber(tokens.token(), &ip))
          continue;  // Ignore malformed lines.
        AddressFamily fam = (ip.size() == 4) ? ADDRESS_FAMILY_IPV4 :
                                               ADDRESS_FAMILY_IPV6;
        while (tokens.GetNext()) {
          DnsHostsKey key(tokens.token(), fam);
          StringToLowerASCII(&(key.first));
          IPAddressNumber& mapped_ip = hosts[key];
          if (mapped_ip.empty())
            mapped_ip = ip;
          // else ignore this entry (first hit counts)
        }
      }
    }
  }
}

// Reads the contents of the file at |path| into |str| if the total length is
// less than |max_size|.
static bool ReadFile(const FilePath& path, int64 max_size, std::string* str) {
  int64 size;
  if (!file_util::GetFileSize(path, &size) || size > max_size)
    return false;
  return file_util::ReadFileToString(path, str);
}

bool ParseHostsFile(const FilePath& path, DnsHosts* dns_hosts) {
  dns_hosts->clear();
  // Missing file indicates empty HOSTS.
  if (!file_util::PathExists(path))
    return true;

  std::string contents;
  const int64 kMaxHostsSize = 1 << 16;
  if (!ReadFile(path, kMaxHostsSize, &contents))
    return false;

  ParseHosts(contents, dns_hosts);
  return true;
}

}  // namespace net

