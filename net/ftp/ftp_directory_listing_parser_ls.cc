// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_ls.h"

#include <vector>

#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "net/ftp/ftp_directory_listing_parser.h"
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

// Returns the column index of the end of the date listing and detected
// last modification time.
bool DetectColumnOffsetAndModificationTime(const std::vector<string16>& columns,
                                           const base::Time& current_time,
                                           size_t* offset,
                                           base::Time* modification_time) {
  // The column offset can be arbitrarily large if some fields
  // like owner or group name contain spaces. Try offsets from left to right
  // and use the first one that matches a date listing.
  //
  // Here is how a listing line should look like. A star ("*") indicates
  // a required field:
  //
  //  * 1. permission listing
  //    2. number of links (optional)
  //  * 3. owner name (may contain spaces)
  //    4. group name (optional, may contain spaces)
  //  * 5. size in bytes
  //  * 6. month
  //  * 7. day of month
  //  * 8. year or time <-- column_offset will be the index of this column
  //    9. file name (optional, may contain spaces)
  for (size_t i = 5U; i < columns.size(); i++) {
    if (net::FtpUtil::LsDateListingToTime(columns[i - 2],
                                          columns[i - 1],
                                          columns[i],
                                          current_time,
                                          modification_time)) {
      *offset = i;
      return true;
    }
  }

  // Some FTP listings have swapped the "month" and "day of month" columns
  // (for example Russian listings). We try to recognize them only after making
  // sure no column offset works above (this is a more strict way).
  for (size_t i = 5U; i < columns.size(); i++) {
    if (net::FtpUtil::LsDateListingToTime(columns[i - 1],
                                          columns[i - 2],
                                          columns[i],
                                          current_time,
                                          modification_time)) {
      *offset = i;
      return true;
    }
  }

  return false;
}

}  // namespace

namespace net {

bool ParseFtpDirectoryListingLs(
    const std::vector<string16>& lines,
    const base::Time& current_time,
    std::vector<FtpDirectoryListingEntry>* entries) {
  // True after we have received a "total n" listing header, where n is an
  // integer. Only one such header is allowed per listing.
  bool received_total_line = false;

  for (size_t i = 0; i < lines.size(); i++) {
    if (lines[i].empty())
      continue;

    std::vector<string16> columns;
    base::SplitString(CollapseWhitespace(lines[i], false), ' ', &columns);

    // Some FTP servers put a "total n" line at the beginning of the listing
    // (n is an integer). Allow such a line, but only once, and only if it's
    // the first non-empty line. Do not match the word exactly, because it may
    // be in different languages (at least English and German have been seen
    // in the field).
    if (columns.size() == 2 && !received_total_line) {
      received_total_line = true;

      int total_number;
      if (!base::StringToInt(columns[1], &total_number))
        return false;
      if (total_number < 0)
        return false;

      continue;
    }

    FtpDirectoryListingEntry entry;

    size_t column_offset;
    if (!DetectColumnOffsetAndModificationTime(columns,
                                               current_time,
                                               &column_offset,
                                               &entry.last_modified)) {
      // If we can't recognize a normal listing line, maybe it's an error?
      // In that case, just ignore the error, but still recognize the data
      // as valid listing.
      if (LooksLikePermissionDeniedError(lines[i]))
        continue;

      return false;
    }

    if (!LooksLikeUnixPermissionsListing(columns[0]))
      return false;
    if (columns[0][0] == 'l') {
      entry.type = FtpDirectoryListingEntry::SYMLINK;
    } else if (columns[0][0] == 'd') {
      entry.type = FtpDirectoryListingEntry::DIRECTORY;
    } else {
      entry.type = FtpDirectoryListingEntry::FILE;
    }

    if (!base::StringToInt64(columns[column_offset - 3], &entry.size)) {
      // Some FTP servers do not separate owning group name from file size,
      // like "group1234". We still want to display the file name for that
      // entry, but can't really get the size (What if the group is named
      // "group1", and the size is in fact 234? We can't distinguish between
      // that and "group" with size 1234). Use a dummy value for the size.
      // TODO(phajdan.jr): Use a value that means "unknown" instead of 0 bytes.
      entry.size = 0;
    }
    if (entry.size < 0)
      return false;
    if (entry.type != FtpDirectoryListingEntry::FILE)
      entry.size = -1;

    if (column_offset == columns.size() - 1) {
      // If the end of the date listing is the last column, there is no file
      // name. Some FTP servers send listing entries with empty names.
      // It's not obvious how to display such an entry, so we ignore them.
      // We don't want to make the parsing fail at this point though.
      // Other entries can still be useful.
      continue;
    }

    entry.name = FtpUtil::GetStringPartAfterColumns(lines[i],
                                                    column_offset + 1);

    if (entry.type == FtpDirectoryListingEntry::SYMLINK) {
      string16::size_type pos = entry.name.rfind(ASCIIToUTF16(" -> "));

      // We don't require the " -> " to be present. Some FTP servers don't send
      // the symlink target, possibly for security reasons.
      if (pos != string16::npos)
        entry.name = entry.name.substr(0, pos);
    }

    entries->push_back(entry);
  }

  return true;
}

}  // namespace net
