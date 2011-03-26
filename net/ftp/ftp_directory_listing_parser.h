// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_PARSER_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_PARSER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/time.h"

namespace net {

struct FtpDirectoryListingEntry {
  enum Type {
    FILE,
    DIRECTORY,
    SYMLINK,
  };

  FtpDirectoryListingEntry();

  Type type;
  string16 name;  // Name (UTF-16-encoded).
  std::string raw_name;  // Name in original character encoding.
  int64 size;  // File size, in bytes. -1 if not applicable.

  // Last modified time, in local time zone.
  base::Time last_modified;
};

// Parses an FTP directory listing |text|. On success fills in |entries|.
// Returns network error code.
int ParseFtpDirectoryListing(const std::string& text,
                             const base::Time& current_time,
                             std::vector<FtpDirectoryListingEntry>* entries);

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_PARSER_H_
