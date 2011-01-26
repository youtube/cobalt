// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_PARSER_VMS_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_PARSER_VMS_H_
#pragma once

#include <queue>

#include "net/ftp/ftp_directory_listing_parser.h"

namespace net {

// Parser for VMS-style directory listing (including variants).
class FtpDirectoryListingParserVms : public FtpDirectoryListingParser {
 public:
  FtpDirectoryListingParserVms();
  virtual ~FtpDirectoryListingParserVms();

  // FtpDirectoryListingParser methods:
  virtual FtpServerType GetServerType() const;
  virtual bool ConsumeLine(const string16& line);
  virtual bool OnEndOfInput();
  virtual bool EntryAvailable() const;
  virtual FtpDirectoryListingEntry PopEntry();

 private:
  enum State {
    STATE_INITIAL,

    // Indicates that we have received the header, like this:
    // Directory SYS$SYSDEVICE:[ANONYMOUS]
    STATE_RECEIVED_HEADER,

    // Indicates that we have received the first listing entry, like this:
    // MADGOAT.DIR;1              2   9-MAY-2001 22:23:44.85
    STATE_ENTRIES,

    // Indicates that we have received the last listing entry.
    STATE_RECEIVED_LAST_ENTRY,

    // Indicates that we have successfully received all parts of the listing.
    STATE_END,
  };

  // Consumes listing line which is expected to be a directory listing entry
  // (and not a comment etc). Returns true on success.
  bool ConsumeEntryLine(const string16& line);

  State state_;

  // VMS can use two physical lines if the filename is long. The first line will
  // contain the filename, and the second line everything else. Store the
  // filename until we receive the next line.
  string16 last_filename_;
  bool last_is_directory_;

  std::queue<FtpDirectoryListingEntry> entries_;

  DISALLOW_COPY_AND_ASSIGN(FtpDirectoryListingParserVms);
};

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_PARSER_VMS_H_
