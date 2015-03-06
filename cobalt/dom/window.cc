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

#include "cobalt/dom/window.h"

#include "cobalt/dom/document.h"
#include "cobalt/dom/document_builder.h"
#include "cobalt/dom/location.h"
#include "cobalt/dom/html_element_factory.h"
#include "cobalt/dom/navigator.h"

namespace cobalt {
namespace dom {

Window::Window(int width, int height, cssom::CSSParser* css_parser,
               loader::FetcherFactory* fetcher_factory,
               script::ScriptRunner* script_runner, const GURL& url,
               const std::string& user_agent)
    : width_(width),
      height_(height),
      html_element_factory_(
          new HTMLElementFactory(fetcher_factory, css_parser, script_runner)),
      document_(
          new Document(html_element_factory_.get(), Document::Options(url))),
      document_builder_(new DocumentBuilder(
          document_, url, fetcher_factory, html_element_factory_.get(),
          DocumentBuilder::DoneCallbackType(),
          DocumentBuilder::ErrorCallbackType())),
      navigator_(new Navigator(user_agent)) {}

const scoped_refptr<Document>& Window::document() const { return document_; }

scoped_refptr<Location> Window::location() const {
  return document_->location();
}

const scoped_refptr<Navigator>& Window::navigator() const { return navigator_; }

Window::~Window() {}

}  // namespace dom
}  // namespace cobalt
