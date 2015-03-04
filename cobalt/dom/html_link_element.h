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
#include "cobalt/browser/loader/text_load.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/dom/html_element.h"

namespace cobalt {
namespace dom {

// The link element allows authors to link their document to other resources.
//   http://www.w3.org/TR/html/document-metadata.html#the-link-element
class HTMLLinkElement : public HTMLElement {
 public:
  static const char* kTagName;

  static scoped_refptr<HTMLLinkElement> Create(
      browser::ResourceLoaderFactory* loader_factory,
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

 protected:
  // From Node.
  void AttachToDocument(Document* document) OVERRIDE;

 private:
  HTMLLinkElement(browser::ResourceLoaderFactory* loader_factory,
                  cssom::CSSParser* css_parser);
  ~HTMLLinkElement() OVERRIDE;

  // From the spec: HTMLLinkElement.
  void Obtain();

  void OnLoadingDone(const std::string& content);
  void OnLoadingError(const browser::ResourceLoaderError& error);
  bool IsLoading() const;
  void StopLoading();

  // ResourceLoaderFactory that is used to create a byte loader.
  browser::ResourceLoaderFactory* const loader_factory_;
  // An abstraction of CSS parser.
  cssom::CSSParser* const css_parser_;
  // This object is responsible for the loading.
  scoped_ptr<browser::TextLoad> text_load_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_HTML_LINK_ELEMENT_H_
