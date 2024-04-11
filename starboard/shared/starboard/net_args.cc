// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/net_args.h"

#include <memory>
#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/socket.h"
#include "starboard/common/time.h"
#include "starboard/socket_waiter.h"
#include "starboard/thread.h"

#ifndef NET_ARGS_PORT
#define NET_ARGS_PORT 49355
#endif

// Controls whether using IPv4 or IPv6.
#ifndef NET_ARGS_IP_VERSION
#define NET_ARGS_IP_VERSION kSbSocketAddressTypeIpv4
#endif

namespace starboard {
namespace shared {
namespace starboard {
namespace {

std::unique_ptr<Socket> CreateListenSocket() {
  std::unique_ptr<Socket> socket(
      new Socket(NET_ARGS_IP_VERSION, kSbSocketProtocolTcp));
  socket->SetReuseAddress(true);
  SbSocketAddress sock_addr;
  // Ip address will be set to 0.0.0.0 so that it will bind to all sockets.
  memset(&sock_addr, 0, sizeof(SbSocketAddress));
  sock_addr.type = NET_ARGS_IP_VERSION;
  sock_addr.port = NET_ARGS_PORT;
  SbSocketError sock_err = socket->Bind(&sock_addr);

  const char kErrFmt[] = "Socket error while attempting to bind, error = %d\n";
  if (sock_err != kSbSocketOk) {
    SbLogRawFormatF(kErrFmt, sock_err);
  }
  sock_err = socket->Listen();
  if (sock_err != kSbSocketOk) {
    SbLogRawFormatF(kErrFmt, sock_err);
  }
  return socket;
}

void WaitUntilReadableOrConnectionReset(SbSocket sock) {
  SbSocketWaiter waiter = SbSocketWaiterCreate();

  struct F {
    static void WakeUp(SbSocketWaiter waiter, SbSocket, void*, int) {
      SbSocketWaiterWakeUp(waiter);
    }
  };

  SbSocketWaiterAdd(waiter, sock, NULL, &F::WakeUp, kSbSocketWaiterInterestRead,
                    false);  // false means one shot.

  SbSocketWaiterWait(waiter);
  SbSocketWaiterRemove(waiter, sock);
  SbSocketWaiterDestroy(waiter);
}

std::unique_ptr<Socket> WaitForClientConnection(Socket* listen_sock,
                                                int64_t timeout) {
  int64_t expire_time = (timeout >= 0) && (timeout < kSbInt64Max)
                            ? CurrentMonotonicTime() + timeout
                            : kSbInt64Max;
  while (true) {
    std::unique_ptr<Socket> client_connection(listen_sock->Accept());
    if (client_connection) {
      return client_connection;
    }
    if (CurrentMonotonicTime() > expire_time) {
      return std::unique_ptr<Socket>();
    }
    SbThreadSleep(1000);
  }
}

std::vector<std::string> SplitStringByLines(const std::string& string_buff) {
  std::vector<std::string> lines;
  std::stringstream ss;
  ss << string_buff;
  for (std::string line; std::getline(ss, line);) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return lines;
}

}  // namespace.

// Command line switch useful for determining if NetArgsWaitForConnection()
// should be called.
const char kNetArgsCommandSwitchWait[] = "net_args_wait_for_connection";

std::vector<std::string> NetArgsWaitForPayload(int64_t timeout) {
  std::unique_ptr<Socket> listen = CreateListenSocket();
  std::unique_ptr<Socket> client_connection =
      WaitForClientConnection(listen.get(), timeout);
  if (!client_connection) {
    SB_LOG(ERROR) << "Timed out waiting for net args.";
    return std::vector<std::string>();
  }

  std::string str_buff;

  while (true) {
    char buff[128];
    int result = client_connection->ReceiveFrom(buff, sizeof(buff), NULL);
    if (result > 0) {
      str_buff.append(buff, static_cast<size_t>(result));
      continue;
    } else if (result == 0) {
      // Socket has closed.
      break;
    } else if (result < 0) {  // Handle error condition.
      SbSocketError err = client_connection->GetLastError();
      client_connection->ClearLastError();

      switch (err) {
        case kSbSocketOk: {
          SB_NOTREACHED() << "Expected error condition when return val "
                          << "is < 0.";
          continue;
        }
        case kSbSocketPending: {
          WaitUntilReadableOrConnectionReset(client_connection->socket());
          continue;
        }
        default: {
          break;
        }
      }
    }
  }
  return SplitStringByLines(str_buff);
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard
