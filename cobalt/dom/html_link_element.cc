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
    loader::FetcherFactory* fetcher_factory, cssom::CSSParser* css_parser) {
  return make_scoped_refptr(new HTMLLinkElement(fetcher_factory, css_parser));
}

const std::string& HTMLLinkElement::tag_name() const {
  static const std::string kLinkTagString(kTagName);
  return kLinkTagString;
}

void HTMLLinkElement::AttachToDocument(Document* document) {
  Node::AttachToDocument(document);
  Obtain();
}

HTMLLinkElement::HTMLLinkElement(loader::FetcherFactory* fetcher_factory,
                                 cssom::CSSParser* css_parser)
    : fetcher_factory_(fetcher_factory), css_parser_(css_parser) {}

HTMLLinkElement::~HTMLLinkElement() {}

// Algorithm for Obtain:
//   http://www.w3.org/TR/html/document-metadata.html#concept-link-obtain
void HTMLLinkElement::Obtain() {
  // Custom, not in any spec.
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(loader_.get() == NULL);

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
  loader_ = make_scoped_ptr(new loader::Loader(
      base::Bind(&loader::FetcherFactory::CreateFetcher,
                 base::Unretained(fetcher_factory_), url),
      scoped_ptr<loader::Decoder>(new loader::TextDecoder(
          base::Bind(&HTMLLinkElement::OnLoadingDone, base::Unretained(this)))),
      base::Bind(&HTMLLinkElement::OnLoadingError, base::Unretained(this))));
}

void HTMLLinkElement::OnLoadingDone(const std::string& content) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<cssom::CSSStyleSheet> style_sheet =
      css_parser_->ParseStyleSheet(href(), content);
  owner_document()->style_sheets()->Append(style_sheet);
  // TODO(***REMOVED***): List of style sheets should be managed by the document, so we
  // don't have to report the mutation manually. Moreover, it's a CSSOM
  // mutation, not a DOM mutation, so we may want to split the RecordMutation()
  // method into two methods to have a better event granularity.
  owner_document()->RecordMutation();
  StopLoading();
}

void HTMLLinkElement::OnLoadingError(const std::string& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG(ERROR) << error;
  StopLoading();
}

void HTMLLinkElement::StopLoading() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(loader_.get() != NULL);
  loader_.reset();
}

}  // namespace dom
}  // namespace cobalt
