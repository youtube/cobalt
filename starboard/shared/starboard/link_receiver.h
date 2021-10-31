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

#ifndef STARBOARD_SHARED_STARBOARD_LINK_RECEIVER_H_
#define STARBOARD_SHARED_STARBOARD_LINK_RECEIVER_H_

#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"

namespace starboard {
namespace shared {
namespace starboard {

// A loopback-only server that listens to receive null-terminated strings that
// will be dispatched to the Link() method on the passed in |application|. This
// will result in kSbEventTypeLink events beind dispatched on the main Starboard
// dispatch thread.
//
// This server Runs on its own thread, joining it on destruction. It must be
// destroyed before the associated Application is destroyed.
//
// When the server is started, it attempts to write a file to the temporary
// directory with the port that it is listening on. Other programs can then look
// for this file to find the port to connect to to send links.
//
// The script starboard/tools/send_link.py can dispatch links to this server, if
// running.
class LinkReceiver {
 public:
  explicit LinkReceiver(Application* application);
  LinkReceiver(Application* application, int port);
  ~LinkReceiver();

 private:
  class Impl;

  Impl* impl_;
};

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_LINK_RECEIVER_H_
