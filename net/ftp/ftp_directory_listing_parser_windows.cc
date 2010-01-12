// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_windows.h"

#include <vector>

#include "base/string_util.h"
#include "net/ftp/ftp_util.h"

namespace {

bool WindowsDateListingToTime(const std::vector<string16>& columns,
                              base::Time* time) {
  DCHECK_LE(4U, columns.size());

  base::Time::Exploded time_exploded = { 0 };

  // Date should be in format MM-DD-YY[YY].
  std::vector<string16> date_parts;
  SplitString(columns[0], '-', &date_parts);
  if (date_parts.size() != 3)
    return false;
  if (!StringToInt(date_parts[0], &time_exploded.month))
    return false;
  if (!StringToInt(date_parts[1], &time_exploded.day_of_month))
    return false;
  if (!StringToInt(date_parts[2], &time_exploded.year))
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
  SplitString(columns[1].substr(0, 5), ':', &time_parts);
  if (time_parts.size() != 2)
    return false;
  if (!StringToInt(time_parts[0], &time_exploded.hour))
    return false;
  if (!StringToInt(time_parts[1], &time_exploded.minute))
    return false;
  string16 am_or_pm(columns[1].substr(5, 2));
  if (EqualsASCII(am_or_pm, "PM"))
    time_exploded.hour += 12;
  else if (!EqualsASCII(am_or_pm, "AM"))
    return false;

  // We don't know the time zone of the server, so just use local time.
  *time = base::Time::FromLocalExploded(time_exploded);
  return true;
}

}  // namespace

namespace net {

FtpDirectoryListingParserWindows::FtpDirectoryListingParserWindows() {
}

bool FtpDirectoryListingParserWindows::ConsumeLine(const string16& line) {
  std::vector<string16> columns;
  SplitString(CollapseWhitespace(line, false), ' ', &columns);

  // We may receive file names containing spaces, which can make the number of
  // columns arbitrarily large. We will handle that later. For now just make
  // sure we have all the columns that should normally be there.
  if (columns.size() < 4)
    return false;

  FtpDirectoryListingEntry entry;
  entry.name = FtpUtil::GetStringPartAfterColumns(line, 3);

  if (EqualsASCII(columns[2], "<DIR>")) {
    entry.type = FtpDirectoryListingEntry::DIRECTORY;
    entry.size = -1;
  } else {
    entry.type = FtpDirectoryListingEntry::FILE;
    if (!StringToInt64(columns[2], &entry.size))
      return false;
    if (entry.size < 0)
      return false;
  }

  if (!WindowsDateListingToTime(columns, &entry.last_modified))
    return false;

  entries_.push(entry);
  return true;
}

bool FtpDirectoryListingParserWindows::OnEndOfInput() {
  return true;
}

bool FtpDirectoryListingParserWindows::EntryAvailable() const {
  return !entries_.empty();
}

FtpDirectoryListingEntry FtpDirectoryListingParserWindows::PopEntry() {
  FtpDirectoryListingEntry entry = entries_.front();
  entries_.pop();
  return entry;
}

}  // namespace net
