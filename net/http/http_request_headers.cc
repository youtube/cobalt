// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_request_headers.h"

#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "net/http/http_util.h"

namespace net {

const char HttpRequestHeaders::kGetMethod[] = "GET";
const char HttpRequestHeaders::kAcceptCharset[] = "Accept-Charset";
const char HttpRequestHeaders::kAcceptEncoding[] = "Accept-Encoding";
const char HttpRequestHeaders::kAcceptLanguage[] = "Accept-Language";
const char HttpRequestHeaders::kCacheControl[] = "Cache-Control";
const char HttpRequestHeaders::kConnection[] = "Connection";
const char HttpRequestHeaders::kContentLength[] = "Content-Length";
const char HttpRequestHeaders::kContentType[] = "Content-Type";
const char HttpRequestHeaders::kCookie[] = "Cookie";
const char HttpRequestHeaders::kHost[] = "Host";
const char HttpRequestHeaders::kIfModifiedSince[] = "If-Modified-Since";
const char HttpRequestHeaders::kIfNoneMatch[] = "If-None-Match";
const char HttpRequestHeaders::kIfRange[] = "If-Range";
const char HttpRequestHeaders::kOrigin[] = "Origin";
const char HttpRequestHeaders::kPragma[] = "Pragma";
const char HttpRequestHeaders::kProxyConnection[] = "Proxy-Connection";
const char HttpRequestHeaders::kRange[] = "Range";
const char HttpRequestHeaders::kReferer[] = "Referer";
const char HttpRequestHeaders::kUserAgent[] = "User-Agent";

HttpRequestHeaders::HeaderKeyValuePair::HeaderKeyValuePair() {
}

HttpRequestHeaders::HeaderKeyValuePair::HeaderKeyValuePair(
    const base::StringPiece& key, const base::StringPiece& value)
    : key(key.data(), key.size()), value(value.data(), value.size()) {
}


HttpRequestHeaders::Iterator::Iterator(const HttpRequestHeaders& headers)
    : started_(false),
      curr_(headers.headers_.begin()),
      end_(headers.headers_.end()) {}

HttpRequestHeaders::Iterator::~Iterator() {}

bool HttpRequestHeaders::Iterator::GetNext() {
  if (!started_) {
    started_ = true;
    return curr_ != end_;
  }

  if (curr_ == end_)
    return false;

  ++curr_;
  return curr_ != end_;
}

HttpRequestHeaders::HttpRequestHeaders() {}
HttpRequestHeaders::~HttpRequestHeaders() {}

bool HttpRequestHeaders::GetHeader(const base::StringPiece& key,
                                   std::string* out) const {
  HeaderVector::const_iterator it = FindHeader(key);
  if (it == headers_.end())
    return false;
  out->assign(it->value);
  return true;
}

void HttpRequestHeaders::Clear() {
  headers_.clear();
}

void HttpRequestHeaders::SetHeaderIfMissing(const base::StringPiece& key,
                                            const base::StringPiece& value) {
  HeaderVector::iterator it = FindHeader(key);
  if (it == headers_.end())
    headers_.push_back(HeaderKeyValuePair(key.as_string(), value.as_string()));
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

    if (header_value_begin == header_value_end) {
      // Value was all LWS.
      SetHeader(header_key, "");
    } else {
      SetHeader(header_key,
                base::StringPiece(&*header_value_begin,
                                  header_value_end - header_value_begin));
    }
  } else if (value_index == header_line.size()) {
    SetHeader(header_key, "");
  } else {
    NOTREACHED();
  }
}

void HttpRequestHeaders::AddHeadersFromString(
    const base::StringPiece& headers) {
  // TODO(willchan): Consider adding more StringPiece support in string_util.h
  // to eliminate copies.
  std::vector<std::string> header_line_vector;
  base::SplitStringUsingSubstr(headers.as_string(), "\r\n",
                               &header_line_vector);
  for (std::vector<std::string>::const_iterator it = header_line_vector.begin();
       it != header_line_vector.end(); ++it) {
    if (!it->empty())
      AddHeaderFromString(*it);
  }
}

void HttpRequestHeaders::MergeFrom(const HttpRequestHeaders& other) {
  for (HeaderVector::const_iterator it = other.headers_.begin();
       it != other.headers_.end(); ++it ) {
    SetHeader(it->key, it->value);
  }
}

std::string HttpRequestHeaders::ToString() const {
  std::string output;
  for (HeaderVector::const_iterator it = headers_.begin();
       it != headers_.end(); ++it) {
    if (!it->value.empty()) {
      base::StringAppendF(&output, "%s: %s\r\n",
                          it->key.c_str(), it->value.c_str());
    } else {
      base::StringAppendF(&output, "%s:\r\n", it->key.c_str());
    }
  }
  output.append("\r\n");
  return output;
}

HttpRequestHeaders::HeaderVector::iterator
HttpRequestHeaders::FindHeader(const base::StringPiece& key) {
  for (HeaderVector::iterator it = headers_.begin();
       it != headers_.end(); ++it) {
    if (key.length() == it->key.length() &&
        !base::strncasecmp(key.data(), it->key.data(), key.length()))
      return it;
  }

  return headers_.end();
}

HttpRequestHeaders::HeaderVector::const_iterator
HttpRequestHeaders::FindHeader(const base::StringPiece& key) const {
  for (HeaderVector::const_iterator it = headers_.begin();
       it != headers_.end(); ++it) {
    if (key.length() == it->key.length() &&
        !base::strncasecmp(key.data(), it->key.data(), key.length()))
      return it;
  }

  return headers_.end();
}

}  // namespace net
