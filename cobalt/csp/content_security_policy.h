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

#ifndef CSP_CONTENT_SECURITY_POLICY_H_
#define CSP_CONTENT_SECURITY_POLICY_H_

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/csp/parsers.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_response_headers.h"

namespace cobalt {
namespace csp {

class DirectiveList;
class Source;

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
  typedef ScopedVector<DirectiveList> PolicyList;

  // Interface for the ContentSecurityPolicy object to delegate requests
  // for talking to the DOM or script engine.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual GURL url() const = 0;

    virtual void ReportViolation(
        const std::string& /* directive_text */,
        const std::string& /* effective_directive */,
        const std::string& /* console_message */, const GURL& /* blocked_url */,
        const std::vector<std::string>& /* endpoints */,
        const std::string& /* header */) {}

    virtual void SetReferrerPolicy(ReferrerPolicy /* policy */) {}
  };

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

  ContentSecurityPolicy();
  ~ContentSecurityPolicy();

  void BindDelegate(Delegate* delegate);

  void DidReceiveHeaders(const ResponseHeaders& headers);
  void DidReceiveHeader(const std::string& header, HeaderType header_type,
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

  // http://www.w3.org/TR/2015/CR-CSP2-20150721/#directives
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

  GURL url() const;
  bool DidSetReferrerPolicy() const;
  void set_enforce_strict_mixed_content_checking() {
    enforce_strict_mixed_content_checking_ = true;
  }

 private:
  // Called by BindDelegate() to update self_source_.
  void ApplyPolicy();
  // Parses CSP header and creates policy based on that.
  void AddPolicyFromHeaderValue(const std::string& header, HeaderType type,
                                HeaderSource source);

  PolicyList policies_;
  scoped_ptr<Source> self_source_;
  std::string self_scheme_;
  std::string disable_eval_error_message_;

  // Fields needing initialization.
  Delegate* delegate_;
  // Bitmasks of HashAlgorithm
  uint8 script_hash_algorithms_used_;
  uint8 style_hash_algorithms_used_;
  bool enforce_strict_mixed_content_checking_;
  ReferrerPolicy referrer_policy_;

  DISALLOW_COPY_AND_ASSIGN(ContentSecurityPolicy);
};

}  // namespace csp
}  // namespace cobalt

#endif  // CSP_CONTENT_SECURITY_POLICY_H_
