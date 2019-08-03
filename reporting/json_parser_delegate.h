// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_JSON_PARSER_DELEGATE_H_
#define NET_REPORTING_JSON_PARSER_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "net/base/net_export.h"

namespace base {
class Value;
}  // namespace base

namespace net {

// Used by the Reporting API implementation to parse JSON policy headers safely.
// Where possible, Reporting embedders should use data_decoder's SafeJsonParser,
// since the policy headers are untrusted content.
class NET_EXPORT JSONParserDelegate {
 public:
  using SuccessCallback =
      base::OnceCallback<void(std::unique_ptr<base::Value>)>;
  using FailureCallback = base::OnceCallback<void()>;

  JSONParserDelegate() = default;
  virtual ~JSONParserDelegate() = default;

  // Parses JSON. How safely, and using what mechanism, is up to the embedder,
  // but //components/data_decoder is recommended if available.
  //
  // Exactly one callback should be made, either to |success_callback| (with the
  // parsed value) if parsing succeeded or to |failure_callback| if parsing
  // failed.  The callbacks may be called either synchronously or
  // asynchronously.
  virtual void ParseJson(const std::string& unsafe_json,
                         SuccessCallback success_callback,
                         FailureCallback failure_callback) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(JSONParserDelegate);
};

// A JSONParserDelegate that parses JSON in-process using JSONReader.
//
// TODO(crbug.com/892148): Replace this with an implementation that uses
// //components/data_decoder, and only allow in-process parsing during tests.
class NET_EXPORT InProcessJSONParser : public JSONParserDelegate {
 public:
  InProcessJSONParser() = default;

  void ParseJson(const std::string& unsafe_json,
                 SuccessCallback success_callback,
                 FailureCallback failure_callback) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InProcessJSONParser);
};

}  // namespace net

#endif  // NET_REPORTING_JSON_PARSER_DELEGATE_H_
