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

#include "cobalt/dom/html_script_element.h"

#include "base/bind.h"
#include "base/string_util.h"
#include "cobalt/dom/document.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

// static
const char* HTMLScriptElement::kTagName = "script";

// static
scoped_refptr<HTMLScriptElement> HTMLScriptElement::Create(
    browser::ResourceLoaderFactory* loader_factory,
    script::ScriptRunner* script_runner) {
  return make_scoped_refptr(
      new HTMLScriptElement(loader_factory, script_runner));
}

// Algorithm for text:
//   http://www.w3.org/TR/html5/scripting-1.html#dom-script-text
std::string HTMLScriptElement::text() const {
  std::string content;

  const Node* child = first_child();
  while (child) {
    if (child->IsText()) {
      content.append(child->text_content());
    }
    child = child->next_sibling();
  }

  return content;
}

void HTMLScriptElement::AttachToDocument(Document* document) {
  Node::AttachToDocument(document);
  if (HasAttribute("src")) {
    // In Cobalt we only support asynchronous execution of loaded script, so the
    // async attribute should be present alongside src.
    if (!async())
      LOG(WARNING) << "Script element has src attribute but doesn't has async "
                      "attribute.";
    // Prepare the script and execute when it's done.
    Prepare();
  } else {
    // Immediately execute the script.
    script_runner_->Execute(text());
  }
}

HTMLScriptElement::HTMLScriptElement(
    browser::ResourceLoaderFactory* loader_factory,
    script::ScriptRunner* script_runner)
    : loader_factory_(loader_factory),
      script_runner_(script_runner),
      is_already_started_(false) {
  DCHECK(script_runner_);
}

HTMLScriptElement::~HTMLScriptElement() {}

// Algorithm for Prepare:
//   http://www.w3.org/TR/html5/scripting-1.html#prepare-a-script
// TODO(***REMOVED***): Support inline script.
void HTMLScriptElement::Prepare() {
  // If the script element is marked as having "already started", then the user
  // agent must abort these steps at this point. The script is not executed.
  if (is_already_started_) return;

  // 2. 3. 4. Not needed by Cobalt.

  // 5. If the element is not in a Document, then the user agent must abort
  // these steps at this point. The script is not executed.
  if (!owner_document()) return;

  // 6. If either:
  //    the script element has a type attribute and its value is the empty
  //      string, or
  //    the script element has no type attribute but it has a language attribute
  //      and that attribute's value is the empty string, or
  //    the script element has neither a type attribute nor a language
  //      attribute, then
  // ...let the script block's type for this script element be
  // "text/javascript".
  // Otherwise, if the script element has a type attribute, let the script
  // block's type for this script element be the value of that attribute with
  // any leading or trailing sequences of space characters removed.
  // Otherwise, the element has a non-empty language attribute; let the script
  // block's type for this script element be the concatenation of the string
  // "text/" followed by the value of the language attribute.
  if (type() == "") {
    set_type("text/javascript");
  } else {
    std::string trimmed_type;
    TrimWhitespace(type(), TRIM_ALL, &trimmed_type);
    set_type(trimmed_type);
  }

  // 7. If the user agent does not support the scripting language given by the
  // script block's type for this script element, then the user agent must abort
  // these steps at this point. The script is not executed.
  if (type() != "text/javascript") return;

  // 8. Not needed by Cobalt.

  // 9. The user agent must set the element's "already started" flag.
  is_already_started_ = true;

  // 10. ~ 13. Not needed by Cobalt.

  // 14. If the element has a src content attribute, run these substeps:
  //   1. Let src be the value of the element's src attribute.
  //   2. If src is the empty string, queue a task to fire a simple event
  // named error at the element, and abort these steps.
  if (src() == "") {
    // TODO(***REMOVED***): Report src is empty.
    LOG(ERROR) << "src attribute of script element is empty.";
    return;
  }

  //   3. Resolve src relative to the element.
  //   4. If the previous step failed, queue a task to fire a simple event named
  // error at the element, and abort these steps.
  const GURL base_url = owner_document()->url_as_gurl();
  const GURL url = base_url.Resolve(src());
  if (!url.is_valid()) {
    // TODO(***REMOVED***): Report URL cannot be resolved.
    LOG(ERROR) << src() << " cannot be resolved based on " << base_url;
    return;
  }

  //   5. Do a potentially CORS-enabled fetch of the resulting absolute URL,
  // with the mode being the current state of the element's crossorigin
  // content attribute, the origin being the origin of the script element's
  // Document, and the default origin behaviour set to taint.
  text_load_ = make_scoped_ptr(new browser::TextLoad(
      owner_document().get(), loader_factory_, url,
      base::Bind(&HTMLScriptElement::OnLoadingDone, this),
      base::Bind(&HTMLScriptElement::OnLoadingError, this),
      base::Bind(&HTMLScriptElement::StopLoading, this)));

  // 15. Not needed by Cobalt.
}

void HTMLScriptElement::OnLoadingDone(const std::string& content) {
  // TODO(***REMOVED***) Consider passing in a callback rather than an interface.
  script_runner_->Execute(content);
}

void HTMLScriptElement::OnLoadingError(
    const browser::ResourceLoaderError& error) {}

bool HTMLScriptElement::IsLoading() const { return text_load_.get() != NULL; }

void HTMLScriptElement::StopLoading() {
  DCHECK(IsLoading());
  text_load_.reset();
}

}  // namespace dom
}  // namespace cobalt
