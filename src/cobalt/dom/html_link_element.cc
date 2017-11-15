// Copyright 2014 Google Inc. All Rights Reserved.
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
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/window.h"
#include "googleurl/src/gurl.h"
#include "nb/memory_scope.h"

namespace cobalt {
namespace dom {
namespace {

CspDelegate::ResourceType GetCspResourceTypeForRel(const std::string& rel) {
  if (rel == "stylesheet") {
    return CspDelegate::kStyle;
  } else if (rel == "splashscreen") {
    return CspDelegate::kLocation;
  } else {
    NOTIMPLEMENTED();
    return CspDelegate::kImage;
  }
}

bool IsRelContentCriticalResource(const std::string& rel) {
  return rel == "stylesheet";
}

loader::RequestMode GetRequestMode(
    const base::optional<std::string>& cross_origin_attribute) {
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
    "stylesheet", "splashscreen"};

void HTMLLinkElement::OnInsertedIntoDocument() {
  HTMLElement::OnInsertedIntoDocument();
  if (std::find(kSupportedRelValues.begin(), kSupportedRelValues.end(),
                rel()) != kSupportedRelValues.end()) {
    Obtain();
  } else {
    LOG(WARNING) << "<link> has unsupported rel value: " << rel() << ".";
  }
}

base::optional<std::string> HTMLLinkElement::cross_origin() const {
  base::optional<std::string> cross_origin_attribute =
      GetAttribute("crossOrigin");
  if (cross_origin_attribute &&
      (*cross_origin_attribute != "anonymous" &&
       *cross_origin_attribute != "use-credentials")) {
    return std::string();
  }
  return cross_origin_attribute;
}

void HTMLLinkElement::set_cross_origin(
    const base::optional<std::string>& value) {
  if (value) {
    SetAttribute("crossOrigin", *value);
  } else {
    RemoveAttribute("crossOrigin");
  }
}

void HTMLLinkElement::OnRemovedFromDocument() {
  HTMLElement::OnRemovedFromDocument();

  if (style_sheet_) {
    Document* document = node_document();
    if (document) {
      document->OnStyleSheetsModified();
    }
  }
}

void HTMLLinkElement::ResolveAndSetAbsoluteURL() {
  // Resolve the URL given by the href attribute, relative to the element.
  const GURL& base_url = node_document()->url_as_gurl();
  absolute_url_ = base_url.Resolve(href());

  LOG_IF(WARNING, !absolute_url_.is_valid())
      << href() << " cannot be resolved based on " << base_url << ".";
}

// Algorithm for Obtain:
//   https://www.w3.org/TR/html5/document-metadata.html#concept-link-obtain
void HTMLLinkElement::Obtain() {
  TRACK_MEMORY_SCOPE("DOM");
  TRACE_EVENT0("cobalt::dom", "HTMLLinkElement::Obtain()");
  // Custom, not in any spec.
  DCHECK(thread_checker_.CalledOnValidThread());

  Document* document = node_document();

  // If the document has no browsing context, do not obtain, parse or apply the
  // resource.
  if (!document->html_element_context()) {
    return;
  }

  DCHECK(MessageLoop::current());
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
      &CspDelegate::CanLoad, base::Unretained(document->csp_delegate()),
      GetCspResourceTypeForRel(rel()));

  fetched_last_url_origin_ = loader::Origin();

  if (IsRelContentCriticalResource(rel())) {
    // The element must delay the load event of the element's document until all
    // the attempts to obtain the resource and its critical subresources are
    // complete.
    document->IncreaseLoadingCounter();
  }

  request_mode_ = GetRequestMode(GetAttribute("crossOrigin"));
  loader_ = make_scoped_ptr(new loader::Loader(
      base::Bind(
          &loader::FetcherFactory::CreateSecureFetcher,
          base::Unretained(document->html_element_context()->fetcher_factory()),
          absolute_url_, csp_callback, request_mode_,
          document->location() ? document->location()->OriginObject()
                               : loader::Origin()),
      scoped_ptr<loader::Decoder>(new loader::TextDecoder(
          base::Bind(&HTMLLinkElement::OnLoadingDone, base::Unretained(this)))),
      base::Bind(&HTMLLinkElement::OnLoadingError, base::Unretained(this))));
}

void HTMLLinkElement::OnLoadingDone(const std::string& content,
                                    const loader::Origin& last_url_origin) {
  TRACK_MEMORY_SCOPE("DOM");
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("cobalt::dom", "HTMLLinkElement::OnLoadingDone()");

  // Get resource's final destination url from loader.
  fetched_last_url_origin_ = last_url_origin;

  Document* document = node_document();
  if (rel() == "stylesheet") {
    OnStylesheetLoaded(document, content);
  } else if (rel() == "splashscreen") {
    OnSplashscreenLoaded(document, content);
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
  PostToDispatchEvent(FROM_HERE, base::Tokens::load());

  if (IsRelContentCriticalResource(rel())) {
    // The element must delay the load event of the element's document until all
    // the attempts to obtain the resource and its critical subresources are
    // complete.
    document->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
  }

  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&HTMLLinkElement::ReleaseLoader, this));
}

void HTMLLinkElement::OnLoadingError(const std::string& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("cobalt::dom", "HTMLLinkElement::OnLoadingError()");

  LOG(ERROR) << error;

  // Once the attempts to obtain the resource and its critical subresources are
  // complete, the user agent must, if the loads were successful, queue a task
  // to fire a simple event named load at the link element, or, if the resource
  // or one of its critical subresources failed to completely load for any
  // reason (e.g. DNS error, HTTP 404 response, a connection being prematurely
  // closed, unsupported Content-Type), queue a task to fire a simple event
  // named error at the link element.
  PostToDispatchEvent(FROM_HERE, base::Tokens::error());

  if (IsRelContentCriticalResource(rel())) {
    // The element must delay the load event of the element's document until all
    // the attempts to obtain the resource and its critical subresources are
    // complete.
    node_document()->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
  }

  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&HTMLLinkElement::ReleaseLoader, this));
}

void HTMLLinkElement::OnSplashscreenLoaded(Document* document,
                                           const std::string& content) {
  scoped_refptr<Window> window = document->window();
  window->CacheSplashScreen(content);
}

void HTMLLinkElement::OnStylesheetLoaded(Document* document,
                                         const std::string& content) {
  scoped_refptr<cssom::CSSStyleSheet> css_style_sheet =
      document->html_element_context()->css_parser()->ParseStyleSheet(
          content, base::SourceLocation(href(), 1, 1));
  css_style_sheet->SetLocationUrl(absolute_url_);
  // If not loading from network-fetched resources or fetched resource is same
  // origin as the document, set origin-clean flag to true.
  if (request_mode_ != loader::kNoCORSMode || !loader_ ||
      document->url_as_gurl().SchemeIsFile() ||
      (fetched_last_url_origin_ == document->location()->OriginObject())) {
    css_style_sheet->SetOriginClean(true);
  }
  style_sheet_ = css_style_sheet;
  document->OnStyleSheetsModified();
}

void HTMLLinkElement::ReleaseLoader() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(loader_);
  loader_.reset();
}

void HTMLLinkElement::CollectStyleSheet(
    cssom::StyleSheetVector* style_sheets) const {
  if (style_sheet_) {
    style_sheets->push_back(style_sheet_);
  }
}

}  // namespace dom
}  // namespace cobalt
