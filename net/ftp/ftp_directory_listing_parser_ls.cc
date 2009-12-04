// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_ls.h"

#include <vector>

#include "base/string_util.h"
#include "net/ftp/ftp_util.h"

namespace {

bool LooksLikeUnixPermission(const string16& text) {
  if (text.length() != 3)
    return false;

  // Meaning of the flags:
  // r - file is readable
  // w - file is writable
  // x - file is executable
  // s or S - setuid/setgid bit set
  // t or T - "sticky" bit set
  return ((text[0] == 'r' || text[0] == '-') &&
          (text[1] == 'w' || text[1] == '-') &&
          (text[2] == 'x' || text[2] == 's' || text[2] == 'S' ||
           text[2] == 't' || text[2] == 'T' || text[2] == '-'));
}

bool LooksLikeUnixPermissionsListing(const string16& text) {
  if (text.length() != 10)
    return false;

  if (text[0] != 'b' && text[0] != 'c' && text[0] != 'd' &&
      text[0] != 'l' && text[0] != 'p' && text[0] != 's' &&
      text[0] != '-')
    return false;

  return (LooksLikeUnixPermission(text.substr(1, 3)) &&
          LooksLikeUnixPermission(text.substr(4, 3)) &&
          LooksLikeUnixPermission(text.substr(7, 3)));
}

string16 GetStringPartAfterColumns(const string16& text, int columns) {
  DCHECK_LE(1, columns);
  int columns_so_far = 0;
  size_t last = 0;
  for (size_t i = 1; i < text.length(); ++i) {
    if (!isspace(text[i - 1]) && isspace(text[i])) {
      last = i;
      if (++columns_so_far == columns)
        break;
    }
  }
  string16 result(text.substr(last));
  TrimWhitespace(result, TRIM_ALL, &result);
  return result;
}

bool UnixDateListingToTime(const std::vector<string16>& columns,
                           base::Time* time) {
  DCHECK_LE(9U, columns.size());

  base::Time::Exploded time_exploded = { 0 };

  if (!net::FtpUtil::ThreeLetterMonthToNumber(columns[5], &time_exploded.month))
    return false;

  if (!StringToInt(columns[6], &time_exploded.day_of_month))
    return false;

  if (!StringToInt(columns[7], &time_exploded.year)) {
    // Maybe it's time. Does it look like time (MM:HH)?
    if (columns[7].length() != 5 || columns[7][2] != ':')
      return false;

    if (!StringToInt(columns[7].substr(0, 2), &time_exploded.hour))
      return false;

    if (!StringToInt(columns[7].substr(3, 2), &time_exploded.minute))
      return false;

    // Use current year.
    base::Time::Exploded now_exploded;
    base::Time::Now().LocalExplode(&now_exploded);
    time_exploded.year = now_exploded.year;
  }

  // We don't know the time zone of the server, so just use local time.
  *time = base::Time::FromLocalExploded(time_exploded);
  return true;
}

}  // namespace

namespace net {

FtpDirectoryListingParserLs::FtpDirectoryListingParserLs()
    : received_nonempty_line_(false) {
}

bool FtpDirectoryListingParserLs::ConsumeLine(const string16& line) {
  if (StartsWith(line, ASCIIToUTF16("total "), true) ||
      StartsWith(line, ASCIIToUTF16("Gesamt "), true)) {
    // Some FTP servers put a "total n" line at the beginning of the listing
    // (n is an integer). Allow such a line, but only once, and only if it's
    // the first non-empty line.
    //
    // Note: "Gesamt" is a German word for "total". The case is important here:
    // for "ls -l" style listings, "total" will be lowercase, and Gesamt will be
    // capitalized. This helps us distinguish that from a VMS-style listing,
    // which would use "Total" (note the uppercase first letter).

    if (received_nonempty_line_)
      return false;

    received_nonempty_line_ = true;
    return true;
  }
  if (line.empty() && !received_nonempty_line_) {
    // Allow empty lines only at the beginning of the listing. For example VMS
    // systems in Unix emulation mode add an empty line before the first listing
    // entry.
    return true;
  }
  received_nonempty_line_ = true;

  std::vector<string16> columns;
  SplitString(CollapseWhitespace(line, false), ' ', &columns);

  // We may receive file names containing spaces, which can make the number of
  // columns arbitrarily large. We will handle that later. For now just make
  // sure we have all the columns that should normally be there.
  if (columns.size() < 9)
    return false;

  if (!LooksLikeUnixPermissionsListing(columns[0]))
    return false;

  FtpDirectoryListingEntry entry;
  if (columns[0][0] == 'l') {
    entry.type = FtpDirectoryListingEntry::SYMLINK;
  } else if (columns[0][0] == 'd') {
    entry.type = FtpDirectoryListingEntry::DIRECTORY;
  } else {
    entry.type = FtpDirectoryListingEntry::FILE;
  }

  int number_of_links;
  if (!StringToInt(columns[1], &number_of_links))
    return false;
  if (number_of_links < 0)
    return false;

  if (!StringToInt64(columns[4], &entry.size))
    return false;
  if (entry.size < 0)
    return false;
  if (entry.type != FtpDirectoryListingEntry::FILE)
    entry.size = -1;

  if (!UnixDateListingToTime(columns, &entry.last_modified))
    return false;

  entry.name = GetStringPartAfterColumns(line, 8);
  if (entry.type == FtpDirectoryListingEntry::SYMLINK) {
    string16::size_type pos = entry.name.rfind(ASCIIToUTF16(" -> "));
    if (pos == string16::npos)
      return false;
    entry.name = entry.name.substr(0, pos);
  }

  entries_.push(entry);
  return true;
}

bool FtpDirectoryListingParserLs::OnEndOfInput() {
  return true;
}

bool FtpDirectoryListingParserLs::EntryAvailable() const {
  return !entries_.empty();
}

FtpDirectoryListingEntry FtpDirectoryListingParserLs::PopEntry() {
  FtpDirectoryListingEntry entry = entries_.front();
  entries_.pop();
  return entry;
}

}  // namespace net
