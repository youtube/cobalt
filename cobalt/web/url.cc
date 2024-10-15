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

#include "cobalt/web/url.h"

#include "base/guid.h"
#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/media_settings.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "url/gurl.h"

namespace cobalt {
namespace web {

namespace {
const char kBlobUrlProtocol[] = "blob";

const dom::MediaSettings& GetMediaSettings(web::EnvironmentSettings* settings) {
  DCHECK(settings);
  DCHECK(settings->context());
  DCHECK(settings->context()->web_settings());

  const auto& web_settings = settings->context()->web_settings();
  return web_settings->media_settings();
}

// If this function returns true, MediaSource::GetSeekable() will short-circuit
// getting the buffered range from HTMLMediaElement by directly calling to
// MediaSource::GetBufferedRange(). This reduces potential cross-object,
// cross-thread calls between MediaSource and HTMLMediaElement.
// The default value is false.
bool IsMediaElementUsingMediaSourceBufferedRangeEnabled(
    web::EnvironmentSettings* settings) {
  return GetMediaSettings(settings)
      .IsMediaElementUsingMediaSourceBufferedRangeEnabled()
      .value_or(false);
}

// If this function returns true, communication between HTMLMediaElement and
// MediaSource objects will be fully proxied between MediaSourceAttachment.
// The default value is false.
bool IsMediaElementUsingMediaSourceAttachmentMethodsEnabled(
    web::EnvironmentSettings* settings) {
  return GetMediaSettings(settings)
      .IsMediaElementUsingMediaSourceAttachmentMethodsEnabled()
      .value_or(false);
}

// If this function returns true, experimental support for creating MediaSource
// objects in Dedicated Workers will be enabled. This also allows MSE handles
// to be transferred from Dedicated Workers back to the main thread.
// Requires MediaElement.EnableUsingMediaSourceBufferedRange and
// MediaElement.EnableUsingMediaSourceAttachmentMethods as prerequisites for
// this feature.
// The default value is false.
bool IsMseInWorkersEnabled(web::EnvironmentSettings* settings) {
  return GetMediaSettings(settings).IsMseInWorkersEnabled().value_or(false);
}
}  // namespace

URL::URL(const std::string& url, const std::string& base,
         script::ExceptionState* exception_state)
    : URLUtils() {
  // Algorithm for URL constructor:
  //   https://www.w3.org/TR/2014/WD-url-1-20141209/#dom-url-url
  // 1. Let parsedBase be the result of running the basic URL parser on base.
  GURL parsed_base(base);

  // 2. If parsedBase is failure, throw a TypeError exception.
  if (parsed_base.is_empty()) {
    exception_state->SetSimpleException(script::kSimpleTypeError);
    return;
  }
  // 3. Set parsedURL to the result of running the basic URL parser on url
  //    with parsedBase.
  GURL parsed_url = parsed_base.Resolve(url);

  // 4. If parsedURL is failure, throw a TypeError exception.
  if (parsed_url.is_empty()) {
    exception_state->SetSimpleException(script::kSimpleTypeError);
    return;
  }

  // 5. Let result be a new URL object.
  // 6. Let result’s get the base return parsedBase.
  // 7. Run result’s set the input given the empty string and parsedURL.
  set_url(parsed_url);

  // 8. Return result.
}

// static
std::string URL::CreateObjectURL(
    script::EnvironmentSettings* environment_settings,
    const scoped_refptr<dom::MediaSource>& media_source) {
  if (!media_source) {
    return "";
  }

  web::EnvironmentSettings* web_settings =
      base::polymorphic_downcast<web::EnvironmentSettings*>(
          environment_settings);
  DCHECK(web_settings);
  DCHECK(web_settings->context());
  web::WindowOrWorkerGlobalScope* global_scope =
      web_settings->context()->GetWindowOrWorkerGlobalScope();
  if (global_scope->IsWorker()) {
    if (!global_scope->IsDedicatedWorker()) {
      // MSE-in-Workers is only available from DedicatedWorkers, but the URL
      // API is exposed to multiple types of Workers. This branch is used to
      // handle Workers that don't support MSE.
      return "";
    }
    if (!IsMseInWorkersEnabled(web_settings) ||
        !IsMediaElementUsingMediaSourceAttachmentMethodsEnabled(web_settings) ||
        !IsMediaElementUsingMediaSourceBufferedRangeEnabled(web_settings)) {
      // Prevent usage of MSE-in-Workers if any of the pre-requisites are not
      // satisfied.
      return "";
    }
  }

  std::string blob_url = kBlobUrlProtocol;
  blob_url += ':' + base::GenerateGUID();
  dom::RegisterMediaSourceObjectURL(environment_settings, blob_url,
                                    media_source);
  return blob_url;
}

// static
std::string URL::CreateObjectURL(
    script::EnvironmentSettings* environment_settings,
    const scoped_refptr<Blob>& blob) {
  EnvironmentSettings* web_settings =
      base::polymorphic_downcast<EnvironmentSettings*>(environment_settings);
  DCHECK(web_settings);
  DCHECK(web_settings->context()->blob_registry());
  if (!blob) {
    return "";
  }

  std::string blob_url = kBlobUrlProtocol;
  blob_url += ':' + base::GenerateGUID();
  web_settings->context()->blob_registry()->Register(blob_url, blob);
  return blob_url;
}

// static
void URL::RevokeObjectURL(script::EnvironmentSettings* environment_settings,
                          const std::string& url) {
  // 1. If the url refers to a Blob that has a readability state of CLOSED OR if
  // the value provided for the url argument is not a Blob URL, OR if the value
  // provided for the url argument does not have an entry in the Blob URL Store,
  // this method call does nothing. User agents may display a message on the
  // error console.
  if (!GURL(url).SchemeIs(kBlobUrlProtocol)) {
    LOG(WARNING) << "URL is not a Blob URL.";
    return;
  }

  // 2. Otherwise, user agents must remove the entry from the Blob URL Store for
  // url.
  if (!dom::UnregisterMediaSourceObjectURL(environment_settings, url) &&
      !base::polymorphic_downcast<EnvironmentSettings*>(environment_settings)
           ->context()
           ->blob_registry()
           ->Unregister(url)) {
    DLOG(WARNING) << "Cannot find object for blob url " << url;
  }
}

bool URL::BlobResolver(Blob::Registry* registry, const GURL& url,
                       const char** data, size_t* size) {
  DCHECK(data);
  DCHECK(size);

  *size = 0;
  *data = NULL;

  web::Blob* blob = registry->Retrieve(url.spec()).get();

  if (blob) {
    *size = static_cast<size_t>(blob->size());

    if (*size > 0) {
      *data = reinterpret_cast<const char*>(blob->data());
    }

    return true;
  } else {
    return false;
  }
}

}  // namespace web
}  // namespace cobalt
