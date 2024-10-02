// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_router.h"

#include <memory>

#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "extensions/browser/extensions_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class TelemetryExtensionEventRouterTest : public extensions::ExtensionsTest {
 public:
  TelemetryExtensionEventRouterTest() = default;

  void SetUp() override {
    extensions::ExtensionsTest::SetUp();

    event_router_ = std::make_unique<EventRouter>(browser_context());
  }

  EventRouter* GetEventRouter() { return event_router_.get(); }

 private:
  std::unique_ptr<EventRouter> event_router_;
};

TEST_F(TelemetryExtensionEventRouterTest, ResetReceiversForExtension) {
  constexpr char kExtensionIdOne[] = "TESTEXTENSION1";
  constexpr char kExtensionIdTwo[] = "TESTEXTENSION2";

  mojo::Remote<crosapi::mojom::TelemetryEventObserver> remote_one(
      GetEventRouter()->GetPendingRemoteForCategoryAndExtension(
          crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack,
          kExtensionIdOne));

  mojo::Remote<crosapi::mojom::TelemetryEventObserver> remote_two(
      GetEventRouter()->GetPendingRemoteForCategoryAndExtension(
          crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack,
          kExtensionIdTwo));

  ASSERT_TRUE(remote_one.is_bound());
  ASSERT_TRUE(remote_two.is_bound());

  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(
      kExtensionIdOne, crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(
      kExtensionIdTwo, crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack));

  GetEventRouter()->ResetReceiversForExtension(kExtensionIdOne);

  // Flush so the result shows up.
  remote_one.FlushForTesting();
  remote_two.FlushForTesting();

  ASSERT_FALSE(remote_one.is_connected());
  ASSERT_TRUE(remote_two.is_connected());

  EXPECT_FALSE(GetEventRouter()->IsExtensionObservingForCategory(
      kExtensionIdOne, crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack));
  EXPECT_TRUE(GetEventRouter()->IsExtensionObservingForCategory(
      kExtensionIdTwo, crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack));
}

TEST_F(TelemetryExtensionEventRouterTest, ResetReceiversOfExtensionByCategory) {
  constexpr char kExtensionIdOne[] = "TESTEXTENSION1";
  constexpr char kExtensionIdTwo[] = "TESTEXTENSION2";

  mojo::Remote<crosapi::mojom::TelemetryEventObserver> remote_one_audio(
      GetEventRouter()->GetPendingRemoteForCategoryAndExtension(
          crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack,
          kExtensionIdOne));
  mojo::Remote<crosapi::mojom::TelemetryEventObserver> remote_one_unmapped(
      GetEventRouter()->GetPendingRemoteForCategoryAndExtension(
          crosapi::mojom::TelemetryEventCategoryEnum::kUnmappedEnumField,
          kExtensionIdOne));

  mojo::Remote<crosapi::mojom::TelemetryEventObserver> remote_two(
      GetEventRouter()->GetPendingRemoteForCategoryAndExtension(
          crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack,
          kExtensionIdTwo));

  ASSERT_TRUE(remote_one_audio.is_bound());
  ASSERT_TRUE(remote_one_unmapped.is_bound());
  ASSERT_TRUE(remote_two.is_bound());

  GetEventRouter()->ResetReceiversOfExtensionByCategory(
      kExtensionIdOne, crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack);

  // Flush so the result shows up.
  remote_one_audio.FlushForTesting();
  remote_one_unmapped.FlushForTesting();
  remote_two.FlushForTesting();

  ASSERT_FALSE(remote_one_audio.is_connected());
  ASSERT_TRUE(remote_one_unmapped.is_connected());
  ASSERT_TRUE(remote_two.is_connected());
}

}  // namespace chromeos
