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

#ifndef COBALT_SERVICES_CRASH_ANNOTATOR_CRASH_ANNOTATOR_SERVICE_H_
#define COBALT_SERVICES_CRASH_ANNOTATOR_CRASH_ANNOTATOR_SERVICE_H_

#include <string>

#include "cobalt/services/crash_annotator/public/mojom/crash_annotator_service.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace crash_annotator {

// Implements the CrashAnnotatorService Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class CrashAnnotatorService
    : public content::DocumentService<mojom::CrashAnnotatorService> {
 public:
  // Creates a CrashAnnotatorService. The CrashAnnotatorService is bound to the
  // receiver and its lifetime is scoped to the render_frame_host.
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::CrashAnnotatorService> receiver);

  CrashAnnotatorService(const CrashAnnotatorService&) = delete;
  CrashAnnotatorService& operator=(const CrashAnnotatorService&) = delete;

  void SetString(const std::string& key,
                 const std::string& value,
                 SetStringCallback callback) override;

 private:
  CrashAnnotatorService(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::CrashAnnotatorService> receiver);
};

}  // namespace crash_annotator

#endif  // COBALT_SERVICES_CRASH_ANNOTATOR_CRASH_ANNOTATOR_SERVICE_H_
