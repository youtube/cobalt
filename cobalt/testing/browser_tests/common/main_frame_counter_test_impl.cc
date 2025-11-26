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

#include "cobalt/testing/browser_tests/common/main_frame_counter_test_impl.h"

#include "base/no_destructor.h"
#include "content/common/main_frame_counter.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

MainFrameCounterTestImpl* GetMainFrameCounterTestImpl() {
  static base::NoDestructor<MainFrameCounterTestImpl> instance;
  return instance.get();
}

MainFrameCounterTestImpl::MainFrameCounterTestImpl() = default;
MainFrameCounterTestImpl::~MainFrameCounterTestImpl() = default;

// static
void MainFrameCounterTestImpl::Bind(
    mojo::PendingReceiver<mojom::MainFrameCounterTest> receiver) {
  GetMainFrameCounterTestImpl()->receiver_.Bind(std::move(receiver));
}

void MainFrameCounterTestImpl::HasMainFrame(HasMainFrameCallback callback) {
  std::move(callback).Run(MainFrameCounter::has_main_frame());
}

}  // namespace content
