// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSP_CONTENT_SECURITY_POLICY_H_
#define COBALT_CSP_CONTENT_SECURITY_POLICY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "cobalt/csp/parsers.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace cobalt {
namespace csp {

class DirectiveList;
class Source;

// Wrap up information about a CSP violation, for passing to the Delegate.
struct ViolationInfo {
  std::string directive_text;
  std::string effective_directive;
  std::string console_message;
  GURL blocked_url;
  std::vector<std::string> endpoints;
  std::string header;
};

// Whether Cobalt can start without CSP headers.
enum CSPHeaderPolicy {
  kCSPRequired,
  kCSPOptional,
};

// A callback that a URL fetcher will call to check if the URL is permitted
// by our security policy. This may be called multiple times if the URL results
// in a redirect. The callback should return |true| if the URL is safe to
// load.
typedef base::Callback<bool(const GURL& url, bool did_redirect)>
    SecurityCallback;
typedef base::Callback<void(const ViolationInfo&)> ViolationCallback;

// Extract CSP-related headers from HttpResponseHeaders.
class ResponseHeaders {
 public:
  ResponseHeaders() {}
  explicit ResponseHeaders(
      const scoped_refptr<net::HttpResponseHeaders>& response);

  const std::string& content_security_policy() const {
    return content_security_policy_;
  }
  const std::string& content_security_policy_report_only() const {
    return content_security_policy_report_only_;
  }

 private:
  std::string content_security_policy_;
  std::string content_security_policy_report_only_;
};

class ContentSecurityPolicy {
 public:
  typedef std::vector<std::unique_ptr<DirectiveList>> PolicyList;

  // CSP Level 1 Directives
  static const char kConnectSrc[];
  static const char kDefaultSrc[];
  static const char kFontSrc[];
  static const char kFrameSrc[];
  static const char kImgSrc[];
  static const char kMediaSrc[];
  static const char kObjectSrc[];
  static const char kReportURI[];
  static const char kSandbox[];
  static const char kScriptSrc[];
  static const char kStyleSrc[];

  // CSP Level 2 Directives
  static const char kBaseURI[];
  static const char kChildSrc[];
  static const char kFormAction[];
  static const char kFrameAncestors[];
  static const char kPluginTypes[];
  static const char kReflectedXSS[];
  static const char kReferrer[];

  // Custom CSP directive for Cobalt
  static const char kLocationSrc[];

  // Manifest Directives (to be merged into CSP Level 2)
  // https://w3c.github.io/manifest/#content-security-policy
  static const char kManifestSrc[];

  // Mixed Content Directive
  // https://w3c.github.io/webappsec/specs/mixedcontent/#strict-mode
  static const char kBlockAllMixedContent[];

  // https://w3c.github.io/webappsec/specs/upgrade/
  static const char kUpgradeInsecureRequests[];

  // Suborigin Directive
  // https://metromoxie.github.io/webappsec/specs/suborigins/index.html
  static const char kSuborigin[];

  enum ReportingStatus {
    kSendReport,
    kSuppressReport,
  };

  // When a resource is loaded after a redirect, source paths are
  // ignored in the matching algorithm.
  enum RedirectStatus {
    kDidRedirect,
    kDidNotRedirect,
  };

  static bool IsDirectiveName(const std::string& name);

  ContentSecurityPolicy(const GURL& url,
                        const ViolationCallback& violation_callback);
  ~ContentSecurityPolicy();

  void OnReceiveHeaders(const ResponseHeaders& headers);
  void OnReceiveHeader(const std::string& header, HeaderType header_type,
                       HeaderSource header_source);

  bool UrlMatchesSelf(const GURL& url) const;
  bool SchemeMatchesSelf(const GURL& url) const;

  void ReportViolation(const std::string& directive_text,
                       const std::string& effective_directive,
                       const std::string& console_message,
                       const GURL& blocked_url,
                       const std::vector<std::string>& report_endpoints,
                       const std::string& header);

  // Diagnostic functions for reporting errors in the CSP directives.
  void ReportInvalidReferrer(const std::string& invalid_value);
  void ReportInvalidPluginTypes(const std::string& invalid_plugin);
  void ReportMetaOutsideHead(const std::string& header);
  void ReportValueForEmptyDirective(const std::string& directive_name,
                                    const std::string& value);
  void ReportDirectiveAsSourceExpression(const std::string& directive_name,
                                         const std::string& source_expression);
  void ReportInvalidSourceExpression(const std::string& directive_name,
                                     const std::string& source);
  void ReportInvalidPathCharacter(const std::string& directive_name,
                                  const std::string& value, char c);
  void ReportDuplicateDirective(const std::string& name);
  void ReportInvalidDirectiveValueCharacter(const std::string& directive_name,
                                            const std::string& value);
  void ReportInvalidReflectedXSS(const std::string& invalid_value);
  void ReportMissingReportURI(const std::string& policy);
  void ReportReportOnlyInMeta(const std::string& header);
  void ReportInvalidSuboriginFlags(const std::string& invalid_flags);
  void ReportUnsupportedDirective(const std::string& name);
  void ReportInvalidInReportOnly(const std::string& name);
  void ReportDirectiveNotSupportedInsideMeta(const std::string& name);

  // https://www.w3.org/TR/2015/CR-CSP2-20150721/#directives
  bool AllowJavaScriptURLs(const std::string& context_url, int context_line,
                           ReportingStatus status = kSendReport) const;
  bool AllowInlineEventHandlers(const std::string& context_url,
                                int context_line,
                                ReportingStatus status = kSendReport) const;
  bool AllowInlineScript(const std::string& context_url, int context_line,
                         const std::string& script_content,
                         ReportingStatus status = kSendReport) const;
  bool AllowInlineStyle(const std::string& context_url, int context_line,
                        const std::string& style_content,
                        ReportingStatus status = kSendReport) const;
  bool AllowEval(ReportingStatus status = kSendReport) const;
  bool AllowScriptFromSource(const GURL& url,
                             RedirectStatus redirect = kDidNotRedirect,
                             ReportingStatus report = kSendReport) const;
  bool AllowObjectFromSource(const GURL& url,
                             RedirectStatus redirect = kDidNotRedirect,
                             ReportingStatus report = kSendReport) const;
  bool AllowImageFromSource(const GURL& url,
                            RedirectStatus redirect = kDidNotRedirect,
                            ReportingStatus report = kSendReport) const;
  bool AllowStyleFromSource(const GURL& url,
                            RedirectStatus redirect = kDidNotRedirect,
                            ReportingStatus report = kSendReport) const;
  bool AllowFontFromSource(const GURL& url,
                           RedirectStatus redirect = kDidNotRedirect,
                           ReportingStatus report = kSendReport) const;
  bool AllowMediaFromSource(const GURL& url,
                            RedirectStatus redirect = kDidNotRedirect,
                            ReportingStatus report = kSendReport) const;
  bool AllowConnectToSource(const GURL& url,
                            RedirectStatus redirect = kDidNotRedirect,
                            ReportingStatus report = kSendReport) const;
  bool AllowNavigateToSource(const GURL& url,
                             RedirectStatus redirect = kDidNotRedirect,
                             ReportingStatus report = kSendReport) const;

  bool AllowFormAction(const GURL& url,
                       RedirectStatus redirect = kDidNotRedirect,
                       ReportingStatus report = kSendReport) const;
  bool AllowBaseURI(const GURL& url, RedirectStatus redirect = kDidNotRedirect,
                    ReportingStatus report = kSendReport) const;
  bool AllowManifestFromSource(const GURL& url,
                               RedirectStatus redirect = kDidNotRedirect,
                               ReportingStatus report = kSendReport) const;

  // The nonce and hash allow functions are guaranteed to not have any side
  // effects, including reporting.
  // Nonce/Hash functions check all policies relating to use of a script/style
  // with the given nonce/hash and return true all CSP policies allow it.
  // If these return true, callers can then process the content or
  // issue a load and be safe disabling any further CSP checks.
  bool AllowScriptWithNonce(const std::string& nonce) const;
  bool AllowStyleWithNonce(const std::string& nonce) const;
  bool AllowScriptWithHash(const std::string& source) const;
  bool AllowStyleWithHash(const std::string& source) const;

  void set_uses_script_hash_algorithms(uint8 algorithm) {
    script_hash_algorithms_used_ |= algorithm;
  }
  void set_uses_style_hash_algorithms(uint8 algorithm) {
    style_hash_algorithms_used_ |= algorithm;
  }

  // Configure the origin URL for the document that owns this policy.
  // This could change during loading if the document load is redirected.
  void NotifyUrlChanged(const GURL& url);
  bool DidSetReferrerPolicy() const;

  const GURL& url() const { return url_; }
  void set_enforce_strict_mixed_content_checking() {
    enforce_strict_mixed_content_checking_ = true;
  }

  const std::string& disable_eval_error_message() const {
    return disable_eval_error_message_;
  }

 private:
  void CreateSelfSource();
  // Parses CSP header and creates policy based on that.
  void AddPolicyFromHeaderValue(const std::string& header, HeaderType type,
                                HeaderSource source);

  PolicyList policies_;
  std::unique_ptr<Source> self_source_;
  std::string self_scheme_;
  std::string disable_eval_error_message_;
  GURL url_;

  // Callback to use for reporting CSP violations.
  ViolationCallback violation_callback_;

  // Fields needing initialization.
  // Bitmasks of HashAlgorithm
  uint8 script_hash_algorithms_used_;
  uint8 style_hash_algorithms_used_;
  bool enforce_strict_mixed_content_checking_;
  ReferrerPolicy referrer_policy_;

  DISALLOW_COPY_AND_ASSIGN(ContentSecurityPolicy);
};

}  // namespace csp
}  // namespace cobalt

#endif  // COBALT_CSP_CONTENT_SECURITY_POLICY_H_
