// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_PARSER_VMS_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_PARSER_VMS_H_
#pragma once

#include <vector>

#include "base/string16.h"

namespace net {

struct FtpDirectoryListingEntry;

// Parses VMS FTP directory listing. Returns true on success.
bool ParseFtpDirectoryListingVms(
    const std::vector<string16>& lines,
    std::vector<FtpDirectoryListingEntry>* entries);

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_PARSER_VMS_H_
