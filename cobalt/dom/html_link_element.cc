/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/html_link_element.h"

#include <string>

#include "base/bind.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element_context.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

// static
const char* HTMLLinkElement::kTagName = "link";

HTMLLinkElement::HTMLLinkElement(Document* document,
                                 HTMLElementContext* html_element_context)
    : HTMLElement(document, html_element_context) {}

std::string HTMLLinkElement::tag_name() const { return kTagName; }

void HTMLLinkElement::OnInsertedIntoDocument() {
  HTMLElement::OnInsertedIntoDocument();
  Obtain();
}

HTMLLinkElement::~HTMLLinkElement() {}

void HTMLLinkElement::ResolveAndSetAbsoluteURL() {
  // Resolve the URL given by the href attribute, relative to the element.
  const GURL base_url = owner_document()->url_as_gurl();
  absolute_url_ = base_url.Resolve(href());

  LOG_IF(INFO, !absolute_url_.is_valid())
      << href() << " cannot be resolved based on " << base_url;
}

// Algorithm for Obtain:
//   http://www.w3.org/TR/html5/document-metadata.html#concept-link-obtain
void HTMLLinkElement::Obtain() {
  // Custom, not in any spec.
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!loader_);

  // 1. If the href attribute's value is the empty string, then abort these
  // steps.
  if (href().empty()) return;

  // 2. Resolve the URL given by the href attribute, relative to the element.
  ResolveAndSetAbsoluteURL();

  // 3. If the previous step fails, then abort these steps.
  if (!absolute_url_.is_valid()) return;

  // 4. Do a potentially CORS-enabled fetch of the resulting absolute URL, with
  // the mode being the current state of the element's crossorigin content
  // attribute, the origin being the origin of the link element's Document, and
  // the default origin behaviour set to taint.
  loader_ = make_scoped_ptr(new loader::Loader(
      base::Bind(&loader::FetcherFactory::CreateFetcher,
                 base::Unretained(html_element_context()->fetcher_factory()),
                 absolute_url_),
      scoped_ptr<loader::Decoder>(new loader::TextDecoder(
          base::Bind(&HTMLLinkElement::OnLoadingDone, base::Unretained(this)))),
      base::Bind(&HTMLLinkElement::OnLoadingError, base::Unretained(this))));
}

void HTMLLinkElement::OnLoadingDone(const std::string& content) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      html_element_context()->css_parser()->ParseStyleSheet(
          content, base::SourceLocation(href(), 1, 1));
  style_sheet->SetLocationUrl(absolute_url_);
  owner_document()->style_sheets()->Append(style_sheet);
  StopLoading();
}

void HTMLLinkElement::OnLoadingError(const std::string& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG(ERROR) << error;
  StopLoading();
}

void HTMLLinkElement::StopLoading() {
  DCHECK(loader_);
  loader_.reset();
}

}  // namespace dom
}  // namespace cobalt
