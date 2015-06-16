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

#include "cobalt/dom/url.h"

#include "base/guid.h"
#include "base/logging.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

namespace {

const char kBlobUrlProtocol[] = "blob";

}  // namespace

// static
std::string URL::CreateObjectURL(
    const scoped_refptr<MediaSource>& media_source) {
  DCHECK(media_source);
  if (!media_source) {
    return "";
  }

  std::string blob_url = kBlobUrlProtocol;
  blob_url += ':' + base::GenerateGUID();
  MediaSource::Registry::Register(blob_url, media_source);
  return blob_url;
}

// static
void URL::RevokeObjectURL(const std::string& url) {
  DCHECK(GURL(url).SchemeIs(kBlobUrlProtocol)) << url << " is not a blob url";
  MediaSource::Registry::Unregister(url);
}

}  // namespace dom
}  // namespace cobalt
