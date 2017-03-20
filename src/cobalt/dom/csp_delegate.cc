// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/dom/csp_delegate.h"

#include "base/bind.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

CspDelegate::CspDelegate() {}
CspDelegate::~CspDelegate() {}

CspDelegateSecure::CspDelegateSecure(
    scoped_ptr<CspViolationReporter> violation_reporter, const GURL& url,
    const std::string& location_policy,
    const base::Closure& policy_changed_callback) {
  location_policy_ = location_policy;
  was_header_received_ = false;
  policy_changed_callback_ = policy_changed_callback;

  reporter_ = violation_reporter.Pass();
  csp::ViolationCallback violation_callback;
  if (reporter_) {
    violation_callback = base::Bind(&CspViolationReporter::Report,
                                    base::Unretained(reporter_.get()));
  }
  csp_.reset(new csp::ContentSecurityPolicy(url, violation_callback));
  SetLocationPolicy(location_policy_);
}

CspDelegateSecure::~CspDelegateSecure() {}

bool CspDelegateSecure::CanLoad(ResourceType type, const GURL& url,
                                bool did_redirect) const {
  const csp::ContentSecurityPolicy::RedirectStatus redirect_status =
      did_redirect ? csp::ContentSecurityPolicy::kDidRedirect
                   : csp::ContentSecurityPolicy::kDidNotRedirect;

  // Special case for "offline" mode- in the absence of any server policy,
  // we check our default navigation policy, to permit navigation to
  // and from the main site and error pages, and disallow everything else.
  if (!was_header_received_) {
    if (type == kLocation) {
      return csp_->AllowNavigateToSource(url, redirect_status);
    } else {
      return false;
    }
  }

  bool can_load = false;
  switch (type) {
    case kFont:
      can_load = csp_->AllowFontFromSource(url, redirect_status);
      break;
    case kImage:
      can_load = csp_->AllowImageFromSource(url, redirect_status);
      break;
    case kLocation:
      can_load = csp_->AllowNavigateToSource(url, redirect_status);
      break;
    case kMedia:
      can_load = csp_->AllowMediaFromSource(url, redirect_status);
      break;
    case kScript:
      can_load = csp_->AllowScriptFromSource(url, redirect_status);
      break;
    case kStyle:
      can_load = csp_->AllowStyleFromSource(url, redirect_status);
      break;
    case kXhr:
      can_load = csp_->AllowConnectToSource(url, redirect_status);
      break;
    default:
      NOTREACHED() << "Invalid resource type " << type;
      break;
  }
  return can_load;
}

bool CspDelegateSecure::IsValidNonce(ResourceType type,
                                     const std::string& nonce) const {
  bool is_valid = false;
  if (type == kScript) {
    is_valid = csp_->AllowScriptWithNonce(nonce);
  } else if (type == kStyle) {
    is_valid = csp_->AllowStyleWithNonce(nonce);
  } else {
    NOTREACHED() << "Invalid resource type " << type;
  }
  return is_valid;
}

bool CspDelegateSecure::AllowInline(ResourceType type,
                                    const base::SourceLocation& location,
                                    const std::string& content) const {
  bool can_load = false;
  if (type == kScript) {
    can_load = csp_->AllowInlineScript(location.file_path, location.line_number,
                                       content);
  } else if (type == kStyle) {
    can_load = csp_->AllowInlineStyle(location.file_path, location.line_number,
                                      content);
  } else {
    NOTREACHED() << "Invalid resource type" << type;
  }
  return can_load;
}

bool CspDelegateSecure::AllowEval(std::string* eval_disabled_message) const {
  bool allow_eval =
      csp_->AllowEval(csp::ContentSecurityPolicy::kSuppressReport);
  if (!allow_eval && eval_disabled_message) {
    *eval_disabled_message = csp_->disable_eval_error_message();
  }
  return allow_eval;
}

void CspDelegateSecure::ReportEval() const {
  csp_->AllowEval(csp::ContentSecurityPolicy::kSendReport);
}

bool CspDelegateSecure::OnReceiveHeaders(const csp::ResponseHeaders& headers) {
  if (headers.content_security_policy().empty()) {
    // Didn't find Content-Security-Policy header.
    if (!headers.content_security_policy_report_only().empty()) {
      DLOG(INFO)
          << "Content-Security-Policy-Report-Only headers were "
             "received, but Content-Security-Policy headers are required.";
    }
    return false;
  }
  csp_->OnReceiveHeaders(headers);
  was_header_received_ = true;
  if (!policy_changed_callback_.is_null()) {
    policy_changed_callback_.Run();
  }
  return true;
}

void CspDelegateSecure::OnReceiveHeader(const std::string& header,
                                        csp::HeaderType header_type,
                                        csp::HeaderSource header_source) {
  if (header_source == csp::kHeaderSourceMetaOutsideHead) {
    csp_->ReportMetaOutsideHead(header);
  } else {
    csp_->OnReceiveHeader(header, header_type, header_source);
    // We don't consider a Report-Only header to be sufficient.
    if (header_type == csp::kHeaderTypeEnforce) {
      was_header_received_ = true;
    }

    if (!policy_changed_callback_.is_null()) {
      policy_changed_callback_.Run();
    }
  }
}

void CspDelegateSecure::SetLocationPolicy(const std::string& policy) {
  if (!policy.length()) {
    return;
  }

  if (policy.find(csp::ContentSecurityPolicy::kLocationSrc) ==
      std::string::npos) {
    LOG(FATAL) << csp::ContentSecurityPolicy::kLocationSrc << " not found in "
               << policy;
  }
  csp_->SetNavigationPolicy(policy);
}

}  // namespace dom
}  // namespace cobalt
