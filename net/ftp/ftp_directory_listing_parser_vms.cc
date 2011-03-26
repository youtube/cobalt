// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_vms.h"

#include <vector>

#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "net/ftp/ftp_directory_listing_parser.h"
#include "net/ftp/ftp_util.h"

namespace net {

namespace {

// Converts the filename component in listing to the filename we can display.
// Returns true on success.
bool ParseVmsFilename(const string16& raw_filename, string16* parsed_filename,
                      FtpDirectoryListingEntry::Type* type) {
  // On VMS, the files and directories are versioned. The version number is
  // separated from the file name by a semicolon. Example: ANNOUNCE.TXT;2.
  std::vector<string16> listing_parts;
  base::SplitString(raw_filename, ';', &listing_parts);
  if (listing_parts.size() != 2)
    return false;
  int version_number;
  if (!base::StringToInt(listing_parts[1], &version_number))
    return false;
  if (version_number < 0)
    return false;

  // Even directories have extensions in the listings. Don't display extensions
  // for directories; it's awkward for non-VMS users. Also, VMS is
  // case-insensitive, but generally uses uppercase characters. This may look
  // awkward, so we convert them to lower case.
  std::vector<string16> filename_parts;
  base::SplitString(listing_parts[0], '.', &filename_parts);
  if (filename_parts.size() != 2)
    return false;
  if (EqualsASCII(filename_parts[1], "DIR")) {
    *parsed_filename = StringToLowerASCII(filename_parts[0]);
    *type = FtpDirectoryListingEntry::DIRECTORY;
  } else {
    *parsed_filename = StringToLowerASCII(listing_parts[0]);
    *type = FtpDirectoryListingEntry::FILE;
  }
  return true;
}

bool ParseVmsFilesize(const string16& input, int64* size) {
  // VMS's directory listing gives us file size in blocks. We assume that
  // the block size is 512 bytes. It doesn't give accurate file size, but is the
  // best information we have.
  const int kBlockSize = 512;

  if (base::StringToInt64(input, size)) {
    *size *= kBlockSize;
    return true;
  }

  std::vector<string16> parts;
  base::SplitString(input, '/', &parts);
  if (parts.size() != 2)
    return false;

  int64 blocks_used, blocks_allocated;
  if (!base::StringToInt64(parts[0], &blocks_used))
    return false;
  if (!base::StringToInt64(parts[1], &blocks_allocated))
    return false;
  if (blocks_used > blocks_allocated)
    return false;

  *size = blocks_used * kBlockSize;
  return true;
}

bool LooksLikeVmsFileProtectionListingPart(const string16& input) {
  if (input.length() > 4)
    return false;

  // On VMS there are four different permission bits: Read, Write, Execute,
  // and Delete. They appear in that order in the permission listing.
  std::string pattern("RWED");
  string16 match(input);
  while (!match.empty() && !pattern.empty()) {
    if (match[0] == pattern[0])
      match = match.substr(1);
    pattern = pattern.substr(1);
  }
  return match.empty();
}

bool LooksLikeVmsFileProtectionListing(const string16& input) {
  if (input.length() < 2)
    return false;
  if (input[0] != '(' || input[input.length() - 1] != ')')
    return false;

  // We expect four parts of the file protection listing: for System, Owner,
  // Group, and World.
  std::vector<string16> parts;
  base::SplitString(input.substr(1, input.length() - 2), ',', &parts);
  if (parts.size() != 4)
    return false;

  return LooksLikeVmsFileProtectionListingPart(parts[0]) &&
      LooksLikeVmsFileProtectionListingPart(parts[1]) &&
      LooksLikeVmsFileProtectionListingPart(parts[2]) &&
      LooksLikeVmsFileProtectionListingPart(parts[3]);
}

bool LooksLikeVmsUserIdentificationCode(const string16& input) {
  if (input.length() < 2)
    return false;
  return input[0] == '[' && input[input.length() - 1] == ']';
}

bool LooksLikePermissionDeniedError(const string16& text) {
  static const char* kPermissionDeniedMessages[] = {
    "%RMS-E-PRV",
    "privilege",
  };

  for (size_t i = 0; i < arraysize(kPermissionDeniedMessages); i++) {
    if (text.find(ASCIIToUTF16(kPermissionDeniedMessages[i])) != string16::npos)
      return true;
  }

  return false;
}

bool VmsDateListingToTime(const std::vector<string16>& columns,
                          base::Time* time) {
  DCHECK_EQ(4U, columns.size());

  base::Time::Exploded time_exploded = { 0 };

  // Date should be in format DD-MMM-YYYY.
  std::vector<string16> date_parts;
  base::SplitString(columns[2], '-', &date_parts);
  if (date_parts.size() != 3)
    return false;
  if (!base::StringToInt(date_parts[0], &time_exploded.day_of_month))
    return false;
  if (!FtpUtil::AbbreviatedMonthToNumber(date_parts[1],
                                         &time_exploded.month))
    return false;
  if (!base::StringToInt(date_parts[2], &time_exploded.year))
    return false;

  // Time can be in format HH:MM, HH:MM:SS, or HH:MM:SS.mm. Try to recognize the
  // last type first. Do not parse the seconds, they will be ignored anyway.
  string16 time_column(columns[3]);
  if (time_column.length() == 11 && time_column[8] == '.')
    time_column = time_column.substr(0, 8);
  if (time_column.length() == 8 && time_column[5] == ':')
    time_column = time_column.substr(0, 5);
  if (time_column.length() != 5)
    return false;
  std::vector<string16> time_parts;
  base::SplitString(time_column, ':', &time_parts);
  if (time_parts.size() != 2)
    return false;
  if (!base::StringToInt(time_parts[0], &time_exploded.hour))
    return false;
  if (!base::StringToInt(time_parts[1], &time_exploded.minute))
    return false;

  // We don't know the time zone of the server, so just use local time.
  *time = base::Time::FromLocalExploded(time_exploded);
  return true;
}

}  // namespace

bool ParseFtpDirectoryListingVms(
    const std::vector<string16>& lines,
    std::vector<FtpDirectoryListingEntry>* entries) {
  // The first non-empty line is the listing header. It often
  // starts with "Directory ", but not always. We set a flag after
  // seing the header.
  bool seen_header = false;

  for (size_t i = 0; i < lines.size(); i++) {
    if (lines[i].empty())
      continue;

    if (StartsWith(lines[i], ASCIIToUTF16("Total of "), true)) {
      // After the "total" line, all following lines must be empty.
      for (size_t j = i + 1; j < lines.size(); j++)
        if (!lines[j].empty())
          return false;

      return true;
    }

    if (!seen_header) {
      seen_header = true;
      continue;
    }

    if (LooksLikePermissionDeniedError(lines[i]))
      continue;

    std::vector<string16> columns;
    base::SplitString(CollapseWhitespace(lines[i], false), ' ', &columns);

    if (columns.size() == 1) {
      // There can be no continuation if the current line is the last one.
      if (i == lines.size() - 1)
        return false;

      // Join the current and next line and split them into columns.
      columns.clear();
      base::SplitString(
          CollapseWhitespace(lines[i] + ASCIIToUTF16(" ") + lines[i + 1],
                             false),
          ' ',
          &columns);
      i++;
    }

    FtpDirectoryListingEntry entry;
    if (!ParseVmsFilename(columns[0], &entry.name, &entry.type))
      return false;

    // There are different variants of a VMS listing. Some display
    // the protection listing and user identification code, some do not.
    if (columns.size() == 6) {
      if (!LooksLikeVmsFileProtectionListing(columns[5]))
        return false;
      if (!LooksLikeVmsUserIdentificationCode(columns[4]))
        return false;

      // Drop the unneeded data, so that the following code can always expect
      // just four columns.
      columns.resize(4);
    }

    if (columns.size() != 4)
      return false;

    if (!ParseVmsFilesize(columns[1], &entry.size))
      return false;
    if (entry.size < 0)
      return false;
    if (entry.type != FtpDirectoryListingEntry::FILE)
      entry.size = -1;
    if (!VmsDateListingToTime(columns, &entry.last_modified))
      return false;

    entries->push_back(entry);
  }

  // The only place where we return true is after receiving the "Total" line,
  // that should be present in every VMS listing.
  return false;
}

}  // namespace net
