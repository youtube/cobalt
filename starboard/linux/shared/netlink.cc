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

#include "starboard/linux/shared/netlink.h"

#include <linux/netlink.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <vector>

#include "starboard/common/log.h"
#include "starboard/types.h"

// Omit namespace linux due to symbol name conflict.
namespace starboard {
namespace shared {

namespace {
void LogLastError(const char* msg) {
  const int kErrorMessageBufferSize = 256;
  char msgbuf[kErrorMessageBufferSize];
  SbSystemError error_code = SbSystemGetLastError();
  if (SbSystemGetErrorString(error_code, msgbuf, kErrorMessageBufferSize) > 0) {
    SB_LOG(ERROR) << msg << ": " << msgbuf;
  }
}

// Buffer size for receiving netlink messages. Should be large enough to hold
// the full routing table dump.
const size_t kNetlinkMessageBufferSize = 32768;
}  // namespace

// Open the netlink socket.
bool NetLink::Open(int type, int protocol) {
  //  https://man7.org/linux/man-pages/man7/netlink.7.html
  //  https://datatracker.ietf.org/doc/html/rfc3549#section-2
  socket_fd_ = socket(AF_NETLINK, type, protocol);
  if (socket_fd_ == -1) {
    LogLastError("Netlink Request socket could not be opened");
    return false;
  }

#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
  {
    // Use SO_NOSIGPIPE to mute SIGPIPE on darwin systems.
    int optval_set = 1;
    setsockopt(socket_fd_, SOL_SOCKET, SO_NOSIGPIPE,
               reinterpret_cast<void*>(&optval_set), sizeof(int));
  }
#endif

#if defined(NETLINK_DUMP_STRICT_CHK)
  {
    int optval_set = 1;
    setsockopt(socket_fd_, SOL_NETLINK, NETLINK_DUMP_STRICT_CHK,
               reinterpret_cast<void*>(&optval_set), sizeof(int));
  }
#endif
  return true;
}

bool NetLink::IsOpened() {
  return socket_fd_ != -1;
}

void NetLink::Close() {
  if (socket_fd_ != -1) {
    shutdown(socket_fd_, SHUT_RDWR);
    close(socket_fd_);
    socket_fd_ = -1;
  }
  request_sent_ = false;
}

// Send a netlink request.
bool NetLink::Request(uint16_t type,
                      uint16_t message_flags,
                      void* payload,
                      int payload_length) {
  if (!IsOpened())
    return false;
  std::vector<char> netlink_buffer(NLMSG_LENGTH(payload_length));
  auto header = reinterpret_cast<struct nlmsghdr*>(netlink_buffer.data());
  memcpy(NLMSG_DATA(header), payload, payload_length);

  header->nlmsg_len = netlink_buffer.size();
  header->nlmsg_type = type;
  header->nlmsg_flags = NLM_F_REQUEST | message_flags;
  header->nlmsg_seq = ++request_sequence_;
  request_sent_ =
      send(socket_fd_, netlink_buffer.data(), netlink_buffer.size(), 0) != -1;
  return request_sent_;
}

// Return the next netlink message. Returns nullptr if there are no more
// messages.
struct nlmsghdr* NetLink::GetNextMessage() {
  // If we already have a message header, return the next header in the
  // message if there is one.
  if (header_) {
    if ((header_->nlmsg_type == NLMSG_DONE) ||
        ((header_->nlmsg_flags & NLM_F_MULTI) == 0)) {
      // This is not a multipart response, or the last header.
      message_length_ = 0;
      header_ = nullptr;
    } else {
      // Get the next header.
      header_ = NLMSG_NEXT(header_, message_length_);
      if ((NLMSG_OK(header_, message_length_) == 0) ||
          (header_->nlmsg_type == NLMSG_ERROR)) {
        header_ = nullptr;
      }
    }
  }

  if (header_)
    return header_;

  // Receive the next message with netlink headers.
  if (!request_sent_)
    return nullptr;
  if (message_buffer_.size() == 0) {
    message_buffer_.resize(kNetlinkMessageBufferSize);
  }
  message_length_ =
      recv(socket_fd_, message_buffer_.data(), message_buffer_.size(), 0);
  if (message_length_ == -1) {
    Close();
    return nullptr;
  }

  header_ = reinterpret_cast<struct nlmsghdr*>(message_buffer_.data());
  if ((NLMSG_OK(header_, message_length_) == 0) ||
      (header_->nlmsg_type == NLMSG_ERROR)) {
    Close();
    return nullptr;
  }

  if ((header_->nlmsg_type == NLMSG_DONE) ||
      ((header_->nlmsg_flags & NLM_F_MULTI) == 0) ||
      (header_->nlmsg_pid == 0)) {
    request_sent_ = false;
  }

  if ((header_->nlmsg_seq != request_sequence_)) {
    SB_LOG(WARNING) << "Unexpected different sequence " << header_->nlmsg_seq
                    << "!= " << request_sequence_ << ".";
    Close();
    return nullptr;
  }

  return header_;
}

}  // namespace shared
}  // namespace starboard
