// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/application.h"

namespace starboard {

// Forward declaration for the implementation class (PImpl idiom).
class LinkReceiverImpl;

class LinkReceiver {
 public:
  // Constructor that listens on an ephemeral port (port 0).
  // The actual port can be retrieved if needed, but is typically written to a
  // temporary file for discovery.
  explicit LinkReceiver(Application* application);

  // Constructor that listens on a specific port.
  LinkReceiver(Application* application, int port);

  // Destructor safely shuts down the receiver thread.
  ~LinkReceiver();

  // Disable copy and move operations.
  LinkReceiver(const LinkReceiver&) = delete;
  LinkReceiver& operator=(const LinkReceiver&) = delete;
  LinkReceiver(LinkReceiver&&) = delete;
  LinkReceiver& operator=(LinkReceiver&&) = delete;

 private:
  LinkReceiverImpl* impl_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_LINK_RECEIVER_H_
