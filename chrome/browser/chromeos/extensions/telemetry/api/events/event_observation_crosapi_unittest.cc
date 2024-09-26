// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_observation_crosapi.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/test/repeating_test_future.h"
#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class EventDelegate : public EventObservationCrosapi::Delegate {
 public:
  ~EventDelegate() override = default;

  // EventManager::Delegate:
  void OnEvent(const extensions::ExtensionId& extension_id,
               crosapi::mojom::TelemetryEventInfoPtr info) override {
    future_.AddValue(std::make_tuple(extension_id, std::move(info)));
  }

  std::tuple<extensions::ExtensionId, crosapi::mojom::TelemetryEventInfoPtr>
  WaitAndGetData() {
    return future_.Take();
  }

 private:
  base::test::RepeatingTestFuture<
      std::tuple<extensions::ExtensionId,
                 crosapi::mojom::TelemetryEventInfoPtr>>
      future_;
};

}  // namespace

class TelemetryExtensionEventObservationCrosapiTest
    : public extensions::ApiUnitTest {
 public:
  TelemetryExtensionEventObservationCrosapiTest() = default;

  void SetUp() override {
    extensions::ApiUnitTest::SetUp();

    event_observation_ = std::make_unique<EventObservationCrosapi>(
        extension()->id(), browser_context());

    event_delegate_ = new EventDelegate();
    event_observation_->SetDelegateForTesting(event_delegate_);
  }

 protected:
  EventObservationCrosapi* GetEventRouter() { return event_observation_.get(); }

  EventDelegate* GetEventDelegate() { return event_delegate_; }

  mojo::Remote<crosapi::mojom::TelemetryEventObserver>& GetRemote() {
    return remote_;
  }

  void Bind(mojo::PendingRemote<crosapi::mojom::TelemetryEventObserver>
                pending_remote) {
    remote_.Bind(std::move(pending_remote));
  }

 private:
  // The observation and its delegate live as long as the test itself.
  std::unique_ptr<EventObservationCrosapi> event_observation_;
  raw_ptr<EventDelegate> event_delegate_;

  mojo::Remote<crosapi::mojom::TelemetryEventObserver> remote_;
};

TEST_F(TelemetryExtensionEventObservationCrosapiTest,
       CanObserveAudioJackEvent) {
  Bind(GetEventRouter()->GetRemote());

  auto audio_info = crosapi::mojom::TelemetryAudioJackEventInfo::New();
  audio_info->state = crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd;

  auto info = crosapi::mojom::TelemetryEventInfo::NewAudioJackEventInfo(
      std::move(audio_info));

  GetRemote()->OnEvent(std::move(info));

  // Flush so that the result shows up.
  GetRemote().FlushForTesting();

  auto result = GetEventDelegate()->WaitAndGetData();

  EXPECT_EQ(std::get<0>(result), extension()->id());
  EXPECT_EQ(std::get<1>(result),
            crosapi::mojom::TelemetryEventInfo::NewAudioJackEventInfo(
                crosapi::mojom::TelemetryAudioJackEventInfo::New(
                    crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd)));
}

}  // namespace chromeos
