// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_HTML_ANCHOR_ELEMENT_H_
#define COBALT_DOM_HTML_ANCHOR_ELEMENT_H_

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/url_utils.h"

namespace cobalt {
namespace dom {

class Document;

// The HTML Anchor Element (<a>) defines a hyperlink to a location on the same
// page or any other page on the Web.
//   https://www.w3.org/TR/html5/text-level-semantics.html#htmlanchorelement
class HTMLAnchorElement : public HTMLElement {
 public:
  static const char kTagName[];

  explicit HTMLAnchorElement(Document* document)
      : HTMLElement(document, base::Token(kTagName)),
        ALLOW_THIS_IN_INITIALIZER_LIST(url_utils_(base::Bind(
            &HTMLAnchorElement::UpdateSteps, base::Unretained(this)))) {}

  // Web API: URLUtils (implements)
  //
  std::string href() const { return url_utils_.href(); }
  void set_href(const std::string& href) { url_utils_.set_href(href); }

  std::string protocol() const { return url_utils_.protocol(); }
  void set_protocol(const std::string& protocol) {
    url_utils_.set_protocol(protocol);
  }

  std::string host() const { return url_utils_.host(); }
  void set_host(const std::string& host) { url_utils_.set_host(host); }

  std::string hostname() const { return url_utils_.hostname(); }
  void set_hostname(const std::string& hostname) {
    url_utils_.set_hostname(hostname);
  }

  std::string port() const { return url_utils_.port(); }
  void set_port(const std::string& port) { url_utils_.set_port(port); }

  std::string pathname() const { return url_utils_.pathname(); }
  void set_pathname(const std::string& pathname) {
    url_utils_.set_pathname(pathname);
  }

  std::string hash() const { return url_utils_.hash(); }
  void set_hash(const std::string& hash) { url_utils_.set_hash(hash); }

  std::string search() const { return url_utils_.search(); }
  void set_search(const std::string& search) { url_utils_.set_search(search); }

  std::string origin() const { return url_utils_.origin(); }

  // Custom, not in any spec.
  scoped_refptr<HTMLAnchorElement> AsHTMLAnchorElement() override {
    return this;
  }

  DEFINE_WRAPPABLE_TYPE(HTMLAnchorElement);

 private:
  ~HTMLAnchorElement() override {}

  void OnSetAttribute(const std::string& name,
                      const std::string& value) override;
  void OnRemoveAttribute(const std::string& name) override;

  bool ResolveAndSetURL(const std::string& value);

  void UpdateSteps(const std::string& value);

  URLUtils url_utils_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_ANCHOR_ELEMENT_H_
