// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/media/media_module.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"

namespace cobalt {
namespace media {

namespace {

void RunClosureAndSignal(const base::Closure& closure,
                         base::WaitableEvent* event) {
  closure.Run();
  event->Signal();
}

void RunClosureOnMessageLoopAndWait(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const base::Closure& closure) {
  base::WaitableEvent waitable_event(true, /* manual_reset */
                                     false /* initially_signaled */);
  message_loop->PostTask(
      FROM_HERE, base::Bind(&RunClosureAndSignal, closure, &waitable_event));
  waitable_event.Wait();
}

}  // namespace

void MediaModule::Suspend() {
  RunClosureOnMessageLoopAndWait(
      message_loop_,
      base::Bind(&MediaModule::SuspendTask, base::Unretained(this)));
  OnSuspend();
}

void MediaModule::Resume(render_tree::ResourceProvider* resource_provider) {
  OnResume(resource_provider);
  RunClosureOnMessageLoopAndWait(
      message_loop_,
      base::Bind(&MediaModule::ResumeTask, base::Unretained(this)));
}

void MediaModule::RegisterPlayer(WebMediaPlayer* player) {
  RunClosureOnMessageLoopAndWait(message_loop_,
                                 base::Bind(&MediaModule::RegisterPlayerTask,
                                            base::Unretained(this), player));
}

void MediaModule::UnregisterPlayer(WebMediaPlayer* player) {
  RunClosureOnMessageLoopAndWait(message_loop_,
                                 base::Bind(&MediaModule::UnregisterPlayerTask,
                                            base::Unretained(this), player));
}

void MediaModule::RegisterDebugState(WebMediaPlayer* player) {
  void* debug_state_address = NULL;
  size_t debug_state_size = 0;
  if (player->GetDebugReportDataAddress(&debug_state_address,
                                        &debug_state_size)) {
    base::UserLog::Register(base::UserLog::kWebMediaPlayerState,
                            "MediaPlyrState", debug_state_address,
                            debug_state_size);
  }
}

void MediaModule::DeregisterDebugState() {
  base::UserLog::Deregister(base::UserLog::kWebMediaPlayerState);
}

void MediaModule::SuspendTask() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  for (Players::iterator iter = players_.begin(); iter != players_.end();
       ++iter) {
    DCHECK(!iter->second);
    if (!iter->second) {
      iter->first->Suspend();
    }
  }
}

void MediaModule::ResumeTask() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  for (Players::iterator iter = players_.begin(); iter != players_.end();
       ++iter) {
    DCHECK(!iter->second);
    if (!iter->second) {
      iter->first->Resume();
    }
  }
}

void MediaModule::RegisterPlayerTask(WebMediaPlayer* player) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  DCHECK(players_.find(player) == players_.end());
  players_.insert(std::make_pair(player, false));

  // Track debug state for the most recently added WebMediaPlayer instance.
  RegisterDebugState(player);
}

void MediaModule::UnregisterPlayerTask(WebMediaPlayer* player) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  DCHECK(players_.find(player) != players_.end());
  players_.erase(players_.find(player));

  if (players_.empty()) {
    DeregisterDebugState();
  } else {
    RegisterDebugState(players_.begin()->first);
  }
}

}  // namespace media
}  // namespace cobalt
