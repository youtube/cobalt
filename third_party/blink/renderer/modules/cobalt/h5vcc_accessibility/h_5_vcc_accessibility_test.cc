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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_accessibility/h_5_vcc_accessibility.h"

#include "base/test/gmock_callback_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class FakeH5vccAccessibilityService final
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
  H5vccAccessibilityTest() {
    h5vcc_accessibility_service_ =
        std::make_unique<FakeH5vccAccessibilityService>();
  }

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

  FakeH5vccAccessibilityService* h5vcc_accessibility_service() {
    return h5vcc_accessibility_service_.get();
  }

 private:
  std::unique_ptr<FakeH5vccAccessibilityService> h5vcc_accessibility_service_;
};

TEST_F(H5vccAccessibilityTest, ConstructDestroy) {}

TEST_F(H5vccAccessibilityTest, IsTextToSpeechEnabledSync) {
  base::RunLoop loop;

  auto* window = GetFrame().DomWindow();
  H5vccAccessibility h5vcc_accessibility(*window);

  constexpr bool kValue = true;
  EXPECT_CALL(*h5vcc_accessibility_service(),
              IsTextToSpeechEnabledSync(::testing::_))
      .WillOnce(base::test::RunOnceCallback<0>(kValue))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));
  EXPECT_EQ(h5vcc_accessibility.textToSpeech(), kValue);
  loop.Run();
}

TEST_F(H5vccAccessibilityTest, IsTextToSpeechEnabledSyncWithCachedValue) {
  base::RunLoop loop;

  auto* window = GetFrame().DomWindow();
  H5vccAccessibility h5vcc_accessibility(*window);

  constexpr bool kValue = true;
  constexpr bool kOverwrite = false;
  h5vcc_accessibility.set_last_text_to_speech_enabled_for_testing(kOverwrite);

  EXPECT_CALL(*h5vcc_accessibility_service(),
              IsTextToSpeechEnabledSync(::testing::_))
      .WillOnce(base::test::RunOnceCallback<0>(kValue))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));
  EXPECT_EQ(h5vcc_accessibility.textToSpeech(), kOverwrite);
  loop.Run();
}

}  // namespace blink
