// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/image/animated_image_tracker.h"

#include <algorithm>

#include "cobalt/base/polymorphic_downcast.h"

namespace cobalt {
namespace loader {
namespace image {

AnimatedImageTracker::AnimatedImageTracker(
    base::ThreadPriority animated_image_decode_thread_priority)
    : animated_image_decode_thread_("AnimatedImage") {
  base::Thread::Options options(MessageLoop::TYPE_DEFAULT,
                                0 /* default stack size */,
                                animated_image_decode_thread_priority);
  animated_image_decode_thread_.StartWithOptions(options);
}

AnimatedImageTracker::~AnimatedImageTracker() {
  animated_image_decode_thread_.Stop();
}

void AnimatedImageTracker::IncreaseURLCount(const GURL& url) {
  current_url_counts_[url]++;
}

void AnimatedImageTracker::DecreaseURLCount(const GURL& url) {
  DCHECK_GT(current_url_counts_[url], 0);
  current_url_counts_[url]--;
}

void AnimatedImageTracker::RecordImage(
    const GURL& url, loader::image::AnimatedImage* animated_image) {
  DCHECK(animated_image);
  image_map_[url] = animated_image;
}

void AnimatedImageTracker::ProcessRecordedImages() {
  for (URLToIntMap::iterator it = current_url_counts_.begin();
       it != current_url_counts_.end();) {
    const GURL& url = it->first;
    URLSet::iterator playing_url = playing_urls_.find(url);
    if (it->second == 0) {
      // Remove the images that are no longer displayed, and stop the
      // animations.
      if (playing_url != playing_urls_.end()) {
        playing_urls_.erase(playing_url);
        OnDisplayEnd(image_map_[url]);
      }
      image_map_.erase(url);
      previous_url_counts_.erase(url);
      current_url_counts_.erase(it++);
    } else {
      // Record the images that start to be displayed, and start the
      // animations.
      if (playing_url == playing_urls_.end()) {
        URLToImageMap::iterator playing_image = image_map_.find(url);
        if (playing_image != image_map_.end()) {
          playing_urls_.insert(std::make_pair(url, base::Unused()));
          OnDisplayStart(playing_image->second);
        }
      }
      previous_url_counts_[url] = it->second;
      ++it;
    }
  }
}

void AnimatedImageTracker::OnDisplayStart(
    loader::image::AnimatedImage* animated_image) {
  DCHECK(animated_image);
  animated_image->Play(animated_image_decode_thread_.message_loop_proxy());
}

void AnimatedImageTracker::OnDisplayEnd(
    loader::image::AnimatedImage* animated_image) {
  DCHECK(animated_image);
  animated_image->Stop();
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
