// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_COBALT_CONTENTS_H_
#define COBALT_COBALT_CONTENTS_H_

#include "components/js_injection/browser/js_communication_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace cobalt {

class CobaltContents : public content::WebContentsObserver {
 public:
  // Returns the AwContents instance associated with |web_contents|, or NULL.
  static CobaltContents* FromWebContents(content::WebContents* web_contents);

  CobaltContents(std::unique_ptr<content::WebContents> web_contents);

  CobaltContents(const CobaltContents&) = delete;
  CobaltContents& operator=(const CobaltContents&) = delete;

  ~CobaltContents() override;

  js_injection::JsCommunicationHost* GetJsCommunicationHost();

  jint AddDocumentStartJavaScript(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& script,
      const base::android::JavaParamRef<jobjectArray>& allowed_origin_rules);

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<js_injection::JsCommunicationHost> js_communication_host_;
};

}  // namespace cobalt

#endif  // COBALT_COBALT_CONTENTS_H_
