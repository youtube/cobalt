// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/web/message_port.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/web/context.h"
#include "cobalt/web/event.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/message_event.h"

namespace cobalt {
namespace web {

void MessagePort::EntangleWithEventTarget(web::EventTarget* event_target) {
  DCHECK(!event_target_);
  {
    base::AutoLock lock(mutex_);
    event_target_ = event_target;
    if (!event_target_) {
      enabled_ = false;
      return;
    }
  }
  target_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Context::AddEnvironmentSettingsChangeObserver,
                     base::Unretained(context()), base::Unretained(this)));
  remove_environment_settings_change_observer_ =
      base::BindOnce(&Context::RemoveEnvironmentSettingsChangeObserver,
                     base::Unretained(context()), base::Unretained(this));

  target_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(
          [](MessagePort* message_port,
             web::EventTarget*
                 event_target) {  // The first time a MessagePort object's
                                  // onmessage IDL attribute is set, the
            // port's port message queue must be enabled, as if the start()
            // method had been called.
            //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#messageport
            if (event_target->HasEventListener(base::Tokens::message())) {
              message_port->Start();
            } else {
              event_target->AddEventListenerRegistrationCallback(
                  message_port, base::Tokens::message(),
                  base::BindOnce(&MessagePort::Start,
                                 base::Unretained(message_port)));
            }
          },
          base::Unretained(this), base::Unretained(event_target)));
}

void MessagePort::Start() {
  // The start() method steps are to enable this's port message queue, if it is
  // not already enabled.
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#dom-messageport-start
  base::AutoLock lock(mutex_);
  if (!event_target_) {
    return;
  }
  enabled_ = true;
  for (auto& message : unshipped_messages_) {
    PostMessageSerializedLocked(std::move(message));
  }
  unshipped_messages_.clear();
}

void MessagePort::Close() {
  base::AutoLock lock(mutex_);
  unshipped_messages_.clear();
  if (!event_target_) {
    return;
  }
  event_target_->RemoveEventListenerRegistrationCallbacks(this);
  if (remove_environment_settings_change_observer_) {
    std::move(remove_environment_settings_change_observer_).Run();
  }
  event_target_ = nullptr;
}

void MessagePort::PostMessage(const script::ValueHandleHolder& message) {
  auto structured_clone = std::make_unique<script::StructuredClone>(message);
  {
    base::AutoLock lock(mutex_);
    if (!(event_target_ && enabled_)) {
      unshipped_messages_.push_back(std::move(structured_clone));
      return;
    }
    PostMessageSerializedLocked(std::move(structured_clone));
  }
}

void MessagePort::PostMessageSerializedLocked(
    std::unique_ptr<script::StructuredClone> structured_clone) {
  if (!structured_clone || !event_target_ || !enabled_) {
    return;
  }
  // TODO: Forward the location of the origating API call to the PostTask
  // call.
  //   https://html.spec.whatwg.org/multipage/workers.html#handler-worker-onmessage
  // TODO: Update MessageEvent to support more types. (b/227665847)
  // TODO: Remove dependency of MessageEvent on net iobuffer (b/227665847)
  target_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<web::EventTarget> event_target,
             std::unique_ptr<script::StructuredClone> structured_clone) {
            event_target->DispatchEvent(new web::MessageEvent(
                base::Tokens::message(), std::move(structured_clone)));
          },
          event_target_->AsWeakPtr(), std::move(structured_clone)));
}

}  // namespace web
}  // namespace cobalt
