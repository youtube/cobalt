// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SHELL_BROWSER_COBALT_SIMPLE_BEGIN_FRAME_OBSERVER_H_
#define COBALT_SHELL_BROWSER_COBALT_SIMPLE_BEGIN_FRAME_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/compositor/host_begin_frame_observer.h"

namespace ui {
class Compositor;
}  // namespace ui

namespace content {

class WebContents;

class SimpleBeginFrameObserver
    : public ui::HostBeginFrameObserver::SimpleBeginFrameObserver,
      public content::WebContentsUserData<SimpleBeginFrameObserver> {
 public:
  using OnBeginFrameCallback = base::RepeatingCallback<void(double)>;

  ~SimpleBeginFrameObserver() override;

  SimpleBeginFrameObserver(const SimpleBeginFrameObserver&) = delete;
  SimpleBeginFrameObserver& operator=(const SimpleBeginFrameObserver&) = delete;

  void StartObservingBeginFrame(OnBeginFrameCallback callback);
  void StopObservingBeginFrames();

 private:
  friend class content::WebContentsUserData<SimpleBeginFrameObserver>;

  explicit SimpleBeginFrameObserver(content::WebContents* web_contents,
                                    ui::Compositor* compositor);

  // ui::HostBeginFrameObserver::SimpleBeginFrameObserver:
  void OnBeginFrame(
      base::TimeTicks frame_begin_time,
      base::TimeDelta frame_interval,
      std::optional<base::TimeTicks> first_coalesced_frame_begin_time) override;
  void OnBeginFrameSourceShuttingDown() override;

  raw_ptr<ui::Compositor> observed_compositor_ = nullptr;
  OnBeginFrameCallback callback_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_COBALT_SIMPLE_BEGIN_FRAME_OBSERVER_H_
