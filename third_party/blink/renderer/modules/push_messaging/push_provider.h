// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_PROVIDER_H_

#include <stdint.h>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-blink.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_callbacks.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace mojom {
enum class PushGetRegistrationStatus;
enum class PushRegistrationStatus;
}  // namespace mojom

class PushSubscriptionOptions;

class PushProvider final : public GarbageCollected<PushProvider>,
                           public Supplement<ServiceWorkerRegistration> {
 public:
  static const char kSupplementName[];

  explicit PushProvider(ServiceWorkerRegistration& registration);

  PushProvider(const PushProvider&) = delete;
  PushProvider& operator=(const PushProvider&) = delete;

  ~PushProvider() = default;

  static PushProvider* From(ServiceWorkerRegistration* registration);

  void Subscribe(PushSubscriptionOptions* options,
                 bool user_gesture,
                 std::unique_ptr<PushSubscriptionCallbacks> callbacks);
  void Unsubscribe(std::unique_ptr<PushUnsubscribeCallbacks> callbacks);
  void GetSubscription(std::unique_ptr<PushSubscriptionCallbacks> callbacks);

  void Trace(Visitor*) const override;

 private:
  // Returns an initialized PushMessaging service. A connection will be
  // established after the first call to this method.
  mojom::blink::PushMessaging* GetPushMessagingRemote();

  void DidSubscribe(std::unique_ptr<PushSubscriptionCallbacks> callbacks,
                    mojom::blink::PushRegistrationStatus status,
                    mojom::blink::PushSubscriptionPtr subscription);

  void DidUnsubscribe(std::unique_ptr<PushUnsubscribeCallbacks> callbacks,
                      mojom::blink::PushErrorType error_type,
                      bool did_unsubscribe,
                      const WTF::String& error_message);

  void DidGetSubscription(std::unique_ptr<PushSubscriptionCallbacks> callbacks,
                          mojom::blink::PushGetRegistrationStatus status,
                          mojom::blink::PushSubscriptionPtr subscription);

  HeapMojoRemote<mojom::blink::PushMessaging> push_messaging_manager_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_PROVIDER_H_
