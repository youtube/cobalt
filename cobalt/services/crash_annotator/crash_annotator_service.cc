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

#include "cobalt/services/crash_annotator/crash_annotator_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace crash_annotator {

CrashAnnotatorService::CrashAnnotatorService(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::CrashAnnotatorService> receiver)
    : content::DocumentService<mojom::CrashAnnotatorService>(
          render_frame_host,
          std::move(receiver)) {}

void CrashAnnotatorService::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::CrashAnnotatorService> receiver) {
  new CrashAnnotatorService(*render_frame_host, std::move(receiver));
}

void CrashAnnotatorService::SetString(const std::string& key,
                                      const std::string& value,
                                      SetStringCallback callback) {
  // TODO(cobalt, b/383301493): actually implement this.
  LOG(INFO) << "CrashAnnotatorImpl::SetString key=" << key
            << " value=" << value;
  std::move(callback).Run(false);
}

}  // namespace crash_annotator
