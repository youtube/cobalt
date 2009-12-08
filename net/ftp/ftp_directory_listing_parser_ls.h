// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_PARSER_LS_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_PARSER_LS_H_

#include <queue>

#include "net/ftp/ftp_directory_listing_parser.h"

namespace net {

// Parser for "ls -l"-style directory listing.
class FtpDirectoryListingParserLs : public FtpDirectoryListingParser {
 public:
  FtpDirectoryListingParserLs();

  // FtpDirectoryListingParser methods:
  virtual FtpServerType GetServerType() const { return SERVER_LS; }
  virtual bool ConsumeLine(const string16& line);
  virtual bool OnEndOfInput();
  virtual bool EntryAvailable() const;
  virtual FtpDirectoryListingEntry PopEntry();

 private:
  bool received_nonempty_line_;

  // True after we have received a "total n" listing header, where n is an
  // integer. Only one such header is allowed per listing.
  bool received_total_line_;

  // There is a variant of the listing served by wu-ftpd which doesn't contain
  // the "number of links" column (the second column in a "standard" ls -l
  // listing). Store an offset to reference later columns.
  int column_offset_;

  std::queue<FtpDirectoryListingEntry> entries_;

  DISALLOW_COPY_AND_ASSIGN(FtpDirectoryListingParserLs);
};

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_PARSER_LS_H_
