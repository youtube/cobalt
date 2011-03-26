// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_PARSER_NETWARE_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_PARSER_NETWARE_H_
#pragma once

#include <vector>

#include "base/string16.h"

namespace base {
class Time;
}

namespace net {

struct FtpDirectoryListingEntry;

// Parses Netware FTP directory listing. Returns true on success.
bool ParseFtpDirectoryListingNetware(
    const std::vector<string16>& lines,
    const base::Time& current_time,
    std::vector<FtpDirectoryListingEntry>* entries);

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_PARSER_NETWARE_H_
