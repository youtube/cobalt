// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/testing/browser_tests/renderer/render_frame_test_helper.h"

#include <utility>

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

void RenderFrameTestHelper::Create(
    RenderFrame& render_frame,
    mojo::PendingReceiver<mojom::RenderFrameTestHelper> receiver) {
  new RenderFrameTestHelper(render_frame, std::move(receiver));
}

RenderFrameTestHelper::~RenderFrameTestHelper() {}

void RenderFrameTestHelper::GetDocumentToken(
    GetDocumentTokenCallback callback) {
  std::move(callback).Run(render_frame()->GetWebFrame()->GetDocument().Token());
}

void RenderFrameTestHelper::OnDestruct() {
  delete this;
}

RenderFrameTestHelper::RenderFrameTestHelper(
    RenderFrame& render_frame,
    mojo::PendingReceiver<mojom::RenderFrameTestHelper> receiver)
    : RenderFrameObserver(&render_frame),
      receiver_(this, std::move(receiver)) {}

}  // namespace content
