// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_CONTENT_DISPOSITION_H_
#define NET_HTTP_HTTP_CONTENT_DISPOSITION_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "net/base/net_export.h"

namespace net {

class NET_EXPORT HttpContentDisposition {
 public:
  enum Type {
    INLINE,
    ATTACHMENT,
  };

  HttpContentDisposition(const std::string& header,
                         const std::string& referrer_charset);
  ~HttpContentDisposition();

  bool is_attachment() const { return type() == ATTACHMENT; }

  Type type() const { return type_; }
  const std::string& filename() const { return filename_; }

 private:
  void Parse(const std::string& header, const std::string& referrer_charset);
  std::string::const_iterator ConsumeDispositionType(
      std::string::const_iterator begin, std::string::const_iterator end);

  Type type_;
  std::string filename_;

  DISALLOW_COPY_AND_ASSIGN(HttpContentDisposition);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_CONTENT_DISPOSITION_H_
