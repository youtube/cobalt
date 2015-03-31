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

#ifndef DOM_HTML_LINK_ELEMENT_H_
#define DOM_HTML_LINK_ELEMENT_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/loader.h"
#include "cobalt/loader/text_decoder.h"

namespace cobalt {
namespace dom {

// The link element allows authors to link their document to other resources.
//   http://www.w3.org/TR/html5/document-metadata.html#the-link-element
class HTMLLinkElement : public HTMLElement {
 public:
  static const char* kTagName;

  HTMLLinkElement(HTMLElementFactory* html_element_factory,
                  loader::FetcherFactory* fetcher_factory,
                  cssom::CSSParser* css_parser);

  // Web API: Element
  //
  const std::string& tag_name() const OVERRIDE;

  // Web API: HTMLLinkElement
  //
  std::string rel() const { return GetAttribute("rel").value_or(""); }
  void set_rel(const std::string& value) { SetAttribute("rel", value); }

  std::string type() const { return GetAttribute("type").value_or(""); }
  void set_type(const std::string& value) { SetAttribute("type", value); }

  std::string href() const { return GetAttribute("href").value_or(""); }
  void set_href(const std::string& value) { SetAttribute("href", value); }

  // Custom, not in any spec.
  //
  scoped_refptr<HTMLLinkElement> AsHTMLLinkElement() OVERRIDE { return this; }

  // From Node.
  void AttachToDocument(Document* document) OVERRIDE;

 private:
  ~HTMLLinkElement() OVERRIDE;

  // From the spec: HTMLLinkElement.
  void Obtain();

  void OnLoadingDone(const std::string& content);
  void OnLoadingError(const std::string& error);
  void StopLoading();

  // FetcherFactory that is used to create a fetcher according to url.
  loader::FetcherFactory* fetcher_factory_;
  // The loader.
  scoped_ptr<loader::Loader> loader_;
  // An abstraction of CSS parser.
  cssom::CSSParser* const css_parser_;
  // Thread checker.
  base::ThreadChecker thread_checker_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_HTML_LINK_ELEMENT_H_
