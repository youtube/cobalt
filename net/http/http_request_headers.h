// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpRequestHeaders manages the request headers (including the request line).
// It maintains these in a vector of header key/value pairs, thereby maintaining
// the order of the headers.  This means that any lookups are linear time
// operations.

#ifndef NET_HTTP_HTTP_REQUEST_HEADERS_H_
#define NET_HTTP_HTTP_REQUEST_HEADERS_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/string_piece.h"

namespace net {

class HttpRequestHeaders {
 public:
  static const char kGetMethod[];

  static const char kCacheControl[];
  static const char kConnection[];
  static const char kContentLength[];
  static const char kHost[];
  static const char kPragma[];
  static const char kProxyConnection[];
  static const char kReferer[];
  static const char kUserAgent[];

  HttpRequestHeaders();
  ~HttpRequestHeaders();

  void SetRequestLine(const base::StringPiece& method,
                      const base::StringPiece& path,
                      const base::StringPiece& version);

  // Sets the header value pair for |key| and |value|.  If |key| already exists,
  // then the header value is modified, but the key is untouched, and the order
  // in the vector remains the same.  When comparing |key|, case is ignored.
  void SetHeader(const base::StringPiece& key, const base::StringPiece& value);

  // Removes the first header that matches (case insensitive) |key|.
  void RemoveHeader(const base::StringPiece& key);

  // Parses the header from a string and calls SetHeader() with it.  This string
  // should not contain any CRLF.  As per RFC2616, the format is:
  //
  // message-header = field-name ":" [ field-value ]
  // field-name     = token
  // field-value    = *( field-content | LWS )
  // field-content  = <the OCTETs making up the field-value
  //                  and consisting of either *TEXT or combinations
  //                  of token, separators, and quoted-string>
  //
  // AddHeaderFromString() will trim any LWS surrounding the
  // field-content.
  void AddHeaderFromString(const base::StringPiece& header_line);

  // Calls SetHeader() on each header from |other|, maintaining order.
  void MergeFrom(const HttpRequestHeaders& other);

  // Serializes HttpRequestHeaders to a string representation.  Joins all the
  // header keys and values with ": ", and inserts "\r\n" between each header
  // line, and adds the trailing "\r\n".
  std::string ToString() const;

 private:
  struct HeaderKeyValuePair {
    HeaderKeyValuePair() {}
    HeaderKeyValuePair(const base::StringPiece& key,
                       const base::StringPiece& value)
        : key(key.data(), key.size()), value(value.data(), value.size()) {}

    std::string key;
    std::string value;
  };

  typedef std::vector<HeaderKeyValuePair> HeaderVector;

  HeaderVector::iterator FindHeader(const base::StringPiece& key);
  HeaderVector::const_iterator FindHeader(const base::StringPiece& key) const;

  std::string method_;
  std::string path_;
  std::string version_;

  HeaderVector headers_;

  DISALLOW_COPY_AND_ASSIGN(HttpRequestHeaders);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_REQUEST_HEADERS_H_
