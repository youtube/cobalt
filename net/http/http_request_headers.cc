// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_request_headers.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "net/http/http_util.h"

namespace net {

const char HttpRequestHeaders::kGetMethod[] = "GET";

const char HttpRequestHeaders::kCacheControl[] = "Cache-Control";
const char HttpRequestHeaders::kConnection[] = "Connection";
const char HttpRequestHeaders::kContentLength[] = "Content-Length";
const char HttpRequestHeaders::kHost[] = "Host";
const char HttpRequestHeaders::kPragma[] = "Pragma";
const char HttpRequestHeaders::kProxyConnection[] = "Proxy-Connection";
const char HttpRequestHeaders::kReferer[] = "Referer";
const char HttpRequestHeaders::kUserAgent[] = "User-Agent";

HttpRequestHeaders::HttpRequestHeaders() {}
HttpRequestHeaders::~HttpRequestHeaders() {}

void HttpRequestHeaders::SetRequestLine(const base::StringPiece& method,
                                        const base::StringPiece& path,
                                        const base::StringPiece& version) {
  DCHECK(!method.empty());
  DCHECK(!path.empty());
  DCHECK(!version.empty());

  method_.assign(method.data(), method.length());
  path_.assign(path.data(), path.length());
  version_.assign(version.data(), version.length());
}

void HttpRequestHeaders::SetHeader(const base::StringPiece& key,
                                   const base::StringPiece& value) {
  HeaderVector::iterator it = FindHeader(key);
  if (it != headers_.end())
    it->value = value.as_string();
  else
    headers_.push_back(HeaderKeyValuePair(key.as_string(), value.as_string()));
}

void HttpRequestHeaders::RemoveHeader(const base::StringPiece& key) {
  HeaderVector::iterator it = FindHeader(key);
  if (it != headers_.end())
    headers_.erase(it);
}

void HttpRequestHeaders::AddHeaderFromString(
    const base::StringPiece& header_line) {
  DCHECK_EQ(std::string::npos, header_line.find("\r\n"))
      << "\"" << header_line << "\" contains CRLF.";

  const std::string::size_type key_end_index = header_line.find(":");
  if (key_end_index == std::string::npos) {
    LOG(DFATAL) << "\"" << header_line << "\" is missing colon delimiter.";
    return;
  }

  if (key_end_index == 0) {
    LOG(DFATAL) << "\"" << header_line << "\" is missing header key.";
    return;
  }

  const base::StringPiece header_key(header_line.data(), key_end_index);

  const std::string::size_type value_index = key_end_index + 1;

  if (value_index < header_line.size()) {
    std::string header_value(header_line.data() + value_index,
                             header_line.size() - value_index);
    std::string::const_iterator header_value_begin =
        header_value.begin();
    std::string::const_iterator header_value_end =
        header_value.end();
    HttpUtil::TrimLWS(&header_value_begin, &header_value_end);
    SetHeader(header_key,
              base::StringPiece(&*header_value_begin,
                                header_value_end - header_value_begin));
  } else if (value_index == header_line.size()) {
    SetHeader(header_key, "");
  } else {
    NOTREACHED();
  }
}

void HttpRequestHeaders::MergeFrom(const HttpRequestHeaders& other) {
  DCHECK(other.method_.empty());
  DCHECK(other.path_.empty());
  DCHECK(other.version_.empty());

  for (HeaderVector::const_iterator it = other.headers_.begin();
       it != other.headers_.end(); ++it ) {
    SetHeader(it->key, it->value);
  }
}

std::string HttpRequestHeaders::ToString() const {
  std::string output;
  if (!method_.empty()) {
    DCHECK(!path_.empty());
    DCHECK(!version_.empty());
    output = StringPrintf(
        "%s %s HTTP/%s\r\n", method_.c_str(), path_.c_str(), version_.c_str());
  }
  for (HeaderVector::const_iterator it = headers_.begin();
       it != headers_.end(); ++it) {
    if (!it->value.empty())
      StringAppendF(&output, "%s: %s\r\n", it->key.c_str(), it->value.c_str());
    else
      StringAppendF(&output, "%s:\r\n", it->key.c_str());
  }
  output.append("\r\n");
  return output;
}

HttpRequestHeaders::HeaderVector::iterator
HttpRequestHeaders::FindHeader(const base::StringPiece& key) {
  for (HeaderVector::iterator it = headers_.begin();
       it != headers_.end(); ++it) {
    if (!base::strncasecmp(key.data(), it->key.data(), key.length()))
      return it;
  }

  return headers_.end();
}

}  // namespace net
