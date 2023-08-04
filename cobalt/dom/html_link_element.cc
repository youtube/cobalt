// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/html_link_element.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_tokenizer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/window.h"
#include "cobalt/web/csp_delegate.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {
namespace {

// Constants for parse time histogram. Do not modify these. If you need to
// change these, create a new histogram and new constants.
constexpr size_t kNumParseTimeHistogramBuckets = 100;
constexpr base::TimeDelta kParseTimeHistogramMinTime =
    base::TimeDelta::FromMicroseconds(1);
constexpr base::TimeDelta kParseTimeHistogramMaxTime =
    base::TimeDelta::FromMilliseconds(10);

bool IsValidRelChar(char const& c) {
  return (isalnum(c) || c == '_' || c == '\\' || c == '-');
}

bool IsValidSplashScreenFormat(const std::string& rel) {
  base::StringTokenizer tokenizer(rel, "_");
  tokenizer.set_options(base::StringTokenizer::RETURN_DELIMS);
  bool is_valid_format = true;
  while (tokenizer.GetNext()) {
    std::string token = tokenizer.token();
    if (strcmp(token.c_str(), "splashscreen") == 0) {
      is_valid_format = true;
    } else {
      for (char const& c : token) {
        if (!IsValidRelChar(c)) {
          return false;
        }
      }
      is_valid_format = false;
    }
  }
  return is_valid_format;
}

web::CspDelegate::ResourceType GetCspResourceTypeForRel(
    const std::string& rel) {
  if (rel == "stylesheet") {
    return web::CspDelegate::kStyle;
  } else if (IsValidSplashScreenFormat(rel)) {
    return web::CspDelegate::kLocation;
  } else {
    NOTIMPLEMENTED();
    return web::CspDelegate::kImage;
  }
}

bool IsRelContentCriticalResource(const std::string& rel) {
  return rel == "stylesheet";
}

loader::RequestMode GetRequestMode(
    const base::Optional<std::string>& cross_origin_attribute) {
  // https://html.spec.whatwg.org/#cors-settings-attribute
  if (cross_origin_attribute) {
    if (*cross_origin_attribute == "use-credentials") {
      return loader::kCORSModeIncludeCredentials;
    } else {
      // The invalid value default of crossorigin is Anonymous state, leading to
      // "same-origin" credentials mode.
      return loader::kCORSModeSameOriginCredentials;
    }
  }
  // crossorigin attribute's missing value default is No CORS state, leading to
  // "no-cors" request mode.
  return loader::kNoCORSMode;
}
}  // namespace

// static
const char HTMLLinkElement::kTagName[] = "link";
// static
const std::vector<std::string> HTMLLinkElement::kSupportedRelValues = {
    "stylesheet"};

void HTMLLinkElement::OnInsertedIntoDocument() {
  HTMLElement::OnInsertedIntoDocument();
  if (std::find(kSupportedRelValues.begin(), kSupportedRelValues.end(),
                rel()) != kSupportedRelValues.end()) {
    Obtain();
  } else if (IsValidSplashScreenFormat(rel())) {
    Obtain();
  } else {
    LOG(WARNING) << "<link> has unsupported rel value: " << rel() << ".";
  }
}

base::Optional<std::string> HTMLLinkElement::cross_origin() const {
  base::Optional<std::string> cross_origin_attribute =
      GetAttribute("crossOrigin");
  if (cross_origin_attribute &&
      (*cross_origin_attribute != "anonymous" &&
       *cross_origin_attribute != "use-credentials")) {
    return std::string();
  }
  return cross_origin_attribute;
}

void HTMLLinkElement::set_cross_origin(
    const base::Optional<std::string>& value) {
  if (value) {
    SetAttribute("crossOrigin", *value);
  } else {
    RemoveAttribute("crossOrigin");
  }
}

void HTMLLinkElement::OnRemovedFromDocument() {
  HTMLElement::OnRemovedFromDocument();

  DCHECK(base::MessageLoop::current());
  ReleaseLoader();

  if (style_sheet_) {
    Document* document = node_document();
    if (document) {
      document->OnStyleSheetsModified();
    }
  }
}

void HTMLLinkElement::ResolveAndSetAbsoluteURL() {
  // Resolve the URL given by the href attribute, relative to the element.
  const GURL& base_url = node_document()->location()->url();
  absolute_url_ = base_url.Resolve(href());

  LOG_IF(WARNING, !absolute_url_.is_valid())
      << href() << " cannot be resolved based on " << base_url << ".";
}

// Algorithm for Obtain:
//   https://www.w3.org/TR/html50/document-metadata.html#concept-link-obtain
void HTMLLinkElement::Obtain() {
  TRACE_EVENT0("cobalt::dom", "HTMLLinkElement::Obtain()");
  // Custom, not in any spec.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Document* document = node_document();

  // If the document has no browsing context, do not obtain, parse or apply the
  // resource.
  if (!document->html_element_context()) {
    return;
  }

  DCHECK(base::MessageLoop::current());
  DCHECK(!loader_);

  // 1. If the href attribute's value is the empty string, then abort these
  // steps.
  if (href().empty()) {
    return;
  }

  // 2. Resolve the URL given by the href attribute, relative to the element.
  ResolveAndSetAbsoluteURL();

  // 3. If the previous step fails, then abort these steps.
  if (!absolute_url_.is_valid()) {
    return;
  }

  // 4. Do a potentially CORS-enabled fetch of the resulting absolute URL, with
  // the mode being the current state of the element's crossorigin content
  // attribute, the origin being the origin of the link element's Document, and
  // the default origin behaviour set to taint.
  csp::SecurityCallback csp_callback = base::Bind(
      &web::CspDelegate::CanLoad, base::Unretained(document->GetCSPDelegate()),
      GetCspResourceTypeForRel(rel()));

  fetched_last_url_origin_ = loader::Origin();

  if (IsRelContentCriticalResource(rel())) {
    // The element must delay the load event of the element's document until all
    // the attempts to obtain the resource and its critical subresources are
    // complete.
    document->IncreaseLoadingCounter();
  }

  request_mode_ = GetRequestMode(GetAttribute("crossOrigin"));

  DCHECK(!loader_);
  loader::Origin origin = document->location()
                              ? document->location()->GetOriginAsObject()
                              : loader::Origin();
  disk_cache::ResourceType type;
  if (rel() == "stylesheet") {
    type = disk_cache::kCSS;
  } else if (IsValidSplashScreenFormat(rel())) {
    type = disk_cache::kSplashScreen;
  } else {
    LOG(WARNING) << "<link> has unsupported rel value: " << rel() << ".";
    NOTIMPLEMENTED();
    return;
  }

  loader_ = html_element_context()->loader_factory()->CreateLinkLoader(
      absolute_url_, origin, csp_callback, request_mode_, type,
      base::Bind(&HTMLLinkElement::OnContentProduced, base::Unretained(this)),
      base::Bind(&HTMLLinkElement::OnLoadingComplete, base::Unretained(this)));
}

void HTMLLinkElement::OnContentProduced(const loader::Origin& last_url_origin,
                                        std::unique_ptr<std::string> content) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(content);
  TRACE_EVENT0("cobalt::dom", "HTMLLinkElement::OnContentProduced()");

  // Get resource's final destination url from loader.
  fetched_last_url_origin_ = last_url_origin;

  Document* document = node_document();
  if (rel() == "stylesheet") {
    OnStylesheetLoaded(document, *content);
  } else if (IsValidSplashScreenFormat(rel())) {
    OnSplashscreenLoaded(document, *content);
  } else {
    NOTIMPLEMENTED();
    return;
  }
  // Once the attempts to obtain the resource and its critical subresources
  // are complete, the user agent must, if the loads were successful, queue a
  // task to fire a simple event named load at the link element, or, if the
  // resource or one of its critical subresources failed to completely load
  // for any reason (e.g. DNS error, HTTP 404 response, a connection being
  // prematurely closed, unsupported Content-Type), queue a task to fire a
  // simple event named error at the link element.
  PostToDispatchEventName(FROM_HERE, base::Tokens::load());

  if (IsRelContentCriticalResource(rel())) {
    // The element must delay the load event of the element's document until all
    // the attempts to obtain the resource and its critical subresources are
    // complete.
    document->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
  }
}

void HTMLLinkElement::OnLoadingComplete(
    const base::Optional<std::string>& error) {
  // GetLoadTimingInfo and create resource timing before loader released.
  GetLoadTimingInfoAndCreateResourceTiming();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&HTMLLinkElement::ReleaseLoader, this));

  if (!error) return;

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("cobalt::dom", "HTMLLinkElement::OnLoadingComplete()");

  LOG(ERROR) << *error;

  // Once the attempts to obtain the resource and its critical subresources are
  // complete, the user agent must, if the loads were successful, queue a task
  // to fire a simple event named load at the link element, or, if the resource
  // or one of its critical subresources failed to completely load for any
  // reason (e.g. DNS error, HTTP 404 response, a connection being prematurely
  // closed, unsupported Content-Type), queue a task to fire a simple event
  // named error at the link element.
  PostToDispatchEventName(FROM_HERE, base::Tokens::error());

  if (IsRelContentCriticalResource(rel())) {
    // The element must delay the load event of the element's document until all
    // the attempts to obtain the resource and its critical subresources are
    // complete.
    node_document()->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
  }
}

void HTMLLinkElement::OnSplashscreenLoaded(Document* document,
                                           const std::string& content) {
  scoped_refptr<Window> window = document->window();
  std::string link = rel();
  size_t last_underscore = link.find_last_of("_");
  base::Optional<std::string> topic;
  if (last_underscore != std::string::npos) {
    topic = link.substr(0, last_underscore);
  }
  window->CacheSplashScreen(content, topic);
}

void HTMLLinkElement::OnStylesheetLoaded(Document* document,
                                         const std::string& content) {
  auto before_parse_micros = SbTimeGetMonotonicNow();
  scoped_refptr<cssom::CSSStyleSheet> css_style_sheet =
      document->html_element_context()->css_parser()->ParseStyleSheet(
          content, base::SourceLocation(href(), 1, 1));
  auto after_parse_micros = SbTimeGetMonotonicNow();
  auto css_kb = content.length() / 1000;
  // Only measure non-trivial CSS sizes and ignore non-HTTP schemes (e.g.,
  // file://), which are primarily used for debug purposes.
  if (css_kb > 0 && absolute_url_.SchemeIsHTTPOrHTTPS()) {
    // Get parse time normalized by byte size, see:
    // go/cobalt-js-css-parsing-metrics.
    auto micros_per_kb = (after_parse_micros - before_parse_micros) / css_kb;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Cobalt.DOM.CSS.Link.ParseTimeMicrosPerKB",
        base::TimeDelta::FromMicroseconds(micros_per_kb),
        kParseTimeHistogramMinTime, kParseTimeHistogramMaxTime,
        kNumParseTimeHistogramBuckets);
  }
  css_style_sheet->SetLocationUrl(absolute_url_);
  // If not loading from network-fetched resources or fetched resource is same
  // origin as the document, set origin-clean flag to true.
  if (request_mode_ != loader::kNoCORSMode || !loader_ ||
      document->location()->url().SchemeIsFile() ||
      (fetched_last_url_origin_ == document->location()->GetOriginAsObject())) {
    css_style_sheet->SetOriginClean(true);
  }
  style_sheet_ = css_style_sheet;
  document->OnStyleSheetsModified();
}

void HTMLLinkElement::ReleaseLoader() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  loader_.reset();
}

void HTMLLinkElement::CollectStyleSheet(
    cssom::StyleSheetVector* style_sheets) const {
  if (style_sheet_) {
    style_sheets->push_back(style_sheet_);
  }
}

void HTMLLinkElement::GetLoadTimingInfoAndCreateResourceTiming() {
  if (html_element_context()->performance() == nullptr) return;
  if (loader_) {
    html_element_context()->performance()->CreatePerformanceResourceTiming(
        loader_->get_load_timing_info(), kTagName, absolute_url_.spec());
  }
}

}  // namespace dom
}  // namespace cobalt
