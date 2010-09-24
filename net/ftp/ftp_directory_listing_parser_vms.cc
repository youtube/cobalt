// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_vms.h"

#include <vector>

#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/ftp/ftp_util.h"

namespace {

// Converts the filename component in listing to the filename we can display.
// Returns true on success.
bool ParseVmsFilename(const string16& raw_filename, string16* parsed_filename,
                      bool* is_directory) {
  // On VMS, the files and directories are versioned. The version number is
  // separated from the file name by a semicolon. Example: ANNOUNCE.TXT;2.
  std::vector<string16> listing_parts;
  SplitString(raw_filename, ';', &listing_parts);
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
  SplitString(listing_parts[0], '.', &filename_parts);
  if (filename_parts.size() != 2)
    return false;
  if (EqualsASCII(filename_parts[1], "DIR")) {
    *parsed_filename = StringToLowerASCII(filename_parts[0]);
    *is_directory = true;
  } else {
    *parsed_filename = StringToLowerASCII(listing_parts[0]);
    *is_directory = false;
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
  SplitString(input, '/', &parts);
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
  SplitString(input.substr(1, input.length() - 2), ',', &parts);
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

bool VmsDateListingToTime(const std::vector<string16>& columns,
                          base::Time* time) {
  DCHECK_EQ(3U, columns.size());

  base::Time::Exploded time_exploded = { 0 };

  // Date should be in format DD-MMM-YYYY.
  std::vector<string16> date_parts;
  SplitString(columns[1], '-', &date_parts);
  if (date_parts.size() != 3)
    return false;
  if (!base::StringToInt(date_parts[0], &time_exploded.day_of_month))
    return false;
  if (!net::FtpUtil::ThreeLetterMonthToNumber(date_parts[1],
                                              &time_exploded.month))
    return false;
  if (!base::StringToInt(date_parts[2], &time_exploded.year))
    return false;

  // Time can be in format HH:MM, HH:MM:SS, or HH:MM:SS.mm. Try to recognize the
  // last type first. Do not parse the seconds, they will be ignored anyway.
  string16 time_column(columns[2]);
  if (time_column.length() == 11 && time_column[8] == '.')
    time_column = time_column.substr(0, 8);
  if (time_column.length() == 8 && time_column[5] == ':')
    time_column = time_column.substr(0, 5);
  if (time_column.length() != 5)
    return false;
  std::vector<string16> time_parts;
  SplitString(time_column, ':', &time_parts);
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

namespace net {

FtpDirectoryListingParserVms::FtpDirectoryListingParserVms()
    : state_(STATE_INITIAL),
      last_is_directory_(false) {
}

bool FtpDirectoryListingParserVms::ConsumeLine(const string16& line) {
  switch (state_) {
    case STATE_INITIAL:
      DCHECK(last_filename_.empty());
      if (line.empty())
        return true;
      if (StartsWith(line, ASCIIToUTF16("Total of "), true)) {
        state_ = STATE_END;
        return true;
      }
      // We assume that the first non-empty line is the listing header. It often
      // starts with "Directory ", but not always.
      state_ = STATE_RECEIVED_HEADER;
      return true;
    case STATE_RECEIVED_HEADER:
      DCHECK(last_filename_.empty());
      if (line.empty())
        return true;
      state_ = STATE_ENTRIES;
      return ConsumeEntryLine(line);
    case STATE_ENTRIES:
      if (line.empty()) {
        if (!last_filename_.empty())
          return false;
        state_ = STATE_RECEIVED_LAST_ENTRY;
        return true;
      }
      return ConsumeEntryLine(line);
    case STATE_RECEIVED_LAST_ENTRY:
      DCHECK(last_filename_.empty());
      if (line.empty())
        return true;
      if (!StartsWith(line, ASCIIToUTF16("Total of "), true))
        return false;
      state_ = STATE_END;
      return true;
    case STATE_END:
      DCHECK(last_filename_.empty());
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

bool FtpDirectoryListingParserVms::OnEndOfInput() {
  return (state_ == STATE_END);
}

bool FtpDirectoryListingParserVms::EntryAvailable() const {
  return !entries_.empty();
}

FtpDirectoryListingEntry FtpDirectoryListingParserVms::PopEntry() {
  FtpDirectoryListingEntry entry = entries_.front();
  entries_.pop();
  return entry;
}

bool FtpDirectoryListingParserVms::ConsumeEntryLine(const string16& line) {
  std::vector<string16> columns;
  SplitString(CollapseWhitespace(line, false), ' ', &columns);

  if (columns.size() == 1) {
    if (!last_filename_.empty())
      return false;
    return ParseVmsFilename(columns[0], &last_filename_, &last_is_directory_);
  }

  // Recognize listing entries which generate "access denied" message even when
  // trying to list them. We don't display them in the final listing.
  static const char* kAccessDeniedMessages[] = {
    "%RMS-E-PRV",
    "privilege",
  };
  for (size_t i = 0; i < arraysize(kAccessDeniedMessages); i++) {
    if (line.find(ASCIIToUTF16(kAccessDeniedMessages[i])) != string16::npos) {
      last_filename_.clear();
      last_is_directory_ = false;
      return true;
    }
  }

  string16 filename;
  bool is_directory = false;
  if (last_filename_.empty()) {
    if (!ParseVmsFilename(columns[0], &filename, &is_directory))
      return false;
    columns.erase(columns.begin());
  } else {
    filename = last_filename_;
    is_directory = last_is_directory_;
    last_filename_.clear();
    last_is_directory_ = false;
  }

  if (columns.size() > 5)
    return false;

  if (columns.size() == 5) {
    if (!LooksLikeVmsFileProtectionListing(columns[4]))
      return false;
    if (!LooksLikeVmsUserIdentificationCode(columns[3]))
      return false;
    columns.resize(3);
  }

  if (columns.size() != 3)
    return false;

  FtpDirectoryListingEntry entry;
  entry.name = filename;
  entry.type = is_directory ? FtpDirectoryListingEntry::DIRECTORY
                            : FtpDirectoryListingEntry::FILE;
  if (!ParseVmsFilesize(columns[0], &entry.size))
    return false;
  if (entry.size < 0)
    return false;
  if (entry.type != FtpDirectoryListingEntry::FILE)
    entry.size = -1;
  if (!VmsDateListingToTime(columns, &entry.last_modified))
    return false;

  entries_.push(entry);
  return true;
}

}  // namespace net
