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

#ifndef COBALT_TESTING_BROWSER_TESTS_COMMON_MAIN_FRAME_COUNTER_TEST_IMPL_H_
#define COBALT_TESTING_BROWSER_TESTS_COMMON_MAIN_FRAME_COUNTER_TEST_IMPL_H_

#include "cobalt/testing/browser_tests/common/main_frame_counter_test.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class MainFrameCounterTestImpl final : public mojom::MainFrameCounterTest {
 public:
  MainFrameCounterTestImpl();
  ~MainFrameCounterTestImpl() override;
  static void Bind(mojo::PendingReceiver<mojom::MainFrameCounterTest> receiver);

  void HasMainFrame(HasMainFrameCallback) override;

 private:
  mojo::Receiver<mojom::MainFrameCounterTest> receiver_{this};
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_COMMON_MAIN_FRAME_COUNTER_TEST_IMPL_H_
