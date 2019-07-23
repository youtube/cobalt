// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LOADER_IMAGE_ANIMATED_IMAGE_TRACKER_H_
#define COBALT_LOADER_IMAGE_ANIMATED_IMAGE_TRACKER_H_

#include <map>
#include <string>

#include "base/containers/small_map.h"
#include "base/threading/thread.h"
#include "cobalt/base/unused.h"
#include "cobalt/loader/image/image.h"
#include "url/gurl.h"

namespace cobalt {
namespace loader {
namespace image {

// This class maintains the list of all images that are being displayed
// at the current time. Whenever an animated image enters / exits the list, the
// playing status is updated hence decoding is turned on / off for it.
class AnimatedImageTracker {
 public:
  explicit AnimatedImageTracker(
      base::ThreadPriority animated_image_decode_thread_priority);
  ~AnimatedImageTracker();

  // Updates the count of an image URL that is being displayed.
  void IncreaseURLCount(const GURL& url);
  void DecreaseURLCount(const GURL& url);

  // Record an actual animated image that is linked to its URL.
  void RecordImage(const GURL& url,
                   loader::image::AnimatedImage* animated_image);

  // This indicates the update calls for the current frame have finished. All
  // changes during two ProcessRecordedImages calls will be processed. It should
  // be called at the end of a layout.
  void ProcessRecordedImages();

  // Clears the AnimatedImageTracker, releasing any tracked in-progress
  // animations.
  void Reset();

 private:
  void OnDisplayStart(loader::image::AnimatedImage* image);
  void OnDisplayEnd(loader::image::AnimatedImage* image);

  typedef std::map<GURL, scoped_refptr<loader::image::AnimatedImage> >
      URLToImageMap;
  typedef std::map<GURL, int> URLToIntMap;
  typedef base::small_map<std::map<GURL, base::Unused>, 1> URLSet;

  // The image decode thread is a thread created and owned by the
  // AnimatedImageTracker, but used by the AnimatedImage decoders.
  base::Thread animated_image_decode_thread_;

  URLToImageMap image_map_;
  URLToIntMap previous_url_counts_;
  URLToIntMap current_url_counts_;
  URLSet playing_urls_;

  // Used to ensure that all AnimatedImageTracker methods are called on the
  // same thread (*not* |animated_image_decode_thread_|), the thread that we
  // were constructed on.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_ANIMATED_IMAGE_TRACKER_H_
