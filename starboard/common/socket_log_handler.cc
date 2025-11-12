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

#include "starboard/common/socket_log_handler.h"

#include <cstdio>
#include <ctime>

#include "starboard/client_porting/eztime/eztime.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/string.h"
#include "starboard/socket.h"
#include "starboard/thread.h"
#include "starboard/time.h"

namespace starboard {

namespace {
const int kMaxQueueSize = 1000;  // Max number of queued log messages
const int kReconnectDelayMs = 5000;  // 5 seconds between reconnect attempts
}  // namespace

SocketLogHandler* SocketLogHandler::GetInstance() {
  static SocketLogHandler instance;
  return &instance;
}

SocketLogHandler::SocketLogHandler()
    : send_thread_(kSbThreadInvalid),
      log_available_(0),
      socket_(kSbSocketInvalid),
      port_(0),
      address_resolved_(false),
      is_running_(false) {
}

SocketLogHandler::~SocketLogHandler() {
  Stop();
}

void SocketLogHandler::Start(const std::string& hostname, int port) {
  if (is_running_) {
    SB_LOG(WARNING) << "SocketLogHandler already running";
    return;
  }

  hostname_ = hostname;
  port_ = port;
  is_running_ = true;

  SB_LOG(INFO) << "Starting SocketLogHandler for " << hostname_ << ":" << port_;

  // Start the background thread for sending logs
  send_thread_ = SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity,
                                 true, "SocketLogSender", &ThreadEntryPoint, this);
  if (!SbThreadIsValid(send_thread_)) {
    SB_LOG(ERROR) << "Failed to create socket log handler thread";
    is_running_ = false;
  }
}

void SocketLogHandler::Stop() {
  if (!is_running_) {
    return;
  }

  SB_LOG(INFO) << "Stopping SocketLogHandler";
  is_running_ = false;

  // Signal the semaphore to wake up the thread
  log_available_.Put();

  // Wait for thread to finish (it will process remaining queue)
  if (SbThreadIsValid(send_thread_)) {
    SbThreadJoin(send_thread_, nullptr);
    send_thread_ = kSbThreadInvalid;
  }

  // Try to flush any remaining logs in the queue
  size_t remaining_logs = 0;
  {
    starboard::ScopedLock lock(queue_lock_);
    remaining_logs = log_queue_.size();
  }

  if (remaining_logs > 0) {
    SB_LOG(INFO) << "Attempting to flush " << remaining_logs << " remaining logs";

    // Try to connect if not connected
    if (!SbSocketIsValid(socket_)) {
      ConnectToServer();
    }

    // If connected, try to send remaining logs
    if (SbSocketIsValid(socket_)) {
      while (remaining_logs > 0) {
        std::string log_message;
        {
          starboard::ScopedLock lock(queue_lock_);
          if (!log_queue_.empty()) {
            log_message = log_queue_.front();
            log_queue_.pop_front();
          } else {
            break;
          }
        }

        if (!log_message.empty()) {
          if (!SendLogToServer(log_message)) {
            // Connection error, stop trying
            SB_LOG(WARNING) << "Failed to send remaining logs, skipping "
                            << remaining_logs - 1 << " messages";
            break;
          }
        }
        remaining_logs--;
      }
    } else {
      SB_LOG(WARNING) << "Cannot connect to log server, skipping "
                      << remaining_logs << " remaining logs";
    }
  }

  DisconnectFromServer();

  // Clear any logs that couldn't be sent
  {
    starboard::ScopedLock lock(queue_lock_);
    if (!log_queue_.empty()) {
      SB_LOG(INFO) << "Discarding " << log_queue_.size() << " unsent logs";
      log_queue_.clear();
    }
  }
}

void* SocketLogHandler::ThreadEntryPoint(void* context) {
  SocketLogHandler* handler = static_cast<SocketLogHandler*>(context);
  handler->SendLogsThreadMain();
  return nullptr;
}

void SocketLogHandler::QueueRawLog(const char* message) {
  if (!is_running_) {
    return;
  }

  // Get current time and format as [HH:MM:SS.mmm]
  EzTimeValue time_value;
  EzTimeValueGetNow(&time_value, NULL);
  EzTimeT t = time_value.tv_sec;
  struct EzTimeExploded local_time = {0};
  EzTimeTExplodeLocal(&t, &local_time);

  int milliseconds = static_cast<int>(time_value.tv_usec / 1000);

  char timestamp[16];
  snprintf(timestamp, sizeof(timestamp), "[%02d:%02d:%02d.%03d] ",
           local_time.tm_hour, local_time.tm_min, local_time.tm_sec, milliseconds);

  // Prepend timestamp to message
  std::string timestamped_message = timestamp;
  timestamped_message += message;

  // Add timestamped message to queue
  {
    starboard::ScopedLock lock(queue_lock_);
    if (log_queue_.size() < kMaxQueueSize) {
      log_queue_.push_back(timestamped_message);
    } else {
      // Queue is full, drop the oldest message
      log_queue_.pop_front();
      log_queue_.push_back(timestamped_message);
    }
  }

  // Signal that a log is available
  log_available_.Put();
}

void SocketLogHandler::SendLogsThreadMain() {
  SB_LOG(INFO) << "SocketLogHandler thread started";

  while (is_running_) {
    // Try to connect if not connected
    if (!SbSocketIsValid(socket_)) {
      if (!ConnectToServer()) {
        SbThreadSleep(kReconnectDelayMs * kSbTimeMillisecond);
        continue;
      }
    }

    // Wait for a log to be available (blocks until signaled)
    log_available_.Take();

    // Check if we're still running after waking up
    if (!is_running_) {
      break;
    }

    // Process log queue
    std::string log_message;
    {
      starboard::ScopedLock lock(queue_lock_);
      if (!log_queue_.empty()) {
        log_message = log_queue_.front();
        log_queue_.pop_front();
      }
    }

    if (!log_message.empty()) {
      if (!SendLogToServer(log_message)) {
        DisconnectFromServer();
        // Always re-queue failed message at front (priority over new messages)
        // The queue-trimming logic in QueueLog handles overflow by dropping oldest
        {
          starboard::ScopedLock lock(queue_lock_);
          log_queue_.push_front(log_message);
        }
      }
    }
  }

  SB_LOG(INFO) << "SocketLogHandler thread stopped";
}

bool SocketLogHandler::ConnectToServer() {
  // Resolve hostname if not already done
  if (!address_resolved_) {
    SbSocketResolution* resolution =
        SbSocketResolve(hostname_.c_str(), kSbSocketResolveFilterIpv4);
    if (!resolution || resolution->address_count == 0) {
      SB_LOG(ERROR) << "Failed to resolve hostname: " << hostname_;
      if (resolution) {
        SbSocketFreeResolution(resolution);
      }
      return false;
    }

    server_address_ = resolution->addresses[0];
    server_address_.port = port_;
    SbSocketFreeResolution(resolution);
    address_resolved_ = true;
  }

  socket_ = SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolTcp);
  if (!SbSocketIsValid(socket_)) {
    SB_LOG(ERROR) << "Failed to create socket";
    return false;
  }

  SbSocketError connect_result = SbSocketConnect(socket_, &server_address_);
  if (connect_result != kSbSocketOk && connect_result != kSbSocketPending) {
    SB_LOG(ERROR) << "Failed to connect to " << hostname_ << ":" << port_
               << " - error: " << connect_result;
    SbSocketDestroy(socket_);
    socket_ = kSbSocketInvalid;
    return false;
  }

  // Wait for connection to complete (non-blocking socket)
  if (connect_result == kSbSocketPending) {
    SbThreadSleep(200 * kSbTimeMillisecond);  // Wait 200ms
  }

  SB_LOG(INFO) << "Connected to log server at " << hostname_ << ":" << port_;
  return true;
}

void SocketLogHandler::DisconnectFromServer() {
  if (SbSocketIsValid(socket_)) {
    SbSocketDestroy(socket_);
    socket_ = kSbSocketInvalid;
    SB_LOG(INFO) << "Disconnected from log server";
  }
}

bool SocketLogHandler::SendLogToServer(const std::string& log_message) {
  if (!SbSocketIsValid(socket_)) {
    return false;
  }

  // Loop until all data is sent (TCP stream may require multiple sends)
  int to_send = static_cast<int>(log_message.length());
  const char* buffer = log_message.c_str();
  int total_sent = 0;

  while (total_sent < to_send) {
    int sent = SbSocketSendTo(socket_, buffer + total_sent,
                              to_send - total_sent, NULL);
    if (sent <= 0) {
      // sent == 0: connection closed by peer
      // sent < 0: socket error
      SB_LOG(ERROR) << "Failed to send log to server (sent=" << sent << ")";
      return false;
    }
    total_sent += sent;
  }

  return true;
}

}  // namespace starboard
