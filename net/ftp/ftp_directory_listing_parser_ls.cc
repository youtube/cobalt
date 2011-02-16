// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_ls.h"

#include <vector>

#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
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
  if (text.length() < 10)
    return false;

  if (text[0] != 'b' && text[0] != 'c' && text[0] != 'd' &&
      text[0] != 'l' && text[0] != 'p' && text[0] != 's' &&
      text[0] != '-')
    return false;

  // Do not check the rest of the string. Some servers fail to properly
  // separate this column from the next column (number of links), resulting
  // in additional characters at the end. Also, sometimes there is a "+"
  // sign at the end indicating the file has ACLs set.
  return (LooksLikeUnixPermission(text.substr(1, 3)) &&
          LooksLikeUnixPermission(text.substr(4, 3)) &&
          LooksLikeUnixPermission(text.substr(7, 3)));
}

bool LooksLikePermissionDeniedError(const string16& text) {
  // Try to recognize a three-part colon-separated error message:
  //
  //   1. ftpd server name
  //   2. directory name (often just ".")
  //   3. message text (usually "Permission denied")
  std::vector<string16> parts;
  base::SplitString(CollapseWhitespace(text, false), ':', &parts);

  if (parts.size() != 3)
    return false;

  return parts[2] == ASCIIToUTF16("Permission denied");
}

bool DetectColumnOffset(const std::vector<string16>& columns,
                        const base::Time& current_time, int* offset) {
  base::Time time;

  if (columns.size() >= 8 &&
      net::FtpUtil::LsDateListingToTime(columns[5], columns[6], columns[7],
                                        current_time, &time)) {
    // Standard listing, exactly like ls -l.
    *offset = 2;
    return true;
  }

  if (columns.size() >= 7 &&
      net::FtpUtil::LsDateListingToTime(columns[4], columns[5], columns[6],
                                        current_time, &time)) {
    // wu-ftpd listing, no "number of links" column.
    *offset = 1;
    return true;
  }

  if (columns.size() >= 6 &&
      net::FtpUtil::LsDateListingToTime(columns[3], columns[4], columns[5],
                                        current_time, &time)) {
    // Xplain FTP Server listing for folders, like this:
    // drwxr-xr-x               folder        0 Jul 17  2006 online
    *offset = 0;
    return true;
  }

  // Unrecognized listing style.
  return false;
}

}  // namespace

namespace net {

FtpDirectoryListingParserLs::FtpDirectoryListingParserLs(
    const base::Time& current_time)
    : received_total_line_(false),
      current_time_(current_time) {
}

FtpDirectoryListingParserLs::~FtpDirectoryListingParserLs() {}

FtpServerType FtpDirectoryListingParserLs::GetServerType() const {
  return SERVER_LS;
}

bool FtpDirectoryListingParserLs::ConsumeLine(const string16& line) {
  if (line.empty())
    return true;

  std::vector<string16> columns;
  base::SplitString(CollapseWhitespace(line, false), ' ', &columns);

  // Some FTP servers put a "total n" line at the beginning of the listing
  // (n is an integer). Allow such a line, but only once, and only if it's
  // the first non-empty line. Do not match the word exactly, because it may be
  // in different languages (at least English and German have been seen in the
  // field).
  if (columns.size() == 2 && !received_total_line_) {
    received_total_line_ = true;

    int total_number;
    if (!base::StringToInt(columns[1], &total_number))
      return false;
    if (total_number < 0)
      return false;

    return true;
  }

  int column_offset;
  if (!DetectColumnOffset(columns, current_time_, &column_offset)) {
    // If we can't recognize a normal listing line, maybe it's an error?
    // In that case, just ignore the error, but still recognize the data
    // as valid listing.
    return LooksLikePermissionDeniedError(line);
  }

  // We may receive file names containing spaces, which can make the number of
  // columns arbitrarily large. We will handle that later. For now just make
  // sure we have all the columns that should normally be there:
  //
  //   1. permission listing
  //   2. number of links (optional)
  //   3. owner name
  //   4. group name (optional)
  //   5. size in bytes
  //   6. month
  //   7. day of month
  //   8. year or time
  //
  // The number of optional columns is stored in |column_offset|
  // and is between 0 and 2 (inclusive).
  if (columns.size() < 6U + column_offset)
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

  if (!base::StringToInt64(columns[2 + column_offset], &entry.size)) {
    // Some FTP servers do not separate owning group name from file size,
    // like "group1234". We still want to display the file name for that entry,
    // but can't really get the size (What if the group is named "group1",
    // and the size is in fact 234? We can't distinguish between that
    // and "group" with size 1234). Use a dummy value for the size.
    // TODO(phajdan.jr): Use a value that means "unknown" instead of 0 bytes.
    entry.size = 0;
  }
  if (entry.size < 0)
    return false;
  if (entry.type != FtpDirectoryListingEntry::FILE)
    entry.size = -1;

  if (!FtpUtil::LsDateListingToTime(columns[3 + column_offset],
                                    columns[4 + column_offset],
                                    columns[5 + column_offset],
                                    current_time_,
                                    &entry.last_modified)) {
    return false;
  }

  entry.name = FtpUtil::GetStringPartAfterColumns(line, 6 + column_offset);

  if (entry.name.empty()) {
    // Some FTP servers send listing entries with empty names. It's not obvious
    // how to display such an entry, so we ignore them. We don't want to make
    // the parsing fail at this point though. Other entries can still be useful.
    return true;
  }

  if (entry.type == FtpDirectoryListingEntry::SYMLINK) {
    string16::size_type pos = entry.name.rfind(ASCIIToUTF16(" -> "));

    // We don't require the " -> " to be present. Some FTP servers don't send
    // the symlink target, possibly for security reasons.
    if (pos != string16::npos)
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
