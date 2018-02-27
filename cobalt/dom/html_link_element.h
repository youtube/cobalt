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

#ifndef COBALT_DOM_HTML_LINK_ELEMENT_H_
#define COBALT_DOM_HTML_LINK_ELEMENT_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/cssom/style_sheet.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/url_utils.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/loader.h"
#include "cobalt/loader/text_decoder.h"

namespace cobalt {
namespace dom {

class Document;

// The link element allows authors to link their document to other resources.
//   https://www.w3.org/TR/html5/document-metadata.html#the-link-element
class HTMLLinkElement : public HTMLElement {
 public:
  static const char kTagName[];
  static const std::vector<std::string> kSupportedRelValues;

  explicit HTMLLinkElement(Document* document)
      : HTMLElement(document, base::Token(kTagName)) {}

  // Web API: HTMLLinkElement
  //
  std::string rel() const { return GetAttribute("rel").value_or(""); }
  void set_rel(const std::string& value) { SetAttribute("rel", value); }

  std::string type() const { return GetAttribute("type").value_or(""); }
  void set_type(const std::string& value) { SetAttribute("type", value); }

  std::string href() const { return GetAttribute("href").value_or(""); }
  void set_href(const std::string& value) { SetAttribute("href", value); }

  base::optional<std::string> cross_origin() const;
  void set_cross_origin(const base::optional<std::string>& value);

  // Custom, not in any spec.
  //
  scoped_refptr<HTMLLinkElement> AsHTMLLinkElement() override { return this; }

  // From Node.
  void OnInsertedIntoDocument() override;
  void OnRemovedFromDocument() override;

  DEFINE_WRAPPABLE_TYPE(HTMLLinkElement);

 private:
  ~HTMLLinkElement() override {}

  void ResolveAndSetAbsoluteURL();

  // From the spec: HTMLLinkElement.
  void Obtain();

  void OnLoadingDone(const loader::Origin& last_url_origin,
                     scoped_ptr<std::string> content);
  void OnLoadingError(const std::string& error);
  void OnSplashscreenLoaded(Document* document, const std::string& content);
  void OnStylesheetLoaded(Document* document, const std::string& content);
  void ReleaseLoader();

  // Add this element's style sheet to the style sheet vector.
  void CollectStyleSheet(cssom::StyleSheetVector* style_sheets) const override;

  // Thread checker ensures all calls to DOM element are made from the same
  // thread that it is created in.
  base::ThreadChecker thread_checker_;
  // The loader.
  scoped_ptr<loader::Loader> loader_;

  // Absolute link url.
  GURL absolute_url_;

  // The style sheet associated with this element.
  scoped_refptr<cssom::StyleSheet> style_sheet_;

  // The origin of fetch request's final destination.
  loader::Origin fetched_last_url_origin_;

  // The request mode for the fetch request.
  loader::RequestMode request_mode_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_LINK_ELEMENT_H_
