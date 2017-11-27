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

#ifndef COBALT_DOM_HTML_IMAGE_ELEMENT_H_
#define COBALT_DOM_HTML_IMAGE_ELEMENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/loader/image/image_cache.h"
#include "cobalt/script/environment_settings.h"

namespace cobalt {
namespace dom {

class Document;

// An img element represents an image.
//   https://www.w3.org/TR/html5/embedded-content-0.html#the-img-element
class HTMLImageElement : public HTMLElement {
 public:
  static const char kTagName[];

  explicit HTMLImageElement(Document* document)
      : HTMLElement(document, base::Token(kTagName)),
        prevent_garbage_collection_count_(0) {}

  explicit HTMLImageElement(script::EnvironmentSettings* env_settings);

  // Web API: HTMLImageElement
  //
  std::string src() const { return GetAttribute("src").value_or(""); }
  void set_src(const std::string& src) { SetAttribute("src", src); }

  // Custom, not in any spec.
  //
  scoped_refptr<HTMLImageElement> AsHTMLImageElement() override { return this; }

  DEFINE_WRAPPABLE_TYPE(HTMLImageElement);

 private:
  ~HTMLImageElement() override {}

  // From Element.
  void OnSetAttribute(const std::string& name,
                      const std::string& value) override;
  void OnRemoveAttribute(const std::string& name) override;

  // From the spec: HTMLImageElement.
  void UpdateImageData();

  void OnLoadingSuccess();
  void OnLoadingError();

  void PreventGarbageCollectionUntilEventIsDispatched(base::Token event_name);
  void AllowGarbageCollectionAfterEventIsDispatched(base::Token event_name);
  void PreventGarbageCollection();
  void AllowGarbageCollection();

  base::ThreadChecker thread_checker_;
  scoped_ptr<loader::image::CachedImage::OnLoadedCallbackHandler>
      cached_image_loaded_callback_handler_;
  int prevent_garbage_collection_count_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_IMAGE_ELEMENT_H_
