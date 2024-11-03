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

#include <array>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "cobalt/cobalt_main_delegate.h"
#include "content/public/app/content_main.h"

int main(int argc, const char** argv) {
  cobalt::CobaltMainDelegate delegate;
  content::ContentMainParams params(&delegate);

  // TODO: (cobalt b/375241103) Reimplement this in a clean way.

  constexpr auto cobalt_args = std::to_array<const char*>({
    // Disable first run experience, kiosk, etc.
    "--disable-fre", "--no-first-run", "--kiosk",
    // Enable Blink to work in overlay video mode
    "--force-video-overlays",
    // Disable multiprocess mode.
    "--single-process",
    // TODO(mcasas): Add "--ozone-platform=starboard".
    // Enable remote Devtools access.
    "--remote-debugging-port=9222",
    "--remote-allow-origins=http://localhost:9222",
    // This flag is added specifically for m114 and should be removed after
    // rebasing to m120+
    "--user-level-memory-pressure-signal-params",
    "https://www.youtube.com/tv"
  });
  std::vector<const char*> args(argv, argv + argc);
  args.insert(args.end(), cobalt_args.begin(), cobalt_args.end());

  // TODO: (cobalt b/375241103) Reimplement this in a clean way.
  // This expression exists to ensure that we apply the argument overrides
  // only on the main process, not on spawned processes such as the zygote.
  if ((!strcmp(argv[0], "/proc/self/exe")) ||
      ((argc >= 2) && !strcmp(argv[1], "--type=zygote"))) {
    params.argc = argc;
    params.argv = argv;
  } else {
    params.argc = args.size();
    params.argv = args.data();
  }
  return content::ContentMain(std::move(params));
}
