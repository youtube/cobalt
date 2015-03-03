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

#include "cobalt/browser/loader/resource_loader_factory.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/document_builder.h"
#include "cobalt/dom/location.h"
#include "cobalt/dom/navigator.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/script/script_runner.h"

namespace cobalt {
namespace dom {

Window::Window(int width, int height, cssom::CSSParser* css_parser,
               browser::ResourceLoaderFactory* resource_loader_factory,
               script::ScriptRunner* script_runner, const GURL& url,
               const std::string& user_agent)
    : width_(width),
      height_(height),
      html_element_factory_(resource_loader_factory, css_parser, script_runner),
      document_builder_(DocumentBuilder::Create(resource_loader_factory,
                                                &html_element_factory_)),
      document_(make_scoped_refptr(
          new Document(&html_element_factory_, Document::Options(url)))),
      navigator_(make_scoped_refptr(new Navigator(user_agent))) {
  document_builder_->BuildDocument(url, document_.get());
}

const scoped_refptr<Document>& Window::document() const { return document_; }

scoped_refptr<Location> Window::location() const {
  return document_->location();
}

const scoped_refptr<Navigator>& Window::navigator() const { return navigator_; }

Window::~Window() {}

}  // namespace dom
}  // namespace cobalt
