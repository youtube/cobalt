/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/debug/page_component.h"

#include "base/bind.h"
#include "base/values.h"

namespace cobalt {
namespace debug {

namespace {
// Definitions from the set specified here:
// https://developer.chrome.com/devtools/docs/protocol/1.1/page

// Command "methods" (names):
const char kDisable[] = "Page.disable";
const char kEnable[] = "Page.enable";
const char kGetResourceTree[] = "Page.getResourceTree";

// Parameter field names:
const char kFrameId[] = "result.frameTree.frame.id";
const char kLoaderId[] = "result.frameTree.frame.loaderId";
const char kMimeType[] = "result.frameTree.frame.mimeType";
const char kResources[] = "result.frameTree.resources";
const char kSecurityOrigin[] = "result.frameTree.frame.securityOrigin";
const char kUrl[] = "result.frameTree.frame.url";

// Constant parameter values:
const char kFrameIdValue[] = "Cobalt";
const char kLoaderIdValue[] = "Cobalt";
const char kMimeTypeValue[] = "text/html";
}  // namespace

PageComponent::PageComponent(const base::WeakPtr<DebugServer>& server,
                             dom::Document* document)
    : DebugServer::Component(server), document_(document) {
  AddCommand(kDisable,
             base::Bind(&PageComponent::Disable, base::Unretained(this)));
  AddCommand(kEnable,
             base::Bind(&PageComponent::Enable, base::Unretained(this)));
  AddCommand(kGetResourceTree, base::Bind(&PageComponent::GetResourceTree,
                                          base::Unretained(this)));
}

JSONObject PageComponent::Disable(const JSONObject& params) {
  UNREFERENCED_PARAMETER(params);
  return JSONObject(new base::DictionaryValue());
}

JSONObject PageComponent::Enable(const JSONObject& params) {
  UNREFERENCED_PARAMETER(params);
  return JSONObject(new base::DictionaryValue());
}

JSONObject PageComponent::GetResourceTree(const JSONObject& params) {
  UNREFERENCED_PARAMETER(params);
  JSONObject response(new base::DictionaryValue());
  response->SetString(kFrameId, kFrameIdValue);
  response->SetString(kLoaderId, kLoaderIdValue);
  response->SetString(kMimeType, kMimeTypeValue);
  response->SetString(kSecurityOrigin, document_->url());
  response->SetString(kUrl, document_->url());
  response->Set(kResources, new base::ListValue());
  return response.Pass();
}

}  // namespace debug
}  // namespace cobalt
