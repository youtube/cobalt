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

#ifndef COBALT_BROWSER_CRASH_ANNOTATOR_CRASH_ANNOTATOR_IMPL_H_
#define COBALT_BROWSER_CRASH_ANNOTATOR_CRASH_ANNOTATOR_IMPL_H_

#include <string>

#include "cobalt/browser/crash_annotator/public/mojom/crash_annotator.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace crash_annotator {

// Implements the CrashAnnotator Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class CrashAnnotatorImpl
    : public content::DocumentService<mojom::CrashAnnotator> {
 public:
  // Creates a CrashAnnotatorImpl. The CrashAnnotatorImpl is bound to the
  // receiver and its lifetime is scoped to the render_frame_host.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::CrashAnnotator> receiver);

  CrashAnnotatorImpl(const CrashAnnotatorImpl&) = delete;
  CrashAnnotatorImpl& operator=(const CrashAnnotatorImpl&) = delete;

  void SetString(const std::string& key,
                 const std::string& value,
                 SetStringCallback callback) override;

  void GetTestValueSync(GetTestValueSyncCallback) override;

 private:
  CrashAnnotatorImpl(content::RenderFrameHost& render_frame_host,
                     mojo::PendingReceiver<mojom::CrashAnnotator> receiver);
};

}  // namespace crash_annotator

#endif  // COBALT_BROWSER_CRASH_ANNOTATOR_CRASH_ANNOTATOR_IMPL_H_
