// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_PARSER_WINDOWS_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_PARSER_WINDOWS_H_

#include <queue>

#include "net/ftp/ftp_directory_listing_parser.h"

namespace net {

class FtpDirectoryListingParserWindows : public FtpDirectoryListingParser {
 public:
  FtpDirectoryListingParserWindows();

  // FtpDirectoryListingParser methods:
  virtual FtpServerType GetServerType() const { return SERVER_WINDOWS; }
  virtual bool ConsumeLine(const string16& line);
  virtual bool OnEndOfInput();
  virtual bool EntryAvailable() const;
  virtual FtpDirectoryListingEntry PopEntry();

 private:
  std::queue<FtpDirectoryListingEntry> entries_;

  DISALLOW_COPY_AND_ASSIGN(FtpDirectoryListingParserWindows);
};

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_PARSER_WINDOWS_H_
