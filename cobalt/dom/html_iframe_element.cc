// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/html_iframe_element.h"

#include "base/memory/singleton.h"
#include "cobalt/dom/document.h"

namespace cobalt {
namespace dom {

// static
FrameFactory* FrameFactory::GetInstance() {
  return base::Singleton<FrameFactory,
                         base::LeakySingletonTraits<FrameFactory>>::get();
}

// static
int FrameFactory::count = 0;

// static
const char HTMLIframeElement::kTagName[] = "iframe";

HTMLIframeElement::HTMLIframeElement(Document* document)
    : HTMLElement(document, base::Token(kTagName)), initial_insertion_(false) {}

HTMLIframeElement::~HTMLIframeElement() {}

std::string HTMLIframeElement::src() const {
  std::string src = GetAttribute("src").value_or("");
  if (src == "" || !node_document()) {
    return src;
  }
  GURL url = node_document()->location()->url().Resolve(src);
  return url.is_valid() ? url.spec() : src;
}

void HTMLIframeElement::NavigateFrame(const GURL& url) {
  // Element, url, referrerPolicy.
  // 1. Let historyHandling be "push".
  // 2. If element's content navigable's active document is not completely
  // loaded, then set historyHandling to "replace".
  // 3. Navigate element's content navigable to url using element's node
  // document, with historyHandling set to historyHandling, referrerPolicy set
  // to referrerPolicy, and documentResource set to scrdocString.
  std::unique_ptr<Frame> content_frame =
      FrameFactory::GetInstance()->CreateFrame(
          url, base::Bind(
                   [](HTMLIframeElement* el) {
                     el->environment_settings()
                         ->context()
                         ->message_loop()
                         ->task_runner()
                         ->PostTask(
                             FROM_HERE,
                             base::BindOnce(
                                 [](HTMLIframeElement* el) {
                                   el->DispatchEvent(new web::Event("load"));
                                 },
                                 base::Unretained(el)));
                   },
                   base::Unretained(this)));
  if (!content_frame) {
    LOG(WARNING) << "Failed to create content frame for <iframe>.";
  }
  content_frame_ = std::move(content_frame);
}

void HTMLIframeElement::ProcessAttributes() {
  // 1. Let url be the result of running the shared attribute processing steps
  // for iframe and frame elements given element and initialInsertion.
  std::string url = src();
  // 2. If url is null, then return.
  if (url == "") {
    return;
  }
  // 3. If url matches about:blank and initialInsertion is true, then:
  if (url == "about:blank") {
    // 1. Run the iframe load event steps given element.
    return;
  }
  // 4. Let referrerPolicy be the current state of element's referrerpolicy
  // content attribute.
  // 5. Set element's current navigation was lazy loaded boolean to false.
  // 6. If the will lazy load element steps given element return true, then:
  //    1. Set element's lazy load resumption steps to the rest of this
  //    algorithm starting with the step labeled navigate.
  //    2. Set element's current navigation was lazy loaded boolean to true.
  //    4. Start intersection-observing a lazy loading element for element.
  //    5. Return.
  // 7. Navigate: navigate an iframe or frame given element, url, and
  // referrerPolicy.
  NavigateFrame(GURL(url));
}

void HTMLIframeElement::OnInsertedIntoDocument() {
  HTMLElement::OnInsertedIntoDocument();
  if (!initial_insertion_) {
    // 1. Create a new child navigable for element.
    // 2. If element has a sandbox attribute, then parse the sandboxing
    // directive given the attribute's value and element's iframe sandboxing
    // flag set.
    // 3. Process the iframe attributes for element, with initialInsertion set
    // to true.
    initial_insertion_ = true;
    ProcessAttributes();
  }
}

void HTMLIframeElement::OnRemovedFromDocument() {
  // When an iframe element is removed from a document, the user agent must
  // destroy a child navigable given the element.
}

void HTMLIframeElement::OnParserStartTag(
    const base::SourceLocation& opening_tag_location) {}

void HTMLIframeElement::OnParserEndTag() {}

scoped_refptr<HTMLIframeElement> HTMLIframeElement::AsHTMLIframeElement() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return this;
}

void HTMLIframeElement::GetLoadTimingInfoAndCreateResourceTiming() {}

}  // namespace dom
}  // namespace cobalt
