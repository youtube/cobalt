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

#ifndef STARBOARD_COMMON_SOCKET_LOG_HANDLER_H_
#define STARBOARD_COMMON_SOCKET_LOG_HANDLER_H_

#include <atomic>
#include <deque>
#include <string>

#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/semaphore.h"
#include "starboard/socket.h"
#include "starboard/thread.h"

namespace starboard {

// SocketLogHandler streams all application logs to a remote server via TCP.
// This class runs a background thread that handles socket I/O separately
// from the main application, so logging never blocks the application.
//
// Usage:
//   SocketLogHandler::GetInstance()->Start("192.168.1.100", 8080);
//
// The handler will automatically:
// - Register with LogMessageHandler to receive all logs
// - Maintain a persistent connection to the server
// - Queue logs if the connection is temporarily unavailable
// - Attempt to reconnect if the connection fails
//
// This class is designed as a singleton.
class SocketLogHandler {
 public:
  // Get the singleton instance
  static SocketLogHandler* GetInstance();

  // Start streaming logs to the specified server.
  void Start(const std::string& hostname, int port);

  // Stop streaming logs and disconnect from the server.
  void Stop();

  // Queue a raw formatted log message to be sent to the server.
  // This method is thread-safe and can be called from SbLogRaw.
  // message: The fully formatted log message (already includes metadata)
  void QueueRawLog(const char* message);

 private:
  SocketLogHandler();
  ~SocketLogHandler();

  // Static thread entry point
  static void* ThreadEntryPoint(void* context);

  // Background thread function that sends logs to the server
  void SendLogsThreadMain();

  // Connect to the log server
  bool ConnectToServer();

  // Disconnect from the log server
  void DisconnectFromServer();

  // Send a single log message to the server
  bool SendLogToServer(const std::string& log_message);

  // Background thread for sending logs
  SbThread send_thread_;

  // Queue of log messages waiting to be sent
  std::deque<std::string> log_queue_;

  // Lock protecting the log queue
  starboard::Mutex queue_lock_;

  // Semaphore to signal when logs are available
  starboard::Semaphore log_available_;

  // Socket connection to the log server
  SbSocket socket_;

  // Server connection details
  std::string hostname_;
  int port_;

  // Server address (resolved from hostname)
  SbSocketAddress server_address_;
  bool address_resolved_;

  // Whether the handler is active
  std::atomic<bool> is_running_;

  SocketLogHandler(const SocketLogHandler&) = delete;
  SocketLogHandler& operator=(const SocketLogHandler&) = delete;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_SOCKET_LOG_HANDLER_H_
