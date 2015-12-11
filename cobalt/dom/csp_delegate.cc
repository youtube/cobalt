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

#include "base/hash.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/security_policy_violation_event.h"
#include "cobalt/network/net_poster.h"
#include "cobalt/script/global_object_proxy.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

namespace {

// Fields in the JSON violation report.
const char kDocumentUri[] = "document-uri";
const char kReferrer[] = "referrer";
const char kViolatedDirective[] = "violated-directive";
const char kEffectiveDirective[] = "effective-directive";
const char kOriginalPolicy[] = "original-policy";
const char kBlockedUri[] = "blocked-uri";
const char kSourceFile[] = "source-file";
const char kLineNumber[] = "line-number";
const char kColumnNumber[] = "column-number";
const char kStatusCode[] = "status-code";
const char kCspReport[] = "csp-report";
const char kCspReportContentType[] = "application/csp-report";

struct ViolationEvent {
  std::string document_uri;
  std::string blocked_uri;
  std::string referrer;
  std::string violated_directive;
  std::string effective_directive;
  std::string original_policy;
  std::string source_file;
  int line_number;
  int column_number;
  int status_code;
};

std::string StripURLForUseInReport(const GURL& origin_url, const GURL& url) {
  // http://www.w3.org/TR/CSP/#strip-uri-for-reporting
  if (!url.is_valid()) {
    return std::string();
  }
  if (!url.IsStandard() || url.SchemeIsFile()) {
    // 1. Non-standard scheme.
    return url.scheme();
  }
  if (origin_url.GetOrigin() != url.GetOrigin()) {
    // 2. Mismatched origin.
    return url.GetOrigin().spec();
  } else {
    // 3. Same origin- strip username, password and ref.
    GURL::Replacements replacements;
    replacements.ClearUsername();
    replacements.ClearPassword();
    replacements.ClearRef();
    return url.ReplaceComponents(replacements).spec();
  }
}

void GatherSecurityPolicyViolationEventData(
    const Document* document, const std::string& directive_text,
    const std::string& effective_directive, const GURL& blocked_url,
    const std::string& header, ViolationEvent* event_data) {
  event_data->document_uri = document->url();
  event_data->blocked_uri =
      StripURLForUseInReport(document->url_as_gurl(), blocked_url);
  // TODO(***REMOVED***): Implement Document referrer, if needed.
  event_data->referrer = "";
  event_data->violated_directive = directive_text;
  event_data->effective_directive = effective_directive;
  event_data->original_policy = header;

  std::string source_url;
  script::GlobalObjectProxy* global_object_proxy =
      document->html_element_context()->script_runner()->GetGlobalObjectProxy();
  const std::vector<script::StackFrame>& stack_trace =
      global_object_proxy->GetStackTrace(1);
  if (stack_trace.size() > 0) {
    event_data->line_number = stack_trace[0].line_number;
    event_data->column_number = stack_trace[0].column_number;
    event_data->source_file = StripURLForUseInReport(
        document->url_as_gurl(), GURL(stack_trace[0].source_url));
  }

  // TODO(***REMOVED***): Set the status code if the document origin is non-secure.
  event_data->status_code = 0;
}

}  // namespace

CSPDelegate::CSPDelegate(Document* document) : document_(document) {
  csp_.reset(new csp::ContentSecurityPolicy());
  if (document_ && !document_->net_poster_factory().is_null()) {
    net_poster_ = document_->net_poster_factory().Run().Pass();
  }
  csp_->BindDelegate(this);
}

CSPDelegate::~CSPDelegate() {}

csp::ContentSecurityPolicy* CSPDelegate::csp() const { return csp_.get(); }

bool CSPDelegate::CanConnectToSource(const GURL& url) const {
  return csp_->AllowConnectToSource(url);
}

bool CSPDelegate::CanLoadFont(const GURL& url) const {
  return csp_->AllowFontFromSource(url);
}

bool CSPDelegate::CanLoadImage(const GURL& url) const {
  return csp_->AllowImageFromSource(url);
}

bool CSPDelegate::CanLoadMedia(const GURL& url) const {
  return csp_->AllowMediaFromSource(url);
}

bool CSPDelegate::CanLoadScript(const GURL& url) const {
  return csp_->AllowScriptFromSource(url);
}

GURL CSPDelegate::url() const { return document_->url_as_gurl(); }

// http://www.w3.org/TR/CSP2/#violation-reports
void CSPDelegate::ReportViolation(const std::string& directive_text,
                                  const std::string& effective_directive,
                                  const std::string& console_message,
                                  const GURL& blocked_url,
                                  const std::vector<std::string>& endpoints,
                                  const std::string& header) {
  DLOG(INFO) << console_message;
  ViolationEvent violation_data;
  GatherSecurityPolicyViolationEventData(document_, directive_text,
                                         effective_directive, blocked_url,
                                         header, &violation_data);
  document_->DispatchEvent(new SecurityPolicyViolationEvent(
      violation_data.document_uri, violation_data.referrer,
      violation_data.blocked_uri, violation_data.violated_directive,
      violation_data.effective_directive, violation_data.original_policy,
      violation_data.source_file, violation_data.status_code,
      violation_data.line_number, violation_data.column_number));

  if (endpoints.empty() || !net_poster_) {
    return;
  }

  // We need to be careful here when deciding what information to send to the
  // report-uri. Currently, we send only the current document's URL and the
  // directive that was violated. The document's URL is safe to send because
  // it's the document itself that's requesting that it be sent. You could
  // make an argument that we shouldn't send HTTPS document URLs to HTTP
  // report-uris (for the same reasons that we supress the Referer in that
  // case), but the Referer is sent implicitly whereas this request is only
  // sent explicitly. As for which directive was violated, that's pretty
  // harmless information.

  scoped_ptr<base::DictionaryValue> csp_report(new base::DictionaryValue());
  csp_report->SetString(kDocumentUri, violation_data.document_uri);
  csp_report->SetString(kReferrer, violation_data.referrer);
  csp_report->SetString(kViolatedDirective, violation_data.violated_directive);
  csp_report->SetString(kEffectiveDirective,
                        violation_data.effective_directive);
  csp_report->SetString(kOriginalPolicy, violation_data.original_policy);
  csp_report->SetString(kBlockedUri, violation_data.blocked_uri);
  if (!violation_data.source_file.empty() && violation_data.line_number != 0) {
    csp_report->SetString(kSourceFile, violation_data.source_file);
    csp_report->SetInteger(kLineNumber, violation_data.line_number);
    csp_report->SetInteger(kColumnNumber, violation_data.column_number);
  }
  csp_report->SetInteger(kStatusCode, violation_data.status_code);

  scoped_ptr<base::DictionaryValue> report_object(new base::DictionaryValue());
  report_object->Set(kCspReport, csp_report.release());

  std::string json_string;
  base::JSONWriter::Write(report_object.get(), &json_string);

  SendViolationReports(endpoints, json_string);
}

void CSPDelegate::SendViolationReports(
    const std::vector<std::string>& endpoints, const std::string& report) {
  uint32 report_hash = base::Hash(report);
  if (violation_reports_sent_.find(report_hash) !=
      violation_reports_sent_.end()) {
    return;
  }
  violation_reports_sent_.insert(report_hash);

  const GURL& origin_url = document_->url_as_gurl();
  for (std::vector<std::string>::const_iterator it = endpoints.begin();
       it != endpoints.end(); ++it) {
    GURL resolved_endpoint = origin_url.Resolve(*it);
    net_poster_->Send(resolved_endpoint, kCspReportContentType, report);
  }
}

void CSPDelegate::SetReferrerPolicy(csp::ReferrerPolicy policy) {
  // TODO(***REMOVED***): Set the referrer policy on the document.
  UNREFERENCED_PARAMETER(policy);
  NOTIMPLEMENTED();
}

}  // namespace dom
}  // namespace cobalt
