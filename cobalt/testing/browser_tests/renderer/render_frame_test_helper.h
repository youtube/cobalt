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

#ifndef COBALT_TESTING_BROWSER_TESTS_RENDERER_RENDER_FRAME_TEST_HELPER_H_
#define COBALT_TESTING_BROWSER_TESTS_RENDERER_RENDER_FRAME_TEST_HELPER_H_

#include "cobalt/testing/browser_tests/common/render_frame_test_helper.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class RenderFrameTestHelper : public mojom::RenderFrameTestHelper,
                              public RenderFrameObserver {
 public:
  // Creates a new instance that deletes itself when the RenderFrame is
  // destroyed.
  static void Create(
      RenderFrame& render_frame,
      mojo::PendingReceiver<mojom::RenderFrameTestHelper> receiver);

  ~RenderFrameTestHelper() override;

  // mojom::RenderFrameTestHelper overrides:
  void GetDocumentToken(GetDocumentTokenCallback callback) override;

  // RenderFrameObserver overrides:
  void OnDestruct() override;

 private:
  explicit RenderFrameTestHelper(
      RenderFrame& render_frame,
      mojo::PendingReceiver<mojom::RenderFrameTestHelper> receiver);

  const mojo::Receiver<mojom::RenderFrameTestHelper> receiver_;
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_RENDERER_RENDER_FRAME_TEST_HELPER_H_
