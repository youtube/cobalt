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

#include "starboard/shared/posix/socket_receive_multi_msg_internal.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>

#include "starboard/common/log.h"
#include "starboard/common/socket.h"
#include "starboard/extension/socket_receive_multi_msg.h"
#include "starboard/shared/posix/socket_internal.h"

namespace sbposix = starboard::shared::posix;

namespace {

/*
  To abstract the implementation details, all data structures are allocated
  in a single large buffer. That also helps improve memory access performance
  and reduce heap fragmentation.

  There is a method to query the size needed for the buffer, and another to
  initialize a buffer. For the read function, only the socket and initialized
  buffer are needed.

  Layout of internal data structure to be allocated in the buffer:
  struct SbSocketReceiveMultiMsgResult result;
  unsigned int num_messages;
  unsigned int message_size;
  unsigned int control_size;
  unsigned int size;
  struct iovec iovecs[num_messages];
  struct mmsghdr msgs[num_messages];
  struct SbSocketReceiveMultiMsgPacket packets[num_messages]
  char message_data[num_messages * messages_size];
  char message_control[num_messages * control_size]
*/

struct SbSocketReceiveMultiMsgDataHeader {
  struct SbSocketReceiveMultiMsgResult result;
  unsigned int num_messages;
  unsigned int message_size;
  unsigned int control_size;
  unsigned int size;
};

struct SbSocketReceiveMultiMsgDataReference {
  SbSocketReceiveMultiMsgDataReference(char* data);
  SbSocketReceiveMultiMsgDataReference(unsigned int num_messages,
                                       unsigned int message_size,
                                       unsigned int control_size,
                                       char* data);

  void InitializeReferences(char* data);

  char* data(int message) const {
    return message_data + message * header->message_size;
  }
  char* control(int message) const {
    return message_control + message * header->control_size;
  }

  SbSocketReceiveMultiMsgDataHeader* header;
  struct iovec* iovecs;
  struct mmsghdr* msgs;
  struct SbSocketReceiveMultiMsgPacket* packets;
  char* message_data;
  char* message_control;
};

SbSocketReceiveMultiMsgResult* GetSbSocketReceiveMultiMsgResult(char* data) {
  return reinterpret_cast<SbSocketReceiveMultiMsgResult*>(data);
}

void SbSocketReceiveMultiMsgDataReference::InitializeReferences(char* data) {
  int next_offset = 0;
  header =
      reinterpret_cast<SbSocketReceiveMultiMsgDataHeader*>(data + next_offset);
  next_offset += sizeof(SbSocketReceiveMultiMsgDataHeader);

  iovecs = reinterpret_cast<struct iovec*>(data + next_offset);
  next_offset += header->num_messages * sizeof(struct iovec);

  msgs = reinterpret_cast<struct mmsghdr*>(data + next_offset);
  next_offset += header->num_messages * sizeof(struct mmsghdr);

  packets = reinterpret_cast<struct SbSocketReceiveMultiMsgPacket*>(
      data + next_offset);
  next_offset +=
      header->num_messages * sizeof(struct SbSocketReceiveMultiMsgPacket);

  message_data = data + next_offset;
  next_offset += header->num_messages * header->message_size;

  message_control = data + next_offset;
  next_offset += header->num_messages * header->control_size;

  header->size = next_offset;

  header->result.packets = packets;
}

SbSocketReceiveMultiMsgDataReference::SbSocketReceiveMultiMsgDataReference(
    char* data) {
  InitializeReferences(data);
}

SbSocketReceiveMultiMsgDataReference::SbSocketReceiveMultiMsgDataReference(
    unsigned int num_messages,
    unsigned int message_size,
    unsigned int control_size,
    char* data) {
  header = reinterpret_cast<SbSocketReceiveMultiMsgDataHeader*>(data);
  header->num_messages = num_messages;
  header->message_size = message_size;
  header->control_size = control_size;
  InitializeReferences(data);
}

}  //  namespace

unsigned int SbSocketReceiveMultiMsgBufferSize(unsigned int num_messages,
                                               unsigned int message_size,
                                               unsigned int control_size) {
  SbSocketReceiveMultiMsgDataHeader temp_header;
  SbSocketReceiveMultiMsgDataReference data_reference(
      num_messages, message_size, control_size,
      reinterpret_cast<char*>(&temp_header));
  return data_reference.header->size;
}

void SbSocketReceiveMultiMsgBufferInitialize(unsigned int num_messages,
                                             unsigned int message_size,
                                             unsigned int control_size,
                                             char* data) {
  SbSocketReceiveMultiMsgDataReference data_reference(
      num_messages, message_size, control_size, data);

  memset(data_reference.msgs, 0, num_messages * sizeof(struct mmsghdr));
  memset(data_reference.iovecs, 0, num_messages * sizeof(struct iovec));
  for (size_t packet = 0; packet < data_reference.header->num_messages;
       packet++) {
    data_reference.packets[packet].buffer = data_reference.data(packet);
    data_reference.packets[packet].result = 0;
    struct iovec* iov = &data_reference.iovecs[packet];
    struct msghdr* hdr = &data_reference.msgs[packet].msg_hdr;
    iov->iov_base = data_reference.data(packet);
    iov->iov_len = data_reference.header->message_size;
    hdr->msg_iov = iov;
    hdr->msg_iovlen = 1;
    hdr->msg_control = data_reference.control(packet);
    hdr->msg_controllen = data_reference.header->control_size;
  }
}

SbSocketReceiveMultiMsgResult* SbSocketReceiveMultiMsg(SbSocket socket,
                                                       char* buffer) {
  SbSocketReceiveMultiMsgDataReference data_reference(buffer);

  if (socket->protocol != kSbSocketProtocolUdp) {
    errno = EPROTONOSUPPORT;
    socket->error = kSbSocketErrorFailed;
    data_reference.header->result.result = -1;
    return GetSbSocketReceiveMultiMsgResult(buffer);
  }

  int rv = recvmmsg(socket->socket_fd, data_reference.msgs,
                    data_reference.header->num_messages, MSG_DONTWAIT, nullptr);
  if (rv > 0) {
    socket->error = kSbSocketOk;
    data_reference.header->result.result = rv;

    for (int i = 0; i < rv; ++i) {
      auto& packet = data_reference.header->result.packets[i];
      struct mmsghdr* mmsg = &data_reference.msgs[i];
      struct msghdr* msg = &mmsg->msg_hdr;
      packet.result = 0;
      if (mmsg->msg_len == 0) {
        continue;
      }
      if (msg->msg_flags & MSG_TRUNC) {
        packet.result = -1;
        continue;
      }
      packet.result = mmsg->msg_len;
    }

    return GetSbSocketReceiveMultiMsgResult(buffer);
  }

  if (errno != EAGAIN && errno != EWOULDBLOCK &&
      socket->error != sbposix::TranslateSocketErrno(errno)) {
    SB_LOG(ERROR) << "recvmmsg failed, errno = " << errno;
  }
  socket->error = sbposix::TranslateSocketErrno(errno);
  data_reference.header->result.result = -1;
  return GetSbSocketReceiveMultiMsgResult(buffer);
}
