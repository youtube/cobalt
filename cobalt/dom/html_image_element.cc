// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/html_image_element.h"

#include <memory>
#include <string>
#include <utility>  // for std::move

#include "base/message_loop/message_loop.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/window.h"
#include "cobalt/script/global_environment.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

// static
const char HTMLImageElement::kTagName[] = "img";

HTMLImageElement::HTMLImageElement(script::EnvironmentSettings* env_settings)
    : HTMLElement(base::polymorphic_downcast<DOMSettings*>(env_settings)
                      ->window()
                      ->document()
                      .get(),
                  base::Token(kTagName)) {}

void HTMLImageElement::PurgeCachedBackgroundImagesOfNodeAndDescendants() {
  if (!cached_image_loaded_callback_handler_) {
    return;
  }

  // While we are still loading, treat this as an error.
  OnLoadingError();
}

void HTMLImageElement::OnSetAttribute(const std::string& name,
                                      const std::string& value) {
  // A user agent that obtains images immediately must synchronously update the
  // image data of an img element whenever that element is created with a src
  // attribute. A user agent that obtains images immediately must also
  // synchronously update the image data of an img element whenever that element
  // has its src or crossorigin attribute set, changed, or removed.
  if (name == "src") {
    UpdateImageData();
  } else {
    HTMLElement::OnSetAttribute(name, value);
  }
}

void HTMLImageElement::OnRemoveAttribute(const std::string& name) {
  // A user agent that obtains images immediately must synchronously update the
  // image data of an img element whenever that element is created with a src
  // attribute. A user agent that obtains images immediately must also
  // synchronously update the image data of an img element whenever that element
  // has its src or crossorigin attribute set, changed, or removed.
  if (name == "src") {
    UpdateImageData();
  } else {
    HTMLElement::OnRemoveAttribute(name);
  }
}

// Algorithm for UpdateTheImageData:
//   https://www.w3.org/TR/html50/embedded-content-0.html#update-the-image-data
void HTMLImageElement::UpdateImageData() {
  DCHECK(base::MessageLoop::current());
  DCHECK(node_document());
  TRACE_EVENT0("cobalt::dom", "HTMLImageElement::UpdateImageData()");

  // 1. Not needed by Cobalt.

  // 2. If an instance of the fetching algorithm is still running for this
  // element, then abort that algorithm, discarding any pending tasks generated
  // by that algorithm.
  // 3. Forget the img element's current image data, if any.
  if (cached_image_loaded_callback_handler_) {
    cached_image_loaded_callback_handler_.reset();
    prevent_gc_until_load_complete_.reset();
    node_document()->DecreaseLoadingCounter();
  }

  // Keep the old weak cached image reference (if it exists) alive until after
  // we're done updating to the new one.
  std::unique_ptr<loader::image::WeakCachedImage> old_weak_cached_image =
      std::move(weak_cached_image_);

  // 4. If the user agent cannot support images, or its support for images has
  // been disabled, then abort these steps.
  // 5. Otherwise, if the element has a src attribute specified and its value is
  // not the empty string, let selected source be the value of the element's src
  // attribute, and selected pixel density be 1.0. Otherwise, let selected
  // source be null and selected pixel density be undefined.
  const auto src_attr = GetAttribute("src");
  const std::string src = src_attr.value_or("");

  // 6. Not needed by Cobalt.

  // 7. If selected source is not null, run these substeps:
  scoped_refptr<loader::image::CachedImage> cached_image;

  if (!src.empty()) {
    // 7.1. Resolve selected source, relative to the element. If that is not
    // successful, abort these steps.
    const GURL& base_url = node_document()->url_as_gurl();
    const GURL selected_source = base_url.Resolve(src);
    if (!selected_source.is_valid()) {
      LOG(WARNING) << src << " cannot be resolved based on " << base_url << ".";
      return;
    }

    // 7.2 Let key be a tuple consisting of the resulting absolute URL, the img
    // element's cross origin attribute's mode, and, if that mode is not No
    // CORS, the Document object's origin.
    // 7.3 If the list of available images contains an entry for key, then set
    // the img element to the completely available state, update the
    // presentation of the image appropriately, queue a task to fire a simple
    // event named load at the img element, and abort these steps.
    auto image_cache = node_document()->html_element_context()->image_cache();
    cached_image = image_cache->GetOrCreateCachedResource(selected_source,
                                                          loader::Origin());
    DCHECK(cached_image);
    weak_cached_image_ = image_cache->CreateWeakCachedResource(cached_image);
    DCHECK(weak_cached_image_);

    if (cached_image->TryGetResource()) {
      PreventGarbageCollectionUntilEventIsDispatched(base::Tokens::load());
      return;
    }
  } else {
    // 8. 9. Not needed by Cobalt.
    // 10. If selected source is null, then set the element to the broken state,
    // queue a task to fire a simple event named error at the img element, and
    // abort these steps.
    if (src_attr) {
      PreventGarbageCollectionUntilEventIsDispatched(base::Tokens::error());
    }
    return;
  }

  // 11. Not needed by Cobalt.

  // 12. Do a potentially CORS-enabled fetch of the absolute URL that resulted
  // from the earlier step, with the mode being the current state of the
  // element's crossorigin content attribute, the origin being the origin of the
  // img element's Document, and the default origin behaviour set to taint.
  // 13. Not needed by Cobalt.
  // 14. If the download was successful, fire a simple event named load at the
  // img element. Otherwise, queue a task to first fire a simple event named
  // error at the img element.
  DCHECK(!prevent_gc_until_load_complete_);
  prevent_gc_until_load_complete_.reset(
      new script::GlobalEnvironment::ScopedPreventGarbageCollection(
          html_element_context()->script_runner()->GetGlobalEnvironment(),
          this));
  node_document()->IncreaseLoadingCounter();
  cached_image_loaded_callback_handler_.reset(
      new loader::image::CachedImage::OnLoadedCallbackHandler(
          cached_image,
          base::Bind(&HTMLImageElement::OnLoadingSuccess,
                     base::Unretained(this)),
          base::Bind(&HTMLImageElement::OnLoadingError,
                     base::Unretained(this))));
}

void HTMLImageElement::OnLoadingSuccess() {
  TRACE_EVENT0("cobalt::dom", "HTMLImageElement::OnLoadingSuccess()");
  AllowGarbageCollectionAfterEventIsDispatched(
      base::Tokens::load(), std::move(prevent_gc_until_load_complete_));
  if (node_document()) {
    node_document()->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
  }
  GetLoadTimingInfoAndCreateResourceTiming();
  cached_image_loaded_callback_handler_.reset();
}

void HTMLImageElement::OnLoadingError() {
  TRACE_EVENT0("cobalt::dom", "HTMLImageElement::OnLoadingError()");
  AllowGarbageCollectionAfterEventIsDispatched(
      base::Tokens::error(), std::move(prevent_gc_until_load_complete_));
  if (node_document()) {
    node_document()->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
  }
  GetLoadTimingInfoAndCreateResourceTiming();
  cached_image_loaded_callback_handler_.reset();
}

void HTMLImageElement::PreventGarbageCollectionUntilEventIsDispatched(
    base::Token event_name) {
  std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>
      prevent_gc_until_event_dispatch(
          new script::GlobalEnvironment::ScopedPreventGarbageCollection(
              html_element_context()->script_runner()->GetGlobalEnvironment(),
              this));
  AllowGarbageCollectionAfterEventIsDispatched(
      event_name, std::move(prevent_gc_until_event_dispatch));
}

void HTMLImageElement::AllowGarbageCollectionAfterEventIsDispatched(
    base::Token event_name,
    std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>
        scoped_prevent_gc) {
  PostToDispatchEventNameAndRunCallback(
      FROM_HERE, event_name,
      base::Bind(&HTMLImageElement::DestroyScopedPreventGC,
                 base::AsWeakPtr<HTMLImageElement>(this),
                 base::Passed(&scoped_prevent_gc)));
}

void HTMLImageElement::DestroyScopedPreventGC(
    std::unique_ptr<script::GlobalEnvironment::ScopedPreventGarbageCollection>
        scoped_prevent_gc) {
  scoped_prevent_gc.reset();
}

void HTMLImageElement::GetLoadTimingInfoAndCreateResourceTiming() {
  if (html_element_context()->performance() == nullptr) return;
  // Resolve selected source, relative to the element.
  const auto src_attr = GetAttribute("src");
  const std::string src = src_attr.value_or("");
  const GURL& base_url = node_document()->url_as_gurl();
  const GURL selected_source = base_url.Resolve(src);

  html_element_context()->performance()->CreatePerformanceResourceTiming(
      cached_image_loaded_callback_handler_->GetLoadTimingInfo(),
      kTagName, selected_source.spec());
}

}  // namespace dom
}  // namespace cobalt
