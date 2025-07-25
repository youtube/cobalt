// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_provider.h"

#include <utility>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/push_messaging/push_error.h"
#include "third_party/blink/renderer/modules/push_messaging/push_messaging_utils.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_options.h"
#include "third_party/blink/renderer/modules/push_messaging/push_type_converter.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
const char PushProvider::kSupplementName[] = "PushProvider";

PushProvider::PushProvider(ServiceWorkerRegistration& registration)
    : Supplement<ServiceWorkerRegistration>(registration),
      push_messaging_manager_(registration.GetExecutionContext()) {}

// static
PushProvider* PushProvider::From(ServiceWorkerRegistration* registration) {
  DCHECK(registration);

  PushProvider* provider =
      Supplement<ServiceWorkerRegistration>::From<PushProvider>(registration);

  if (!provider) {
    provider = MakeGarbageCollected<PushProvider>(*registration);
    ProvideTo(*registration, provider);
  }

  return provider;
}

// static
mojom::blink::PushMessaging* PushProvider::GetPushMessagingRemote() {
  if (!push_messaging_manager_.is_bound()) {
    GetSupplementable()
        ->GetExecutionContext()
        ->GetBrowserInterfaceBroker()
        .GetInterface(push_messaging_manager_.BindNewPipeAndPassReceiver(
            GetSupplementable()->GetExecutionContext()->GetTaskRunner(
                TaskType::kMiscPlatformAPI)));
  }

  return push_messaging_manager_.get();
}

void PushProvider::Subscribe(
    PushSubscriptionOptions* options,
    bool user_gesture,
    std::unique_ptr<PushSubscriptionCallbacks> callbacks) {
  DCHECK(callbacks);

  mojom::blink::PushSubscriptionOptionsPtr content_options_ptr =
      mojo::ConvertTo<mojom::blink::PushSubscriptionOptionsPtr>(options);

  GetPushMessagingRemote()->Subscribe(
      GetSupplementable()->RegistrationId(), std::move(content_options_ptr),
      user_gesture,
      WTF::BindOnce(&PushProvider::DidSubscribe, WrapPersistent(this),
                    std::move(callbacks)));
}

void PushProvider::DidSubscribe(
    std::unique_ptr<PushSubscriptionCallbacks> callbacks,
    mojom::blink::PushRegistrationStatus status,
    mojom::blink::PushSubscriptionPtr subscription) {
  DCHECK(callbacks);

  if (status ==
          mojom::blink::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE ||
      status == mojom::blink::PushRegistrationStatus::
                    SUCCESS_NEW_SUBSCRIPTION_FROM_PUSH_SERVICE ||
      status == mojom::blink::PushRegistrationStatus::SUCCESS_FROM_CACHE) {
    DCHECK(subscription);

    callbacks->OnSuccess(
        PushSubscription::Create(std::move(subscription), GetSupplementable()));
  } else {
    callbacks->OnError(PushError::CreateException(
        PushRegistrationStatusToPushErrorType(status),
        PushRegistrationStatusToString(status)));
  }
}

void PushProvider::Unsubscribe(
    std::unique_ptr<PushUnsubscribeCallbacks> callbacks) {
  DCHECK(callbacks);

  GetPushMessagingRemote()->Unsubscribe(
      GetSupplementable()->RegistrationId(),
      WTF::BindOnce(&PushProvider::DidUnsubscribe, WrapPersistent(this),
                    std::move(callbacks)));
}

void PushProvider::DidUnsubscribe(
    std::unique_ptr<PushUnsubscribeCallbacks> callbacks,
    mojom::blink::PushErrorType error_type,
    bool did_unsubscribe,
    const WTF::String& error_message) {
  DCHECK(callbacks);

  // ErrorTypeNone indicates success.
  if (error_type == mojom::blink::PushErrorType::NONE) {
    callbacks->OnSuccess(did_unsubscribe);
  } else {
    callbacks->OnError(PushError::CreateException(error_type, error_message));
  }
}

void PushProvider::GetSubscription(
    std::unique_ptr<PushSubscriptionCallbacks> callbacks) {
  DCHECK(callbacks);

  GetPushMessagingRemote()->GetSubscription(
      GetSupplementable()->RegistrationId(),
      WTF::BindOnce(&PushProvider::DidGetSubscription, WrapPersistent(this),
                    std::move(callbacks)));
}

void PushProvider::Trace(Visitor* visitor) const {
  visitor->Trace(push_messaging_manager_);
  Supplement::Trace(visitor);
}

void PushProvider::DidGetSubscription(
    std::unique_ptr<PushSubscriptionCallbacks> callbacks,
    mojom::blink::PushGetRegistrationStatus status,
    mojom::blink::PushSubscriptionPtr subscription) {
  DCHECK(callbacks);

  if (status == mojom::blink::PushGetRegistrationStatus::SUCCESS) {
    DCHECK(subscription);

    callbacks->OnSuccess(
        PushSubscription::Create(std::move(subscription), GetSupplementable()));
  } else {
    // We are only expecting an error if we can't find a registration.
    callbacks->OnSuccess(nullptr);
  }
}

}  // namespace blink
