// Copyright 2026 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// SPDX-License-Identifier: Apache-2.0

#include "local_aows_server.h"

#include "base/base64.h"
#include "base/hash/sha1.h"
#include "starboard/common/log.h"

#include <arpa/inet.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace starboard {
namespace rdk {
namespace shared {
namespace microphone {
namespace {

const char logtag[] = "sbLAOWS";
const char kAowsHost[] = "127.0.0.1";
const char kWebSocketGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const int kDefaultAowsPort = 9880;
const int kSocketBacklog = 1;
const size_t kMaxHandshakeBytes = 8192;
const size_t kMaxFramePayloadBytes = 64 * 1024;
const size_t kMaxBufferedAudioBytes = 512 * 1024;

std::string TrimAsciiWhitespace(const std::string& value) {
  const size_t begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return std::string();
  }

  const size_t end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::string ToLowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char character) {
                   if (character >= 'A' && character <= 'Z') {
                     return static_cast<char>(character - 'A' + 'a');
                   }
                   return static_cast<char>(character);
                 });
  return value;
}

int ParseAowsPort() {
  const char* value = std::getenv("SB_AOWS_PORT");
  if (!value || !value[0]) {
    return kDefaultAowsPort;
  }

  char* end = nullptr;
  const long parsed_port = std::strtol(value, &end, 10);
  if (end == value || (end && *end != '\0') || parsed_port <= 0 ||
      parsed_port > std::numeric_limits<uint16_t>::max()) {
    SB_LOG(WARNING) << logtag << ": Ignoring invalid SB_AOWS_PORT='" << value
                    << "', using default port " << kDefaultAowsPort;
    return kDefaultAowsPort;
  }

  return static_cast<int>(parsed_port);
}

bool ReadExact(int socket_fd, void* buffer, size_t size) {
  uint8_t* bytes = static_cast<uint8_t*>(buffer);
  size_t offset = 0;
  while (offset < size) {
    const ssize_t bytes_read = recv(socket_fd, bytes + offset, size - offset, 0);
    if (bytes_read == 0) {
      return false;
    }
    if (bytes_read < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    offset += static_cast<size_t>(bytes_read);
  }
  return true;
}

bool SendAll(int socket_fd, const void* buffer, size_t size) {
  const uint8_t* bytes = static_cast<const uint8_t*>(buffer);
  size_t offset = 0;
  while (offset < size) {
    const ssize_t bytes_sent = send(socket_fd, bytes + offset, size - offset, 0);
    if (bytes_sent < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    offset += static_cast<size_t>(bytes_sent);
  }
  return true;
}

bool ReadHttpRequest(int socket_fd, std::string* request) {
  request->clear();
  char buffer[1024];
  while (request->find("\r\n\r\n") == std::string::npos) {
    const ssize_t bytes_read = recv(socket_fd, buffer, sizeof(buffer), 0);
    if (bytes_read == 0) {
      return false;
    }
    if (bytes_read < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }

    request->append(buffer, static_cast<size_t>(bytes_read));
    if (request->size() > kMaxHandshakeBytes) {
      return false;
    }
  }
  return true;
}

bool SendControlFrame(int socket_fd,
                      uint8_t opcode,
                      const uint8_t* payload,
                      size_t payload_size) {
  if (payload_size > 125) {
    return false;
  }

  uint8_t frame_header[2] = {
      static_cast<uint8_t>(0x80 | (opcode & 0x0F)),
      static_cast<uint8_t>(payload_size),
  };

  if (!SendAll(socket_fd, frame_header, sizeof(frame_header))) {
    return false;
  }

  if (payload_size == 0) {
    return true;
  }

  return SendAll(socket_fd, payload, payload_size);
}

bool SendCloseFrame(int socket_fd) {
  return SendControlFrame(socket_fd, 0x8, nullptr, 0);
}

bool PerformWebSocketHandshake(int socket_fd) {
  std::string request;
  if (!ReadHttpRequest(socket_fd, &request)) {
    return false;
  }

  const size_t request_line_end = request.find("\r\n");
  if (request_line_end == std::string::npos ||
      request.compare(0, 4, "GET ") != 0) {
    return false;
  }

  std::string websocket_key;
  size_t line_start = request_line_end + 2;
  while (line_start < request.size()) {
    const size_t line_end = request.find("\r\n", line_start);
    if (line_end == std::string::npos || line_end == line_start) {
      break;
    }

    const std::string line = request.substr(line_start, line_end - line_start);
    const size_t separator = line.find(':');
    if (separator != std::string::npos) {
      const std::string key = ToLowerAscii(line.substr(0, separator));
      const std::string value = TrimAsciiWhitespace(line.substr(separator + 1));
      if (key == "sec-websocket-key") {
        websocket_key = value;
      }
    }

    line_start = line_end + 2;
  }

  if (websocket_key.empty()) {
    return false;
  }

  std::string accept_key;
  base::Base64Encode(base::SHA1HashString(websocket_key + kWebSocketGuid),
                     &accept_key);

  const std::string response =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: " + accept_key + "\r\n"
      "\r\n";
  return SendAll(socket_fd, response.data(), response.size());
}

bool ReadWebSocketFrame(int socket_fd,
                        uint8_t* opcode,
                        std::vector<uint8_t>* payload) {
  uint8_t frame_header[2];
  if (!ReadExact(socket_fd, frame_header, sizeof(frame_header))) {
    return false;
  }

  const bool fin = (frame_header[0] & 0x80) != 0;
  *opcode = frame_header[0] & 0x0F;
  const bool masked = (frame_header[1] & 0x80) != 0;
  uint64_t payload_size = frame_header[1] & 0x7F;

  if (!fin) {
    return false;
  }

  if (payload_size == 126) {
    uint8_t extended_payload[2];
    if (!ReadExact(socket_fd, extended_payload, sizeof(extended_payload))) {
      return false;
    }
    payload_size = (static_cast<uint64_t>(extended_payload[0]) << 8) |
                   static_cast<uint64_t>(extended_payload[1]);
  } else if (payload_size == 127) {
    uint8_t extended_payload[8];
    if (!ReadExact(socket_fd, extended_payload, sizeof(extended_payload))) {
      return false;
    }

    payload_size = 0;
    for (size_t index = 0; index < sizeof(extended_payload); ++index) {
      payload_size = (payload_size << 8) |
                     static_cast<uint64_t>(extended_payload[index]);
    }
  }

  if (payload_size > kMaxFramePayloadBytes) {
    return false;
  }

  uint8_t masking_key[4] = {0, 0, 0, 0};
  if (masked && !ReadExact(socket_fd, masking_key, sizeof(masking_key))) {
    return false;
  }

  payload->resize(static_cast<size_t>(payload_size));
  if (payload_size > 0 &&
      !ReadExact(socket_fd, payload->data(), static_cast<size_t>(payload_size))) {
    return false;
  }

  if (masked) {
    for (size_t index = 0; index < payload->size(); ++index) {
      (*payload)[index] ^= masking_key[index % sizeof(masking_key)];
    }
  }

  return true;
}

class LocalAowsServer {
 public:
  LocalAowsServer()
      : port_(ParseAowsPort()),
        listen_socket_fd_(-1),
        stop_requested_(false) {}

  ~LocalAowsServer() {
    SB_LOG(INFO) << logtag << ": Stopping local AOWS server.";
    stop_requested_.store(true);
    const int listen_socket_fd = listen_socket_fd_.exchange(-1);
    if (listen_socket_fd >= 0) {
      close(listen_socket_fd);
    }

    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  }

  void EnsureStarted() {
    std::lock_guard<std::mutex> lock(start_mutex_);
    if (server_thread_.joinable()) {
      return;
    }

    server_thread_ = std::thread(&LocalAowsServer::Run, this);
  }

  void ClearBufferedAudio() {
    std::lock_guard<std::mutex> lock(audio_mutex_);
    audio_buffer_.clear();
  }

  int ReadAudio(void* out_audio_data, int audio_data_size) {
    std::lock_guard<std::mutex> lock(audio_mutex_);
    const int bytes_to_copy =
        std::min(audio_data_size, static_cast<int>(audio_buffer_.size()));
    uint8_t* destination = static_cast<uint8_t*>(out_audio_data);
    for (int index = 0; index < bytes_to_copy; ++index) {
      destination[index] = audio_buffer_.front();
      audio_buffer_.pop_front();
    }
    return bytes_to_copy;
  }

 private:
  void Run() {
    const int listen_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_fd < 0) {
      SB_LOG(ERROR) << logtag << ": Failed to create AOWS server socket, errno=" << errno;
      return;
    }

    const int reuse_address = 1;
    if (setsockopt(listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_address,
                   sizeof(reuse_address)) != 0) {
      SB_LOG(WARNING) << logtag << ": Failed to enable SO_REUSEADDR for local AOWS server, errno="
                      << errno;
    }

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port_));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(listen_socket_fd, reinterpret_cast<const sockaddr*>(&address),
             sizeof(address)) != 0) {
      SB_LOG(ERROR) << logtag << ": Failed to bind local AOWS server to " << kAowsHost << ':'
                    << port_ << ", errno=" << errno;
      close(listen_socket_fd);
      return;
    }

    if (listen(listen_socket_fd, kSocketBacklog) != 0) {
      SB_LOG(ERROR) << logtag << ": Failed to listen on local AOWS server socket, errno="
                    << errno;
      close(listen_socket_fd);
      return;
    }

    listen_socket_fd_.store(listen_socket_fd);
    SB_LOG(INFO) << logtag << ": Local AOWS server listening on aows://" << kAowsHost << ':'
                 << port_ << "/mic";

    while (!stop_requested_.load()) {
      sockaddr_in peer_address;
      socklen_t peer_address_length = sizeof(peer_address);
      const int connection_fd =
          accept(listen_socket_fd, reinterpret_cast<sockaddr*>(&peer_address),
                 &peer_address_length);
      if (connection_fd < 0) {
        if (stop_requested_.load() || errno == EBADF || errno == EINVAL) {
          break;
        }
        if (errno == EINTR) {
          continue;
        }
        SB_LOG(WARNING) << logtag << ": Local AOWS accept failed, errno=" << errno;
        continue;
      }

      const bool is_loopback_peer =
          peer_address.sin_family == AF_INET &&
          ntohl(peer_address.sin_addr.s_addr) == INADDR_LOOPBACK;
      if (!is_loopback_peer) {
        SB_LOG(WARNING) << logtag << ": Rejected non-loopback AOWS connection.";
        close(connection_fd);
        continue;
      }

      HandleConnection(connection_fd);
      close(connection_fd);
    }

    const int closed_socket_fd = listen_socket_fd_.exchange(-1);
    if (closed_socket_fd >= 0) {
      close(closed_socket_fd);
    }
  }

  void HandleConnection(int connection_fd) {
    if (!PerformWebSocketHandshake(connection_fd)) {
      SB_LOG(WARNING) << logtag << ": Failed websocket handshake on local AOWS socket.";
      return;
    }

    SB_LOG(INFO) << logtag << ": Accepted local AOWS client connection.";

    std::vector<uint8_t> payload;
    while (!stop_requested_.load()) {
      uint8_t opcode = 0;
      if (!ReadWebSocketFrame(connection_fd, &opcode, &payload)) {
        break;
      }

      switch (opcode) {
        case 0x2:
          AppendAudio(payload);
          break;
        case 0x8:
          SendCloseFrame(connection_fd);
          return;
        case 0x9:
          if (!SendControlFrame(connection_fd, 0xA, payload.data(),
                                payload.size())) {
            return;
          }
          break;
        case 0xA:
          break;
        default:
          break;
      }
    }

    SB_LOG(INFO) << logtag << ": Local AOWS client connection closed.";
  }

  void AppendAudio(const std::vector<uint8_t>& payload) {
    if (payload.empty()) {
      return;
    }

    std::lock_guard<std::mutex> lock(audio_mutex_);
    if (payload.size() >= kMaxBufferedAudioBytes) {
      audio_buffer_.clear();
      audio_buffer_.insert(audio_buffer_.end(),
                           payload.end() - kMaxBufferedAudioBytes,
                           payload.end());
      return;
    }

    const size_t overflow =
        audio_buffer_.size() + payload.size() > kMaxBufferedAudioBytes
            ? audio_buffer_.size() + payload.size() - kMaxBufferedAudioBytes
            : 0;
    for (size_t index = 0; index < overflow; ++index) {
      audio_buffer_.pop_front();
    }

    audio_buffer_.insert(audio_buffer_.end(), payload.begin(), payload.end());
  }

  const int port_;
  std::mutex start_mutex_;
  std::mutex audio_mutex_;
  std::deque<uint8_t> audio_buffer_;
  std::thread server_thread_;
  std::atomic<int> listen_socket_fd_;
  std::atomic<bool> stop_requested_;
};

LocalAowsServer& GetLocalAowsServer() {
  static LocalAowsServer server;
  return server;
}

}  // namespace

void EnsureLocalAowsServerStarted() {
  GetLocalAowsServer().EnsureStarted();
}

void ClearLocalAowsBufferedAudio() {
  GetLocalAowsServer().ClearBufferedAudio();
}

int ReadLocalAowsAudio(void* out_audio_data, int audio_data_size) {
  return GetLocalAowsServer().ReadAudio(out_audio_data, audio_data_size);
}

}  // namespace microphone
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
