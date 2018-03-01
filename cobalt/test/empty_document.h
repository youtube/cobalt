// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_TEST_EMPTY_DOCUMENT_H_
#define COBALT_TEST_EMPTY_DOCUMENT_H_

#include "cobalt/base/application_state.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/html_element_context.h"

namespace cobalt {
namespace test {

// This is a helper class for tests that want an empty dom::Document, which can
// be used to create other dom::Element instances.
class EmptyDocument {
 public:
  EmptyDocument()
      : css_parser_(css_parser::Parser::Create()),
        dom_stat_tracker_(new dom::DomStatTracker("EmptyDocument")),
        html_element_context_(NULL, css_parser_.get(), NULL, NULL, NULL, NULL,
                              NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                              dom_stat_tracker_.get(), "",
                              base::kApplicationStateStarted, NULL),
        document_(new dom::Document(&html_element_context_)) {}

  dom::Document* document() { return document_.get(); }

 private:
  scoped_ptr<css_parser::Parser> css_parser_;
  scoped_ptr<dom::DomStatTracker> dom_stat_tracker_;
  dom::HTMLElementContext html_element_context_;
  scoped_refptr<dom::Document> document_;
};
}  // namespace test
}  // namespace cobalt
#endif  // COBALT_TEST_EMPTY_DOCUMENT_H_
