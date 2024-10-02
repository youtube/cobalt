// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_OBSERVATION_CROSAPI_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_OBSERVATION_CROSAPI_H_

#include <memory>

#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

class EventObservationCrosapi : public crosapi::mojom::TelemetryEventObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnEvent(const extensions::ExtensionId& extension_id,
                         crosapi::mojom::TelemetryEventInfoPtr info) = 0;
  };

  explicit EventObservationCrosapi(const extensions::ExtensionId& extension_id,
                                   content::BrowserContext* context);

  EventObservationCrosapi(const EventObservationCrosapi&) = delete;
  EventObservationCrosapi& operator=(const EventObservationCrosapi&) = delete;

  ~EventObservationCrosapi() override;

  // crosapi::mojom::TelemetryEventObserver:
  void OnEvent(crosapi::mojom::TelemetryEventInfoPtr info) override;

  // Binds a new pending remote to this implementation.
  mojo::PendingRemote<crosapi::mojom::TelemetryEventObserver> GetRemote();

  // Sets the delegate for testing, assumes ownership.
  void SetDelegateForTesting(Delegate* delegate) { delegate_.reset(delegate); }

 private:
  extensions::ExtensionId extension_id_;
  mojo::Receiver<crosapi::mojom::TelemetryEventObserver> receiver_;
  std::unique_ptr<Delegate> delegate_;
  const raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENT_OBSERVATION_CROSAPI_H_
