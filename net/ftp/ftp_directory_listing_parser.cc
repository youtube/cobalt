// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser.h"

#include "base/i18n/icu_encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/stl_util-inl.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/ftp/ftp_directory_listing_parser_ls.h"
#include "net/ftp/ftp_directory_listing_parser_netware.h"
#include "net/ftp/ftp_directory_listing_parser_vms.h"
#include "net/ftp/ftp_directory_listing_parser_windows.h"
#include "net/ftp/ftp_server_type_histograms.h"

namespace {

// Converts a string with unknown character encoding to UTF-16. On success
// fills in |converted_text| and |encoding|. Returns network error code.
int ConvertStringToUTF16(const std::string& text,
                         string16* converted_text,
                         std::string* encoding) {
  std::vector<std::string> encodings;
  if (!base::DetectAllEncodings(text, &encodings))
    return net::ERR_ENCODING_DETECTION_FAILED;

  // Use first encoding that can be used to decode the text.
  for (size_t i = 0; i < encodings.size(); i++) {
    if (base::CodepageToUTF16(text,
                              encodings[i].c_str(),
                              base::OnStringConversionError::FAIL,
                              converted_text)) {
      *encoding = encodings[i];
      return net::OK;
    }
  }

  return net::ERR_ENCODING_DETECTION_FAILED;
}

int FillInRawName(const std::string& encoding,
                  std::vector<net::FtpDirectoryListingEntry>* entries) {
  for (size_t i = 0; i < entries->size(); i++) {
    if (!base::UTF16ToCodepage(entries->at(i).name, encoding.c_str(),
                               base::OnStringConversionError::FAIL,
                               &entries->at(i).raw_name)) {
      return net::ERR_ENCODING_CONVERSION_FAILED;
    }
  }

  return net::OK;
}

}  // namespace

namespace net {

int ParseFtpDirectoryListing(const std::string& text,
                             const base::Time& current_time,
                             std::vector<FtpDirectoryListingEntry>* entries) {
  std::string encoding;

  string16 converted_text;
  int rv = ConvertStringToUTF16(text, &converted_text, &encoding);
  if (rv != OK)
    return rv;

  std::vector<string16> lines;
  base::SplitString(converted_text, '\n', &lines);

  // TODO(phajdan.jr): Use a table of callbacks instead of repeating code.

  entries->clear();
  if (ParseFtpDirectoryListingLs(lines, current_time, entries)) {
    UpdateFtpServerTypeHistograms(SERVER_LS);
    return FillInRawName(encoding, entries);
  }

  entries->clear();
  if (ParseFtpDirectoryListingWindows(lines, entries)) {
    UpdateFtpServerTypeHistograms(SERVER_WINDOWS);
    return FillInRawName(encoding, entries);
  }

  entries->clear();
  if (ParseFtpDirectoryListingVms(lines, entries)) {
    UpdateFtpServerTypeHistograms(SERVER_VMS);
    return FillInRawName(encoding, entries);
  }

  entries->clear();
  if (ParseFtpDirectoryListingNetware(lines, current_time, entries)) {
    UpdateFtpServerTypeHistograms(SERVER_NETWARE);
    return FillInRawName(encoding, entries);
  }

  entries->clear();
  UpdateFtpServerTypeHistograms(SERVER_UNKNOWN);
  return ERR_UNRECOGNIZED_FTP_DIRECTORY_LISTING_FORMAT;
}

}  // namespace
