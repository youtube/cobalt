// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
#include "cobalt/media/base/media_log.h"
#include "nb/memory_scope.h"
#include "starboard/common/string.h"
#include "starboard/media.h"
#include "starboard/window.h"

namespace cobalt {
namespace media {

namespace {

class CanPlayTypeHandlerStarboard : public CanPlayTypeHandler {
 public:
  std::string CanPlayType(bool is_progressive, const std::string& mime_type,
                          const std::string& key_system) override {
    if (is_progressive) {
      // |mime_type| is something like:
      //   video/mp4
      //   video/webm
      //   video/mp4; codecs="avc1.4d401e"
      //   video/webm; codecs="vp9"
      // We do a rough pre-filter to ensure that only video/mp4 is supported as
      // progressive.
      if (SbStringFindString(mime_type.c_str(), "video/mp4") == 0 &&
          SbStringFindString(mime_type.c_str(), "application/x-mpegURL") == 0) {
        return "";
      }
    }
    SbMediaSupportType type =
        SbMediaCanPlayMimeAndKeySystem(mime_type.c_str(), key_system.c_str());
    switch (type) {
      case kSbMediaSupportTypeNotSupported:
        return "";
      case kSbMediaSupportTypeMaybe:
        return "maybe";
      case kSbMediaSupportTypeProbably:
        return "probably";
    }
    NOTREACHED();
    return "";
  }
};

void RunClosureAndSignal(const base::Closure& closure,
                         base::WaitableEvent* event) {
  closure.Run();
  event->Signal();
}

void RunClosureOnMessageLoopAndWait(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const base::Closure& closure) {
  base::WaitableEvent waitable_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner->PostTask(
      FROM_HERE, base::Bind(&RunClosureAndSignal, closure, &waitable_event));
  waitable_event.Wait();
}

}  // namespace

std::unique_ptr<WebMediaPlayer> MediaModule::CreateWebMediaPlayer(
    WebMediaPlayerClient* client) {
  TRACK_MEMORY_SCOPE("Media");
  SbWindow window = kSbWindowInvalid;
  if (system_window_) {
    window = system_window_->GetSbWindow();
  }
  return std::unique_ptr<WebMediaPlayer>(new media::WebMediaPlayerImpl(
      window,
      base::Bind(&MediaModule::GetSbDecodeTargetGraphicsContextProvider,
                 base::Unretained(this)),
      client, this, &decoder_buffer_allocator_,
      options_.allow_resume_after_suspend, new media::MediaLog));
}

void MediaModule::Suspend() {
  RunClosureOnMessageLoopAndWait(
      task_runner_,
      base::Bind(&MediaModule::SuspendTask, base::Unretained(this)));
  resource_provider_ = NULL;
}

void MediaModule::Resume(render_tree::ResourceProvider* resource_provider) {
  resource_provider_ = resource_provider;
  RunClosureOnMessageLoopAndWait(
      task_runner_,
      base::Bind(&MediaModule::ResumeTask, base::Unretained(this)));
}

void MediaModule::RegisterPlayer(WebMediaPlayer* player) {
  RunClosureOnMessageLoopAndWait(task_runner_,
                                 base::Bind(&MediaModule::RegisterPlayerTask,
                                            base::Unretained(this), player));
}

void MediaModule::UnregisterPlayer(WebMediaPlayer* player) {
  RunClosureOnMessageLoopAndWait(task_runner_,
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
  DCHECK(task_runner_->BelongsToCurrentThread());

  suspended_ = true;

  for (Players::iterator iter = players_.begin(); iter != players_.end();
       ++iter) {
    DCHECK(!iter->second);
    if (!iter->second) {
      iter->first->Suspend();
    }
  }
}

void MediaModule::ResumeTask() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  for (Players::iterator iter = players_.begin(); iter != players_.end();
       ++iter) {
    DCHECK(!iter->second);
    if (!iter->second) {
      iter->first->Resume();
    }
  }

  suspended_ = false;
}

void MediaModule::RegisterPlayerTask(WebMediaPlayer* player) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  DCHECK(players_.find(player) == players_.end());
  players_.insert(std::make_pair(player, false));

  // Track debug state for the most recently added WebMediaPlayer instance.
  RegisterDebugState(player);

  if (suspended_) {
    player->Suspend();
  }
}

void MediaModule::UnregisterPlayerTask(WebMediaPlayer* player) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  DCHECK(players_.find(player) != players_.end());
  players_.erase(players_.find(player));

  if (players_.empty()) {
    DeregisterDebugState();
  } else {
    RegisterDebugState(players_.begin()->first);
  }
}

std::unique_ptr<CanPlayTypeHandler> MediaModule::CreateCanPlayTypeHandler() {
  return std::unique_ptr<CanPlayTypeHandler>(new CanPlayTypeHandlerStarboard);
}

}  // namespace media
}  // namespace cobalt
