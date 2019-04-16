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

#ifndef COBALT_CSP_DIRECTIVE_LIST_H_
#define COBALT_CSP_DIRECTIVE_LIST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "cobalt/csp/content_security_policy.h"
#include "url/gurl.h"

namespace cobalt {
namespace csp {

class MediaListDirective;
class SourceListDirective;

class DirectiveList {
 public:
  DirectiveList(ContentSecurityPolicy* policy, const base::StringPiece& text,
                HeaderType header_type, HeaderSource header_source);
  ~DirectiveList();
  void Parse(const base::StringPiece& text);

  const std::string& header() const { return header_; }
  HeaderType header_type() const { return header_type_; }
  HeaderSource header_source() const { return header_source_; }

  bool AllowJavaScriptURLs(
      const std::string& context_url, int context_line,
      ContentSecurityPolicy::ReportingStatus reporting_status) const;
  bool AllowInlineEventHandlers(
      const std::string& context_url, int context_line,
      ContentSecurityPolicy::ReportingStatus reporting_status) const;
  bool AllowInlineScript(
      const std::string& context_url, int context_line,
      ContentSecurityPolicy::ReportingStatus reporting_status,
      const std::string& script_content) const;
  bool AllowInlineStyle(const std::string& context_url, int context_line,
                        ContentSecurityPolicy::ReportingStatus reporting_status,
                        const std::string& style_content) const;
  bool AllowEval(ContentSecurityPolicy::ReportingStatus reporting_status) const;
  bool AllowPluginType(
      const std::string& type, const std::string& type_attribute,
      const GURL& url,
      ContentSecurityPolicy::ReportingStatus reporting_status) const;

  bool AllowScriptFromSource(
      const GURL& url, ContentSecurityPolicy::RedirectStatus redirect_status,
      ContentSecurityPolicy::ReportingStatus reporting_status) const;
  bool AllowObjectFromSource(
      const GURL&, ContentSecurityPolicy::RedirectStatus redirect_status,
      ContentSecurityPolicy::ReportingStatus reporting_status) const;
  bool AllowImageFromSource(
      const GURL&, ContentSecurityPolicy::RedirectStatus redirect_status,
      ContentSecurityPolicy::ReportingStatus reporting_status) const;
  bool AllowStyleFromSource(
      const GURL&, ContentSecurityPolicy::RedirectStatus redirect_status,
      ContentSecurityPolicy::ReportingStatus reporting_status) const;
  bool AllowFontFromSource(
      const GURL&, ContentSecurityPolicy::RedirectStatus redirect_status,
      ContentSecurityPolicy::ReportingStatus reporting_status) const;
  bool AllowMediaFromSource(
      const GURL&, ContentSecurityPolicy::RedirectStatus redirect_status,
      ContentSecurityPolicy::ReportingStatus reporting_status) const;
  bool AllowManifestFromSource(
      const GURL&, ContentSecurityPolicy::RedirectStatus redirect_status,
      ContentSecurityPolicy::ReportingStatus reporting_status) const;
  bool AllowConnectToSource(
      const GURL&, ContentSecurityPolicy::RedirectStatus redirect_status,
      ContentSecurityPolicy::ReportingStatus reporting_status) const;
  bool AllowNavigateToSource(
      const GURL&, ContentSecurityPolicy::RedirectStatus redirect_status,
      ContentSecurityPolicy::ReportingStatus reporting_status) const;
  bool AllowFormAction(
      const GURL&, ContentSecurityPolicy::RedirectStatus redirect_status,
      ContentSecurityPolicy::ReportingStatus reporting_status) const;
  bool AllowBaseURI(
      const GURL&, ContentSecurityPolicy::RedirectStatus redirect_status,
      ContentSecurityPolicy::ReportingStatus reporting_status) const;
  bool AllowScriptNonce(const std::string& script) const;
  bool AllowStyleNonce(const std::string& style) const;
  bool AllowScriptHash(const HashValue& script) const;
  bool AllowStyleHash(const HashValue& script) const;

  const std::string& eval_disabled_error_message() const {
    return eval_disabled_error_message_;
  }
  ReflectedXSSDisposition reflected_xss_disposition() const {
    return reflected_xss_disposition_;
  }
  ReferrerPolicy referrer_policy() const { return referrer_policy_; }
  bool did_set_referrer_policy() const { return did_set_referrer_policy_; }
  bool report_only() const { return report_only_; }
  const std::vector<std::string>& report_endpoints() const {
    return report_endpoints_;
  }
  bool has_location_src() const { return location_src_ != NULL; }

  // Used to copy plugin-types into a plugin document in a nested
  // browsing context.
  bool has_plugin_types() const { return !!plugin_types_; }
  const std::string& PluginTypesText() const;

 private:
  bool ParseDirective(const char* begin, const char* end, std::string* name,
                      std::string* value);
  void ParseReportURI(const std::string& name, const std::string& value);
  void ParsePluginTypes(const std::string& name, const std::string& value);
  void ParseReflectedXSS(const std::string& name, const std::string& value);
  void ParseReferrer(const std::string& name, const std::string& value);
  std::string ParseSuboriginName(const std::string& policy);
  void AddDirective(const std::string& name, const std::string& value);
  void ApplySandboxPolicy(const std::string& name,
                          const std::string& sandbox_policy);
  void ApplySuboriginPolicy(const std::string& name,
                            const std::string& suborigin_policy);
  void EnforceStrictMixedContentChecking(const std::string& name,
                                         const std::string& value);
  void EnableInsecureRequestsUpgrade(const std::string& name,
                                     const std::string& value);

  void SetCSPDirective(const std::string& name, const std::string& value,
                       std::unique_ptr<SourceListDirective>*);
  void SetCSPDirective(const std::string& name, const std::string& value,
                       std::unique_ptr<MediaListDirective>*);

  SourceListDirective* OperativeDirective(SourceListDirective* directive) const;
  SourceListDirective* OperativeDirective(
      SourceListDirective*, SourceListDirective* directive_override) const;
  void ReportViolation(const std::string& directive_text,
                       const std::string& effective_directive,
                       const std::string& console_message,
                       const GURL& blocked_url) const;
  void ReportViolationWithLocation(const std::string& directive_text,
                                   const std::string& effective_directive,
                                   const std::string& console_message,
                                   const GURL& blocked_url,
                                   const std::string& context_url,
                                   int context_line) const;

  bool CheckEval(SourceListDirective* directive) const;
  bool CheckInline(SourceListDirective* directive) const;
  bool CheckNonce(SourceListDirective* directive,
                  const std::string& nonce) const;
  bool CheckHash(SourceListDirective* directive,
                 const HashValue& hash_value) const;
  bool CheckSource(SourceListDirective* directive, const GURL& url,
                   ContentSecurityPolicy::RedirectStatus redirect_status) const;
  bool CheckMediaType(MediaListDirective* directive, const std::string& type,
                      const std::string& type_attribute) const;

  void set_eval_disabled_error_message(const std::string& error_message) {
    eval_disabled_error_message_ = error_message;
  }

  bool CheckEvalAndReportViolation(SourceListDirective* directive,
                                   const std::string& console_message) const;
  bool CheckInlineAndReportViolation(SourceListDirective* directive,
                                     const std::string& console_message,
                                     const std::string& context_url,
                                     int context_line, bool is_script,
                                     const std::string& hash_value) const;

  bool CheckSourceAndReportViolation(
      SourceListDirective* directive, const GURL& url,
      const std::string& effective_directive,
      ContentSecurityPolicy::RedirectStatus redirect_status) const;
  bool CheckMediaTypeAndReportViolation(
      MediaListDirective* directive, const std::string& type,
      const std::string& type_attribute,
      const std::string& console_message) const;

  bool deny_if_enforcing_policy() const { return report_only_; }

  ContentSecurityPolicy* policy_;

  std::string header_;
  HeaderType header_type_;
  HeaderSource header_source_;

  bool report_only_;
  bool has_sandbox_policy_;
  bool has_suborigin_policy_;
  ReflectedXSSDisposition reflected_xss_disposition_;

  bool did_set_referrer_policy_;
  ReferrerPolicy referrer_policy_;

  bool strict_mixed_content_checking_enforced_;

  bool upgrade_insecure_requests_;

  std::unique_ptr<MediaListDirective> plugin_types_;
  std::unique_ptr<SourceListDirective> base_uri_;
  std::unique_ptr<SourceListDirective> child_src_;
  std::unique_ptr<SourceListDirective> connect_src_;
  std::unique_ptr<SourceListDirective> default_src_;
  std::unique_ptr<SourceListDirective> font_src_;
  std::unique_ptr<SourceListDirective> form_action_;
  std::unique_ptr<SourceListDirective> frame_ancestors_;
  std::unique_ptr<SourceListDirective> frame_src_;
  std::unique_ptr<SourceListDirective> img_src_;
  std::unique_ptr<SourceListDirective> location_src_;
  std::unique_ptr<SourceListDirective> media_src_;
  std::unique_ptr<SourceListDirective> manifest_src_;
  std::unique_ptr<SourceListDirective> object_src_;
  std::unique_ptr<SourceListDirective> script_src_;
  std::unique_ptr<SourceListDirective> style_src_;

  std::vector<std::string> report_endpoints_;

  std::string eval_disabled_error_message_;
};
}  // namespace csp
}  // namespace cobalt

#endif  // COBALT_CSP_DIRECTIVE_LIST_H_
