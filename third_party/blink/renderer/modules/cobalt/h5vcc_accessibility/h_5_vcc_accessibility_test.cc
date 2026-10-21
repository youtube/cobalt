// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_accessibility/h_5_vcc_accessibility.h"

#include "base/test/gmock_callback_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

using ::testing::_;

class FakeH5vccAccessibilityService
    : public h5vcc_accessibility::mojom::blink::H5vccAccessibilityBrowser {
 public:
  FakeH5vccAccessibilityService() : receiver_(this) {}
  ~FakeH5vccAccessibilityService() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    DCHECK(!receiver_.is_bound());
    receiver_.Bind(
        mojo::PendingReceiver<
            h5vcc_accessibility::mojom::blink::H5vccAccessibilityBrowser>(
            std::move(handle)));
    receiver_.set_disconnect_handler(
        WTF::BindOnce(&FakeH5vccAccessibilityService::OnConnectionError,
                      WTF::Unretained(this)));
  }

  void OnConnectionError() {
    receiver_.reset();
    client_.reset();
  }

  MOCK_METHOD(void,
              IsTextToSpeechEnabledSync,
              (IsTextToSpeechEnabledSyncCallback callback),
              (override));

  MOCK_METHOD(
      void,
      RegisterClient,
      (::mojo::PendingRemote<
          h5vcc_accessibility::mojom::blink::H5vccAccessibilityClient> client),
      (override));

 private:
  mojo::Remote<h5vcc_accessibility::mojom::blink::H5vccAccessibilityClient>
      client_;
  mojo::Receiver<h5vcc_accessibility::mojom::blink::H5vccAccessibilityBrowser>
      receiver_;
};

class H5vccAccessibilityTest : public PageTestBase {
 public:
  H5vccAccessibilityTest()
      : h5vcc_accessibility_service_(
            std::make_unique<
                ::testing::StrictMock<FakeH5vccAccessibilityService>>()) {}

  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());

    GetFrame().DomWindow()->GetBrowserInterfaceBroker().SetBinderForTesting(
        h5vcc_accessibility::mojom::blink::H5vccAccessibilityBrowser::Name_,
        WTF::BindRepeating(&FakeH5vccAccessibilityService::BindRequest,
                           WTF::Unretained(h5vcc_accessibility_service())));
  }

  void TearDown() override {
    GetFrame().DomWindow()->GetBrowserInterfaceBroker().SetBinderForTesting(
        h5vcc_accessibility::mojom::blink::H5vccAccessibilityBrowser::Name_,
        {});
  }

  ::testing::StrictMock<FakeH5vccAccessibilityService>*
  h5vcc_accessibility_service() {
    return h5vcc_accessibility_service_.get();
  }

 private:
  std::unique_ptr<::testing::StrictMock<FakeH5vccAccessibilityService>>
      h5vcc_accessibility_service_;
};

TEST_F(H5vccAccessibilityTest, ConstructDestroy) {
  auto* window = GetFrame().DomWindow();
  auto* h5vcc_accessibility = MakeGarbageCollected<H5vccAccessibility>(*window);
  ASSERT_TRUE(h5vcc_accessibility);
}

// Verifies that textToSpeech() triggers a Mojo "remote" query.
TEST_F(H5vccAccessibilityTest, IsTextToSpeechEnabledSync) {
  auto* window = GetFrame().DomWindow();
  auto* h5vcc_accessibility = MakeGarbageCollected<H5vccAccessibility>(*window);

  base::RunLoop loop;
  auto closure = loop.QuitClosure();
  constexpr bool kValue = true;
  EXPECT_CALL(*h5vcc_accessibility_service(), IsTextToSpeechEnabledSync(_))
      .WillOnce(::testing::WithArg<0>(
          ::testing::Invoke([&closure](base::OnceCallback<void(bool)> cb) {
            // For some reason base::test::RunOnceCallback() didn't work here.
            std::move(cb).Run(kValue);
            closure.Run();
          })));
  EXPECT_EQ(h5vcc_accessibility->textToSpeech(), kValue);
  loop.Run();
}

// Verifies that H5vccAccessibility won't query the "remote"
// h5vcc_accessibility_service() if it has queried it at least once.
TEST_F(H5vccAccessibilityTest, IsTextToSpeechEnabledSyncWithCachedValue) {
  auto* window = GetFrame().DomWindow();
  auto* h5vcc_accessibility = MakeGarbageCollected<H5vccAccessibility>(*window);

  constexpr bool kValue = true;
  // First time around, textToSpeech() triggers a Mojo call.
  {
    base::RunLoop loop;
    auto closure = loop.QuitClosure();

    EXPECT_CALL(*h5vcc_accessibility_service(), IsTextToSpeechEnabledSync(_))
        .WillOnce(::testing::WithArg<0>(
            ::testing::Invoke([&closure](base::OnceCallback<void(bool)> cb) {
              // For some reason base::test::RunOnceCallback() didn't work here.
              std::move(cb).Run(kValue);
              closure.Run();
            })));
    EXPECT_EQ(h5vcc_accessibility->textToSpeech(), kValue);
    loop.Run();
  }
  // Second time around, textToSpeech() does not trigger trigger a Mojo call:
  // last fetched value is cached.
  {
    base::RunLoop loop;
    EXPECT_CALL(*h5vcc_accessibility_service(), IsTextToSpeechEnabledSync(_))
        .Times(0);
    EXPECT_EQ(h5vcc_accessibility->textToSpeech(), kValue);
    loop.RunUntilIdle();
  }
}

}  // namespace blink
