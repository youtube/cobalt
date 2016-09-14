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

#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/script/global_environment.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

// static
const char HTMLImageElement::kTagName[] = "img";

HTMLImageElement::HTMLImageElement(script::EnvironmentSettings* env_settings)
    : HTMLElement(base::polymorphic_downcast<DOMSettings*>(env_settings)
                      ->window()
                      ->document(),
                  base::Token(kTagName)),
      prevent_garbage_collection_count_(0) {}

void HTMLImageElement::OnSetAttribute(const std::string& name,
                                      const std::string& /* value */) {
  // A user agent that obtains images immediately must synchronously update the
  // image data of an img element whenever that element is created with a src
  // attribute. A user agent that obtains images immediately must also
  // synchronously update the image data of an img element whenever that element
  // has its src or crossorigin attribute set, changed, or removed.
  if (name == "src") {
    UpdateImageData();
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
  }
}

// Algorithm for UpdateTheImageData:
//   https://www.w3.org/TR/html5/embedded-content-0.html#update-the-image-data
void HTMLImageElement::UpdateImageData() {
  DCHECK(MessageLoop::current());
  TRACE_EVENT0("cobalt::dom", "HTMLImageElement::UpdateImageData()");

  // 1. Not needed by Cobalt.

  // 2. If an instance of the fetching algorithm is still running for this
  // element, then abort that algorithm, discarding any pending tasks generated
  // by that algorithm.
  // 3. Forget the img element's current image data, if any.
  if (cached_image_loaded_callback_handler_) {
    cached_image_loaded_callback_handler_.reset();
    node_document()->DecreaseLoadingCounter();
  }

  // 4. If the user agent cannot support images, or its support for images has
  // been disabled, then abort these steps.
  // 5. Otherwise, if the element has a src attribute specified and its value is
  // not the empty string, let selected source be the value of the element's src
  // attribute, and selected pixel density be 1.0. Otherwise, let selected
  // source be null and selected pixel density be undefined.
  const std::string src = GetAttribute("src").value_or("");

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
    cached_image = node_document()
                       ->html_element_context()
                       ->image_cache()
                       ->CreateCachedResource(selected_source);
    if (cached_image->TryGetResource()) {
      PreventGarbageCollectionUntilEventIsDispatched(base::Tokens::load());
      return;
    }
  } else {
    // 8. 9. Not needed by Cobalt.
    // 10. If selected source is null, then set the element to the broken state,
    // queue a task to fire a simple event named error at the img element, and
    // abort these steps.
    PreventGarbageCollectionUntilEventIsDispatched(base::Tokens::error());
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
  PreventGarbageCollection();
  cached_image_loaded_callback_handler_.reset(
      new loader::image::CachedImage::OnLoadedCallbackHandler(
          cached_image, base::Bind(&HTMLImageElement::OnLoadingSuccess,
                                   base::Unretained(this)),
          base::Bind(&HTMLImageElement::OnLoadingFailure,
                     base::Unretained(this)),
          base::Bind(&HTMLImageElement::OnLoadingError,
                     base::Unretained(this))));
  node_document()->IncreaseLoadingCounter();
}

void HTMLImageElement::OnLoadingSuccess() {
  TRACE_EVENT0("cobalt::dom", "HTMLImageElement::OnLoadingSuccess()");
  AllowGarbageCollectionAfterEventIsDispatched(base::Tokens::load());
  node_document()->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
  cached_image_loaded_callback_handler_.reset();
}

void HTMLImageElement::OnLoadingFailure() {
  TRACE_EVENT0("cobalt::dom", "HTMLImageElement::OnLoadingFailure()");
  // No event is dispatched.
  AllowGarbageCollection();
  node_document()->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
  cached_image_loaded_callback_handler_.reset();
}

void HTMLImageElement::OnLoadingError() {
  TRACE_EVENT0("cobalt::dom", "HTMLImageElement::OnLoadingError()");
  AllowGarbageCollectionAfterEventIsDispatched(base::Tokens::error());
  node_document()->DecreaseLoadingCounterAndMaybeDispatchLoadEvent();
  cached_image_loaded_callback_handler_.reset();
}

void HTMLImageElement::PreventGarbageCollectionUntilEventIsDispatched(
    base::Token event_name) {
  PreventGarbageCollection();
  AllowGarbageCollectionAfterEventIsDispatched(event_name);
}

void HTMLImageElement::AllowGarbageCollectionAfterEventIsDispatched(
    base::Token event_name) {
  PostToDispatchEventAndRunCallback(
      FROM_HERE, event_name,
      base::Bind(&HTMLImageElement::AllowGarbageCollection,
                 base::AsWeakPtr<HTMLImageElement>(this)));
}

void HTMLImageElement::PreventGarbageCollection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GE(prevent_garbage_collection_count_, 0);
  if (prevent_garbage_collection_count_++ == 0) {
    DCHECK(html_element_context());
    DCHECK(html_element_context()->script_runner());
    DCHECK(html_element_context()->script_runner()->GetGlobalEnvironment());
    html_element_context()
        ->script_runner()
        ->GetGlobalEnvironment()
        ->PreventGarbageCollection(make_scoped_refptr(this));
  }
}

void HTMLImageElement::AllowGarbageCollection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(prevent_garbage_collection_count_, 0);
  if (--prevent_garbage_collection_count_ == 0) {
    html_element_context()
        ->script_runner()
        ->GetGlobalEnvironment()
        ->AllowGarbageCollection(make_scoped_refptr(this));
  }
}

}  // namespace dom
}  // namespace cobalt
