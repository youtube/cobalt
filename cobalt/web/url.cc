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
#include "cobalt/script/exception_message.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "url/gurl.h"

namespace cobalt {
namespace web {

namespace {
const char kBlobUrlProtocol[] = "blob";
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
