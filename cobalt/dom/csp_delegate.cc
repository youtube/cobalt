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

#include "base/bind.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

CspDelegate::CspDelegate(scoped_ptr<CspViolationReporter> violation_reporter,
                         const GURL& url,
                         const std::string& default_security_policy,
                         EnforcementType mode) {
  default_security_policy_ = default_security_policy;
  was_header_received_ = false;
  enforcement_mode_ = mode;

  reporter_ = violation_reporter.Pass();
  csp::ViolationCallback violation_callback;
  if (reporter_) {
    violation_callback = base::Bind(&CspViolationReporter::Report,
                                    base::Unretained(reporter_.get()));
  }
  csp_.reset(new csp::ContentSecurityPolicy(url, violation_callback));
  SetLocationPolicy(default_security_policy_);
}

CspDelegate::~CspDelegate() {}

bool CspDelegate::CanLoad(ResourceType type, const GURL& url,
                          bool did_redirect) const {
#if !defined(COBALT_FORCE_CSP)
  if (enforcement_mode() == kEnforcementDisable) {
    return true;
  }
#endif  // !defined(COBALT_FORCE_CSP)

  const csp::ContentSecurityPolicy::RedirectStatus redirect_status =
      did_redirect ? csp::ContentSecurityPolicy::kDidRedirect
                   : csp::ContentSecurityPolicy::kDidNotRedirect;

  // Special case for "offline" mode- in the absence of any server policy,
  // we check our default navigation policy, to permit navigation to
  // and from the main site and error pages, and disallow everything else.
  if (enforcement_mode() == kEnforcementRequire && !was_header_received_) {
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

bool CspDelegate::IsValidNonce(ResourceType type,
                               const std::string& nonce) const {
#if !defined(COBALT_FORCE_CSP)
  if (enforcement_mode() == kEnforcementDisable) {
    return true;
  }
#endif  // !defined(COBALT_FORCE_CSP)

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

bool CspDelegate::AllowInline(ResourceType type,
                              const base::SourceLocation& location,
                              const std::string& content) const {
#if !defined(COBALT_FORCE_CSP)
  if (enforcement_mode() == kEnforcementDisable) {
    return true;
  }
#endif  // !defined(COBALT_FORCE_CSP)

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

bool CspDelegate::OnReceiveHeaders(const csp::ResponseHeaders& headers) {
  if (enforcement_mode() == kEnforcementRequire &&
      headers.content_security_policy().empty()) {
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
  return true;
}

void CspDelegate::OnReceiveHeader(const std::string& header,
                                  csp::HeaderType header_type,
                                  csp::HeaderSource header_source) {
  csp_->OnReceiveHeader(header, header_type, header_source);
  // In "require" mode we don't consider a Report-Only header to be sufficient.
  if (enforcement_mode() == kEnforcementRequire) {
    if (header_type == csp::kHeaderTypeEnforce) {
      was_header_received_ = true;
    }
  } else {
    was_header_received_ = true;
  }
}

void CspDelegate::SetLocationPolicy(const std::string& policy) {
  if (!policy.length()) {
    return;
  }

  if (policy.find(csp::ContentSecurityPolicy::kLocationSrc) ==
      std::string::npos) {
    LOG(FATAL) << csp::ContentSecurityPolicy::kLocationSrc << " not found in "
               << policy;
  }
  csp_->SetNavigationFallbackPolicy(policy);
}

}  // namespace dom
}  // namespace cobalt
