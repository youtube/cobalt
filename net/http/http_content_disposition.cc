// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_content_disposition.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/net_util.h"
#include "net/http/http_util.h"

namespace net {

HttpContentDisposition::HttpContentDisposition(
    const std::string& header, const std::string& referrer_charset)
  : type_(INLINE) {
  Parse(header, referrer_charset);
}

HttpContentDisposition::~HttpContentDisposition() {
}

std::string::const_iterator HttpContentDisposition::ConsumeDispositionType(
    std::string::const_iterator begin, std::string::const_iterator end) {
  DCHECK(type_ == INLINE);
  std::string::const_iterator delimiter = std::find(begin, end, ';');

  std::string::const_iterator type_begin = begin;
  std::string::const_iterator type_end = delimiter;
  HttpUtil::TrimLWS(&type_begin, &type_end);

  // If the disposition-type isn't a valid token the then the
  // Content-Disposition header is malformed, and we treat the first bytes as
  // a parameter rather than a disposition-type.
  if (!HttpUtil::IsToken(type_begin, type_end))
    return begin;

  DCHECK(std::find(type_begin, type_end, '=') == type_end);

  if (!LowerCaseEqualsASCII(type_begin, type_end, "inline"))
    type_ = ATTACHMENT;
  return delimiter;
}

// http://tools.ietf.org/html/rfc6266
//
//  content-disposition = "Content-Disposition" ":"
//                         disposition-type *( ";" disposition-parm )
//
//  disposition-type    = "inline" | "attachment" | disp-ext-type
//                      ; case-insensitive
//  disp-ext-type       = token
//
//  disposition-parm    = filename-parm | disp-ext-parm
//
//  filename-parm       = "filename" "=" value
//                      | "filename*" "=" ext-value
//
//  disp-ext-parm       = token "=" value
//                      | ext-token "=" ext-value
//  ext-token           = <the characters in token, followed by "*">
//
void HttpContentDisposition::Parse(const std::string& header,
                                   const std::string& referrer_charset) {
  DCHECK(type_ == INLINE);
  DCHECK(filename_.empty());

  std::string::const_iterator pos = header.begin();
  std::string::const_iterator end = header.end();
  pos = ConsumeDispositionType(pos, end);

  std::string name;
  std::string filename;
  std::string ext_filename;

  HttpUtil::NameValuePairsIterator iter(pos, end, ';');
  while (iter.GetNext()) {
    if (filename.empty() && LowerCaseEqualsASCII(iter.name_begin(),
                                                 iter.name_end(),
                                                 "filename")) {
      DecodeFilenameValue(iter.value(), referrer_charset, &filename);
    } else if (name.empty() && LowerCaseEqualsASCII(iter.name_begin(),
                                                    iter.name_end(),
                                                    "name")) {
      DecodeFilenameValue(iter.value(), referrer_charset, &name);
    } else if (ext_filename.empty() && LowerCaseEqualsASCII(iter.name_begin(),
                                                            iter.name_end(),
                                                            "filename*")) {
      DecodeExtValue(iter.raw_value(), &ext_filename);
    }
  }

  if (!ext_filename.empty())
    filename_ = ext_filename;
  else if (!filename.empty())
    filename_ = filename;
  else
    filename_ = name;
}

}  // namespace net
