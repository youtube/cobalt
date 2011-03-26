// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_windows.h"

#include <vector>

#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/ftp/ftp_directory_listing_parser.h"
#include "net/ftp/ftp_util.h"

namespace {

bool WindowsDateListingToTime(const std::vector<string16>& columns,
                              base::Time* time) {
  DCHECK_LE(3U, columns.size());

  base::Time::Exploded time_exploded = { 0 };

  // Date should be in format MM-DD-YY[YY].
  std::vector<string16> date_parts;
  base::SplitString(columns[0], '-', &date_parts);
  if (date_parts.size() != 3)
    return false;
  if (!base::StringToInt(date_parts[0], &time_exploded.month))
    return false;
  if (!base::StringToInt(date_parts[1], &time_exploded.day_of_month))
    return false;
  if (!base::StringToInt(date_parts[2], &time_exploded.year))
    return false;
  if (time_exploded.year < 0)
    return false;
  // If year has only two digits then assume that 00-79 is 2000-2079,
  // and 80-99 is 1980-1999.
  if (time_exploded.year < 80)
    time_exploded.year += 2000;
  else if (time_exploded.year < 100)
    time_exploded.year += 1900;

  // Time should be in format HH:MM(AM|PM)
  if (columns[1].length() != 7)
    return false;
  std::vector<string16> time_parts;
  base::SplitString(columns[1].substr(0, 5), ':', &time_parts);
  if (time_parts.size() != 2)
    return false;
  if (!base::StringToInt(time_parts[0], &time_exploded.hour))
    return false;
  if (!base::StringToInt(time_parts[1], &time_exploded.minute))
    return false;
  if (!time_exploded.HasValidValues())
    return false;
  string16 am_or_pm(columns[1].substr(5, 2));
  if (EqualsASCII(am_or_pm, "PM")) {
    if (time_exploded.hour < 12)
      time_exploded.hour += 12;
  } else if (EqualsASCII(am_or_pm, "AM")) {
    if (time_exploded.hour == 12)
      time_exploded.hour = 0;
  } else {
    return false;
  }

  // We don't know the time zone of the server, so just use local time.
  *time = base::Time::FromLocalExploded(time_exploded);
  return true;
}

}  // namespace

namespace net {

bool ParseFtpDirectoryListingWindows(
    const std::vector<string16>& lines,
    std::vector<FtpDirectoryListingEntry>* entries) {
  for (size_t i = 0; i < lines.size(); i++) {
    if (lines[i].empty())
      continue;

    std::vector<string16> columns;
    base::SplitString(CollapseWhitespace(lines[i], false), ' ', &columns);

    // Every line of the listing consists of the following:
    //
    //   1. date
    //   2. time
    //   3. size in bytes (or "<DIR>" for directories)
    //   4. filename (may be empty or contain spaces)
    //
    // For now, make sure we have 1-3, and handle 4 later.
    if (columns.size() < 3)
      return false;

    FtpDirectoryListingEntry entry;
    if (EqualsASCII(columns[2], "<DIR>")) {
      entry.type = FtpDirectoryListingEntry::DIRECTORY;
      entry.size = -1;
    } else {
      entry.type = FtpDirectoryListingEntry::FILE;
      if (!base::StringToInt64(columns[2], &entry.size))
        return false;
      if (entry.size < 0)
        return false;
    }

    if (!WindowsDateListingToTime(columns, &entry.last_modified))
      return false;

    entry.name = FtpUtil::GetStringPartAfterColumns(lines[i], 3);
    if (entry.name.empty()) {
      // Some FTP servers send listing entries with empty names.
      // It's not obvious how to display such an entry, so ignore them.
      // We don't want to make the parsing fail at this point though.
      // Other entries can still be useful.
      continue;
    }

    entries->push_back(entry);
  }

  return true;
}

}  // namespace net
