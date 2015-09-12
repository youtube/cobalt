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

#include "cobalt/dom/html_image_element.h"

#include <string>

#include "base/message_loop.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/html_element_context.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

// static
const char HTMLImageElement::kTagName[] = "img";

HTMLImageElement::HTMLImageElement(script::EnvironmentSettings* env_settings)
    : HTMLElement(base::polymorphic_downcast<DOMSettings*>(env_settings)
                      ->window()
                      ->document()) {}

std::string HTMLImageElement::tag_name() const { return kTagName; }

void HTMLImageElement::set_src(const std::string& src) {
  SetAttribute("src", src);
  UpdateImageData();
}

// Algorithm for UpdateTheImageData:
//   http://www.w3.org/TR/html5/embedded-content-0.html#update-the-image-data
// TODO(***REMOVED***): This function should be called whenever src is set, changed or
// removed.
void HTMLImageElement::UpdateImageData() {
  // 2. If an instance of the fetching algorithm is still running for this
  // element, then abort that algorithm, discarding any pending tasks generated
  // by that algorithm.
  if (cached_image_ && cached_image_->IsLoading()) {
    cached_image_->StopLoading();
    cached_image_loaded_callback_handler_.reset();
  }

  // 3. Forget the img element's current image data, if any.
  cached_image_ = NULL;

  // 7. If selected source is not null, run these substeps:
  // 7.1. Resolve selected source, relative to the element. If that is not
  // successful, abort these steps.
  const GURL base_url = owner_document()->url_as_gurl();
  const std::string src = GetAttribute("src").value_or("");
  const GURL url = base_url.Resolve(src);
  if (!url.is_valid()) {
    LOG(WARNING) << src << " cannot be resolved based on " << base_url << ".";
    return;
  }

  // 7.2 Let key be a tuple consisting of the resulting absolute URL, the img
  // element's crossorigin attribute's mode, and, if that mode is not No CORS,
  // the Document object's origin.
  // 7.3 If the list of available images contains an entry for key, then set the
  // img element to the completely available state, update the presentation of
  // the image appropriately, queue a task to fire a simple event named load at
  // the img element, and abort these steps.
  // 8. ~ 13. Loading the image.
  cached_image_ = html_element_context()->image_cache()->CreateCachedImage(url);
  if (cached_image_->TryGetImage()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&HTMLImageElement::DispatchEvent),
                   base::AsWeakPtr<HTMLImageElement>(this),
                   make_scoped_refptr(new Event("load"))));
    return;
  }
  base::Closure callback =
      base::Bind(&HTMLImageElement::OnImageLoaded, base::Unretained(this));
  cached_image_loaded_callback_handler_.reset(
      new loader::CachedImage::OnLoadedCallbackHandler(cached_image_,
                                                       callback));
}

void HTMLImageElement::OnImageLoaded() {
  DispatchEvent(new Event("load"));
  cached_image_loaded_callback_handler_.reset();
}

}  // namespace dom
}  // namespace cobalt
