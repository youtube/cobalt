// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_netware.h"

#include <vector>

#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/ftp/ftp_util.h"

namespace {

bool LooksLikeNetwarePermissionsListing(const string16& text) {
  if (text.length() != 10)
    return false;

  if (text[0] != '[' || text[9] != ']')
    return false;
  return (text[1] == 'R' || text[1] == '-') &&
         (text[2] == 'W' || text[2] == '-') &&
         (text[3] == 'C' || text[3] == '-') &&
         (text[4] == 'E' || text[4] == '-') &&
         (text[5] == 'A' || text[5] == '-') &&
         (text[6] == 'F' || text[6] == '-') &&
         (text[7] == 'M' || text[7] == '-') &&
         (text[8] == 'S' || text[8] == '-');
}

}  // namespace

namespace net {

FtpDirectoryListingParserNetware::FtpDirectoryListingParserNetware(
    const base::Time& current_time)
    : received_first_line_(false),
      current_time_(current_time) {
}

FtpDirectoryListingParserNetware::~FtpDirectoryListingParserNetware() {}

FtpServerType FtpDirectoryListingParserNetware::GetServerType() const {
  return SERVER_NETWARE;
}

bool FtpDirectoryListingParserNetware::ConsumeLine(const string16& line) {
  if (!received_first_line_) {
    received_first_line_ = true;

    return StartsWith(line, ASCIIToUTF16("total "), true);
  }

  std::vector<string16> columns;
  base::SplitString(CollapseWhitespace(line, false), ' ', &columns);

  if (columns.size() != 8)
    return false;

  FtpDirectoryListingEntry entry;

  if (columns[0].length() != 1)
    return false;
  if (columns[0][0] == 'd') {
    entry.type = FtpDirectoryListingEntry::DIRECTORY;
  } else if (columns[0][0] == '-') {
    entry.type = FtpDirectoryListingEntry::FILE;
  } else {
    return false;
  }

  // Note: on older Netware systems the permissions listing is in the same
  // column as the entry type (just there is no space between them). We do not
  // support the older format here for simplicity.
  if (!LooksLikeNetwarePermissionsListing(columns[1]))
    return false;

  if (!base::StringToInt64(columns[3], &entry.size))
    return false;
  if (entry.size < 0)
    return false;
  if (entry.type != FtpDirectoryListingEntry::FILE)
    entry.size = -1;

  // Netware uses the same date listing format as Unix "ls -l".
  if (!FtpUtil::LsDateListingToTime(columns[4], columns[5], columns[6],
                                    current_time_, &entry.last_modified)) {
    return false;
  }

  entry.name = columns[7];

  entries_.push(entry);
  return true;
}

bool FtpDirectoryListingParserNetware::OnEndOfInput() {
  return true;
}

bool FtpDirectoryListingParserNetware::EntryAvailable() const {
  return !entries_.empty();
}

FtpDirectoryListingEntry FtpDirectoryListingParserNetware::PopEntry() {
  FtpDirectoryListingEntry entry = entries_.front();
  entries_.pop();
  return entry;
}

}  // namespace net
