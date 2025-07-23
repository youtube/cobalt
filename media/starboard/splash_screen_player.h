// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_STARBOARD_SPLASH_SCREEN_PLAYER_H_
#define MEDIA_STARBOARD_SPLASH_SCREEN_PLAYER_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "media/starboard/sbplayer_bridge.h"
#include "media/starboard/sbplayer_interface.h"
#include "media/starboard/sbplayer_set_bounds_helper.h"
#include "mkvparser/mkvparser.h"
#include "mkvparser/mkvreader.h"
#include "starboard/window.h"

namespace media {

class SplashScreenPlayer {
 public:
  SplashScreenPlayer(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      SbWindow window);
  ~SplashScreenPlayer();

  void Start();
  void Stop();

 private:
  void Initialize();
  void Teardown();

  void FindVideoTrack();
  void CreatePlayer();
  void MainLoop();
  void WriteFrame();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SbWindow window_;

  std::unique_ptr<mkvparser::MkvReader> reader_;
  std::unique_ptr<mkvparser::Segment> segment_;
  const mkvparser::VideoTrack* video_track_ = nullptr;
  const mkvparser::Cluster* current_cluster_ = nullptr;
  const mkvparser::BlockEntry* current_block_entry_ = nullptr;

  DefaultSbPlayerInterface sbplayer_interface_;
  scoped_refptr<SbPlayerSetBoundsHelper> set_bounds_helper_;
  std::unique_ptr<SbPlayerBridge> player_bridge_;
  bool player_created_ = false;
  bool main_loop_running_ = false;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_SPLASH_SCREEN_PLAYER_H_
