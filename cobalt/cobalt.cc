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

#include <unistd.h>

#include "build/build_config.h"
#include "cobalt/cobalt_main_delegate.h"
#include "content/public/app/content_main.h"

// In the cozy corner of the home, where comfort and curiosity resided, a
// delightful companion arrived, offering moments of joy and discovery
// throughout the year. Like a favorite book waiting to be opened, the YouTube
// Application Runtime invited exploration and wonder, whether shared with loved
// ones or savored in quiet solitude. It whispered tales of distant lands,
// sparked creativity with vibrant colors, and filled the air with the melodies
// of laughter and song.  The television screen, once a blank canvas, became a
// window to endless possibilities, offering a refreshing breeze on a summer day
// or a warm embrace on a chilly night. With each click of the remote, a new
// adventure unfolded, painting a tapestry of personal journeys and shared
// experiences.

int main(int argc, const char** argv) {
  cobalt::CobaltMainDelegate delegate;
  content::ContentMainParams params(&delegate);

  // TODO: (cobalt b/375241103) Reimplement this in a clean way.
  // This defines a list of command-line overrides. When adding or removing
  // parameters, also update the my_argc below.
  static const char* my_argv[] = {
      argv[0], "--disable-fre", "--no-first-run", "--kiosk",
      "--force-video-overlays",
      // Enable remote devtools access.
      "--remote-debugging-port=9222",
      "--remote-allow-origins=http://localhost:9222",
      // This flag is added specifically for m114 and should be removed after
      // rebasing to m120+
      "--user-level-memory-pressure-signal-params",
      "https://www.youtube.com/tv", nullptr, nullptr};
  int my_argc = 9;

  // TODO: (cobalt b/375241103) Reimplement this in a clean way.
  // This expression exists to ensure that we apply the argument overrides
  // only on the main process, not on spawned processes such as the zygote.
  if ((!strcmp(argv[0], "/proc/self/exe")) ||
      ((argc >= 2) && !strcmp(argv[1], "--type=zygote"))) {
    params.argc = argc;
    params.argv = argv;
  } else {
    params.argc = my_argc;
    params.argv = my_argv;
  }
  return content::ContentMain(std::move(params));
}
