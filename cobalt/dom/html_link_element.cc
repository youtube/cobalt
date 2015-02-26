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

#include "base/bind.h"
#include "cobalt/dom/document.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

// static
const char* HTMLLinkElement::kTagName = "link";

// static
scoped_refptr<HTMLLinkElement> HTMLLinkElement::Create(
    browser::ResourceLoaderFactory* loader_factory,
    cssom::CSSParser* css_parser) {
  return make_scoped_refptr(new HTMLLinkElement(loader_factory, css_parser));
}

void HTMLLinkElement::AttachToDocument(Document* document) {
  Node::AttachToDocument(document);
  Obtain();
}

HTMLLinkElement::HTMLLinkElement(browser::ResourceLoaderFactory* loader_factory,
                                 cssom::CSSParser* css_parser)
    : loader_factory_(loader_factory), css_parser_(css_parser) {}

HTMLLinkElement::~HTMLLinkElement() {}

// Algorithm for Obtain:
//   http://www.w3.org/TR/html/document-metadata.html#concept-link-obtain
void HTMLLinkElement::Obtain() {
  // Custom, not in any spec.
  DCHECK(!IsLoading());

  // 1. If the href attribute's value is the empty string, then abort these
  // steps.
  if (href().empty()) return;

  // 2. Resolve the URL given by the href attribute, relative to the element.
  // 3. If the previous step fails, then abort these steps.
  const GURL base_url = owner_document()->url_as_gurl();
  const GURL url = base_url.Resolve(href());
  if (!url.is_valid()) {
    // TODO(***REMOVED***): Report URL cannot be resolved.
    LOG(INFO) << href() << " cannot be resolved based on " << base_url;
    return;
  }

  // 4. Do a potentially CORS-enabled fetch of the resulting absolute URL, with
  // the mode being the current state of the element's crossorigin content
  // attribute, the origin being the origin of the link element's Document, and
  // the default origin behaviour set to taint.
  text_load_ = make_scoped_ptr(
      new browser::TextLoad(owner_document().get(), loader_factory_, url,
                            base::Bind(&HTMLLinkElement::OnLoadingDone, this),
                            base::Bind(&HTMLLinkElement::OnLoadingError, this),
                            base::Bind(&HTMLLinkElement::StopLoading, this)));
}

void HTMLLinkElement::OnLoadingDone(const std::string& content) {
  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      css_parser_->ParseStyleSheet(href(), content);
  owner_document()->style_sheets()->Append(style_sheet);
  // TODO(***REMOVED***): List of style sheets should be managed by the document, so we
  // don't have to report the mutation manually. Moreover, it's a CSSOM
  // mutation, not a DOM mutation, so we may want to split the RecordMutation()
  // method into two methods to have a better event granularity.
  owner_document()->RecordMutation();
}

void HTMLLinkElement::OnLoadingError(
    const browser::ResourceLoaderError& error) {}

bool HTMLLinkElement::IsLoading() const { return text_load_.get() != NULL; }

void HTMLLinkElement::StopLoading() {
  DCHECK(IsLoading());
  text_load_.reset();
}

}  // namespace dom
}  // namespace cobalt
