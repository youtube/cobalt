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

#include "cobalt/worker/window_client.h"

#include "cobalt/dom/document.h"
#include "cobalt/dom/visibility_state.h"
#include "cobalt/dom/window.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/window_or_worker_global_scope.h"

namespace cobalt {
namespace worker {

WindowClient::WindowClient(const WindowData& window_data)
    : Client(window_data.client) {
  // Algorithm for Create Window Client:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#create-window-client

  // 1. Let windowClient be a new WindowClient object.
  // 2. Set windowClient’s service worker client to client.
  // Passed to Client constructor.

  // 3. Set windowClient’s frame type to frameType.
  set_frame_type(window_data.frame_type);

  // 4. Set windowClient’s visibility state to visibilityState.
  // 5. Set windowClient’s focus state to focusState.
  // Determined in the property getter.

  // 6. Set windowClient’s ancestor origins array to a frozen array created from
  //    ancestorOriginsList.
  // Not supported by Cobalt.

  // 7. Return windowClient.
}

dom::VisibilityState WindowClient::visibility_state() {
  DCHECK(event_target());
  DCHECK(event_target()->environment_settings());
  web::Context* context = event_target()->environment_settings()->context();
  DCHECK(context);
  base::MessageLoop* message_loop = context->message_loop();
  if (!message_loop) {
    return dom::kVisibilityStateHidden;
  }

  dom::VisibilityState visibility_state = dom::kVisibilityStateHidden;
  if (message_loop->task_runner()->BelongsToCurrentThread()) {
    DCHECK(context->GetWindowOrWorkerGlobalScope()->IsWindow());
    DCHECK(context->GetWindowOrWorkerGlobalScope()->AsWindow()->document());
    visibility_state = context->GetWindowOrWorkerGlobalScope()
                           ->AsWindow()
                           ->document()
                           ->visibility_state();
  } else {
    message_loop->task_runner()->PostBlockingTask(
        FROM_HERE,
        base::Bind(
            [](web::Context* context, dom::VisibilityState* visibility_state) {
              DCHECK(context->GetWindowOrWorkerGlobalScope()->IsWindow());
              DCHECK(context->GetWindowOrWorkerGlobalScope()
                         ->AsWindow()
                         ->document());
              *visibility_state = context->GetWindowOrWorkerGlobalScope()
                                      ->AsWindow()
                                      ->document()
                                      ->visibility_state();
            },
            context, &visibility_state));
  }

  return visibility_state;
}

bool WindowClient::focused() {
  DCHECK(event_target());
  DCHECK(event_target()->environment_settings());
  web::Context* context = event_target()->environment_settings()->context();
  DCHECK(context);
  base::MessageLoop* message_loop = context->message_loop();
  if (!message_loop) {
    return false;
  }

  bool focused = false;
  if (message_loop->task_runner()->BelongsToCurrentThread()) {
    DCHECK(context->GetWindowOrWorkerGlobalScope()->IsWindow());
    DCHECK(context->GetWindowOrWorkerGlobalScope()->AsWindow()->document());
    focused = context->GetWindowOrWorkerGlobalScope()
                  ->AsWindow()
                  ->document()
                  ->HasFocus();
  } else {
    message_loop->task_runner()->PostBlockingTask(
        FROM_HERE,
        base::Bind(
            [](web::Context* context, bool* focused) {
              DCHECK(context->GetWindowOrWorkerGlobalScope()->IsWindow());
              DCHECK(context->GetWindowOrWorkerGlobalScope()
                         ->AsWindow()
                         ->document());
              *focused = context->GetWindowOrWorkerGlobalScope()
                             ->AsWindow()
                             ->document()
                             ->HasFocus();
            },
            context, &focused));
  }

  return focused;
}

}  // namespace worker
}  // namespace cobalt
