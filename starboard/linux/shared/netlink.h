// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_LINUX_SHARED_NETLINK_H_
#define STARBOARD_LINUX_SHARED_NETLINK_H_

#include <linux/netlink.h>

#include <vector>

#include "starboard/types.h"

// Omit namespace linux due to symbol name conflict.
namespace starboard {
namespace shared {

// Support class for netlink sockets.
//  https://man7.org/linux/man-pages/man7/netlink.7.html

// Helper class for the netlink socket API.
class NetLink {
 public:
  NetLink() {}
  virtual ~NetLink() { Close(); }

  // Open the netlink socket.
  bool Open(int type, int protocol);

  bool IsOpened();

  void Close();

  // Send a netlink request.
  bool Request(uint16_t type,
               uint16_t message_flags,
               void* payload = nullptr,
               int payload_length = 0);

  // Return the next netlink message. Returns nullptr if there are no more
  // messages.
  struct nlmsghdr* GetNextMessage();

 private:
  NetLink(const NetLink&) = delete;
  NetLink& operator=(NetLink&&) = delete;
  void operator=(const NetLink&) = delete;

  int socket_fd_ = -1;
  int request_sequence_ = 0;
  bool request_sent_ = false;
  std::vector<char> message_buffer_;
  int message_length_ = 0;
  struct nlmsghdr* header_ = nullptr;
};

}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_LINUX_SHARED_NETLINK_H_
