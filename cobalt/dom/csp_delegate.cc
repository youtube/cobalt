/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/csp_delegate.h"

#include "cobalt/dom/document.h"

namespace cobalt {
namespace dom {

CSPDelegate::CSPDelegate(Document* document) : document_(document) {
  csp_.reset(new csp::ContentSecurityPolicy());
  csp_->BindDelegate(this);
}

CSPDelegate::~CSPDelegate() {}

csp::ContentSecurityPolicy* CSPDelegate::csp() const { return csp_.get(); }

bool CSPDelegate::CanLoadImage(const GURL& url) const {
  return csp_->AllowImageFromSource(url);
}

GURL CSPDelegate::url() const { return document_->url_as_gurl(); }

void CSPDelegate::ReportViolation(const std::string& directive_text,
                                  const std::string& effective_directive,
                                  const std::string& console_message,
                                  const GURL& blocked_url,
                                  const std::vector<std::string>& endpoints,
                                  const std::string& header) {
  // TODO(***REMOVED***): Actually send this data to the endpoints.
  DLOG(INFO) << "Directive text: " << directive_text;
  DLOG(INFO) << "Effective directive: " << effective_directive;
  DLOG(INFO) << "Console message: " << console_message;
  DLOG(INFO) << "Blocked URL: " << blocked_url;
  DLOG(INFO) << "Header: " << header;
  UNREFERENCED_PARAMETER(endpoints);
  NOTIMPLEMENTED();
}

void CSPDelegate::SetReferrerPolicy(csp::ReferrerPolicy policy) {
  // TODO(***REMOVED***): Set the referrer policy on the document.
  UNREFERENCED_PARAMETER(policy);
  NOTIMPLEMENTED();
}

}  // namespace dom
}  // namespace cobalt
