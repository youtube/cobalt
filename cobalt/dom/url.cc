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

#include "cobalt/dom/url.h"

#include "base/guid.h"
#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

namespace {

const char kBlobUrlProtocol[] = "blob";

}  // namespace

// static
std::string URL::CreateObjectURL(
    script::EnvironmentSettings* environment_settings,
    const scoped_refptr<MediaSource>& media_source) {
  DOMSettings* dom_settings =
      base::polymorphic_downcast<DOMSettings*>(environment_settings);
  DCHECK(dom_settings);
  DCHECK(dom_settings->media_source_registry());
  if (!media_source) {
    return "";
  }

  std::string blob_url = kBlobUrlProtocol;
  blob_url += ':' + base::GenerateGUID();
  dom_settings->media_source_registry()->Register(blob_url, media_source);
  return blob_url;
}

// static
std::string URL::CreateObjectURL(
    script::EnvironmentSettings* environment_settings,
    const scoped_refptr<Blob>& blob) {
  DOMSettings* dom_settings =
      base::polymorphic_downcast<DOMSettings*>(environment_settings);
  DCHECK(dom_settings);
  DCHECK(dom_settings->blob_registry());
  if (!blob) {
    return "";
  }

  std::string blob_url = kBlobUrlProtocol;
  blob_url += ':' + base::GenerateGUID();
  dom_settings->blob_registry()->Register(blob_url, blob);
  return blob_url;
}

// static
void URL::RevokeObjectURL(script::EnvironmentSettings* environment_settings,
                          const std::string& url) {
  DOMSettings* dom_settings =
      base::polymorphic_downcast<DOMSettings*>(environment_settings);
  DCHECK(dom_settings);
  DCHECK(dom_settings->media_source_registry());

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
  if (!dom_settings->media_source_registry()->Unregister(url) &&
      !dom_settings->blob_registry()->Unregister(url)) {
    DLOG(WARNING) << "Cannot find object for blob url " << url;
  }
}

bool URL::BlobResolver(dom::Blob::Registry* registry, const GURL& url,
                       const char** data, size_t* size) {
  DCHECK(data);
  DCHECK(size);

  *size = 0;
  *data = NULL;

  dom::Blob* blob = registry->Retrieve(url.spec()).get();

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

}  // namespace dom
}  // namespace cobalt
