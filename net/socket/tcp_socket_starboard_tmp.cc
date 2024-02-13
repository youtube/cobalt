// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "net/socket/tcp_socket_starboard_tmp.h"

#include <memory>
#include <utility>

// #include "base/functional/callback_helpers.h"
#include "net/base/net_errors.h"
#include "net/base/network_activity_monitor.h"
#include "net/socket/socket_net_log_params.h"
#include "net/socket/socket_options.h"
#include "starboard/configuration_constants.h"

namespace net {

TCPSocketStarboard::TCPSocketStarboard(
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetLog* net_log,
    const NetLogSource& source)
    : socket_performance_watcher_(std::move(socket_performance_watcher)),
      socket_(kSbSocketInvalid),
      family_(ADDRESS_FAMILY_UNSPECIFIED),
      logging_multiple_connect_attempts_(false),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::SOCKET)),
      listening_(false),
      waiting_connect_(false) {
}

TCPSocketStarboard::~TCPSocketStarboard() {
}

int TCPSocketStarboard::Open(AddressFamily family) {
  return OK;
}

int TCPSocketStarboard::AdoptConnectedSocket(SocketDescriptor socket,
                                             const IPEndPoint& peer_address) {
  return OK;
}

int TCPSocketStarboard::AdoptUnconnectedSocket(SocketDescriptor socket) {
  return OK;
}

int TCPSocketStarboard::Bind(const IPEndPoint& address) {
  return OK;
}

int TCPSocketStarboard::Listen(int backlog) {
  return OK;
}

int TCPSocketStarboard::Accept(std::unique_ptr<TCPSocketStarboard>* socket,
                               IPEndPoint* address,
                               CompletionOnceCallback callback) {
  return OK;
}

int TCPSocketStarboard::SetDefaultOptionsForServer() {
  return OK;
}

void TCPSocketStarboard::SetDefaultOptionsForClient() {
}

int TCPSocketStarboard::AcceptInternal(
    std::unique_ptr<TCPSocketStarboard>* socket,
    IPEndPoint* address) {
  return OK;
}

void TCPSocketStarboard::Close() {
}

void TCPSocketStarboard::StopWatchingAndCleanUp() {
}

void TCPSocketStarboard::OnSocketReadyToRead(SbSocket socket) {
}

void TCPSocketStarboard::OnSocketReadyToWrite(SbSocket socket) {
}

int TCPSocketStarboard::Connect(const IPEndPoint& address,
                                CompletionOnceCallback callback) {
  return OK;
}

void TCPSocketStarboard::DidCompleteConnect() {
}

void TCPSocketStarboard::StartLoggingMultipleConnectAttempts(
    const AddressList& addresses) {
}

bool TCPSocketStarboard::IsConnected() const {
  return false;
}

bool TCPSocketStarboard::IsConnectedAndIdle() const {
  return false;
}

void TCPSocketStarboard::EndLoggingMultipleConnectAttempts(int net_error) {
}

void TCPSocketStarboard::LogConnectBegin(const AddressList& addresses) const {
}

void TCPSocketStarboard::LogConnectEnd(int net_error) {
}

int TCPSocketStarboard::Read(IOBuffer* buf,
                             int buf_len,
                             CompletionOnceCallback callback) {
  return OK;
}

int TCPSocketStarboard::ReadIfReady(IOBuffer* buf,
                                    int buf_len,
                                    CompletionOnceCallback callback) {
  return OK;
}

int TCPSocketStarboard::CancelReadIfReady() {
  return net::OK;
}

void TCPSocketStarboard::RetryRead(int rv) {
}

int TCPSocketStarboard::DoRead(IOBuffer* buf, int buf_len) {
  return OK;
}

void TCPSocketStarboard::DidCompleteRead() {
}

int TCPSocketStarboard::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return OK;
}

int TCPSocketStarboard::DoWrite(IOBuffer* buf, int buf_len) {
  return OK;
}

void TCPSocketStarboard::DidCompleteWrite() {
}

bool TCPSocketStarboard::SetReceiveBufferSize(int32_t size) {
  return false;
}

bool TCPSocketStarboard::SetSendBufferSize(int32_t size) {
  return false;
}

bool TCPSocketStarboard::SetKeepAlive(bool enable, int delay) {
  return false;
}

bool TCPSocketStarboard::SetNoDelay(bool no_delay) {
  return false;
}

bool TCPSocketStarboard::GetEstimatedRoundTripTime(
    base::TimeDelta* out_rtt) const {
  return false;
}

int TCPSocketStarboard::GetLocalAddress(IPEndPoint* address) const {
  return OK;
}

int TCPSocketStarboard::GetPeerAddress(IPEndPoint* address) const {
  return OK;
}

void TCPSocketStarboard::DetachFromThread() {
}

SocketDescriptor TCPSocketStarboard::ReleaseSocketDescriptorForTesting() {
  return kSbSocketInvalid;
}

SocketDescriptor TCPSocketStarboard::SocketDescriptorForTesting() const {
  return kSbSocketInvalid;
}

void TCPSocketStarboard::ApplySocketTag(const SocketTag& tag) {
}

void TCPSocketStarboard::ClearWatcherIfOperationsNotPending() {
}

}  // namespace net
