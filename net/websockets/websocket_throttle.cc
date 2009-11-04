// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_throttle.h"

#if defined(OS_WIN)
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

#include <string>

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "net/base/io_buffer.h"
#include "net/socket_stream/socket_stream.h"

namespace net {

static std::string AddrinfoToHashkey(const struct addrinfo* addrinfo) {
  switch (addrinfo->ai_family) {
    case AF_INET: {
      const struct sockaddr_in* const addr =
          reinterpret_cast<const sockaddr_in*>(addrinfo->ai_addr);
      return StringPrintf("%d:%s",
                          addrinfo->ai_family,
                          HexEncode(&addr->sin_addr, 4).c_str());
      }
    case AF_INET6: {
      const struct sockaddr_in6* const addr6 =
          reinterpret_cast<const sockaddr_in6*>(addrinfo->ai_addr);
      return StringPrintf("%d:%s",
                          addrinfo->ai_family,
                          HexEncode(&addr6->sin6_addr,
                                    sizeof(addr6->sin6_addr)).c_str());
      }
    default:
      return StringPrintf("%d:%s",
                          addrinfo->ai_family,
                          HexEncode(addrinfo->ai_addr,
                                    addrinfo->ai_addrlen).c_str());
  }
}

// State for WebSocket protocol on each SocketStream.
// This is owned in SocketStream as UserData keyed by WebSocketState::kKeyName.
// This is alive between connection starts and handshake is finished.
// In this class, it doesn't check actual handshake finishes, but only checks
// end of header is found in read data.
class WebSocketThrottle::WebSocketState : public SocketStream::UserData {
 public:
  explicit WebSocketState(const AddressList& addrs)
      : address_list_(addrs),
        callback_(NULL),
        waiting_(false),
        handshake_finished_(false),
        buffer_(NULL) {
  }
  ~WebSocketState() {}

  int OnStartOpenConnection(CompletionCallback* callback) {
    DCHECK(!callback_);
    if (!waiting_)
      return OK;
    callback_ = callback;
    return ERR_IO_PENDING;
  }

  int OnRead(const char* data, int len, CompletionCallback* callback) {
    DCHECK(!waiting_);
    DCHECK(!callback_);
    DCHECK(!handshake_finished_);
    static const int kBufferSize = 8129;

    if (!buffer_) {
      // Fast path.
      int eoh = HttpUtil::LocateEndOfHeaders(data, len, 0);
      if (eoh > 0) {
        handshake_finished_ = true;
        return OK;
      }
      buffer_ = new GrowableIOBuffer();
      buffer_->SetCapacity(kBufferSize);
    } else {
      if (buffer_->RemainingCapacity() < len) {
        if (!buffer_->SetCapacity(buffer_->capacity() + kBufferSize)) {
          // TODO(ukai): Check more correctly.
          // Seek to the last CR or LF and reduce memory usage.
          LOG(ERROR) << "Too large headers? capacity=" << buffer_->capacity();
          handshake_finished_ = true;
          return OK;
        }
      }
    }
    memcpy(buffer_->data(), data, len);
    buffer_->set_offset(buffer_->offset() + len);

    int eoh = HttpUtil::LocateEndOfHeaders(buffer_->StartOfBuffer(),
                                           buffer_->offset(), 0);
    handshake_finished_ = (eoh > 0);
    return OK;
  }

  const AddressList& address_list() const { return address_list_; }
  void SetWaiting() { waiting_ = true; }
  bool IsWaiting() const { return waiting_; }
  bool HandshakeFinished() const { return handshake_finished_; }
  void Wakeup() {
    waiting_ = false;
    // We wrap |callback_| to keep this alive while this is released.
    scoped_refptr<CompletionCallbackRunner> runner =
        new CompletionCallbackRunner(callback_);
    callback_ = NULL;
    MessageLoopForIO::current()->PostTask(
        FROM_HERE,
        NewRunnableMethod(runner.get(),
                          &CompletionCallbackRunner::Run));
  }

  static const char* kKeyName;

 private:
  class CompletionCallbackRunner
      : public base::RefCountedThreadSafe<CompletionCallbackRunner> {
   public:
    explicit CompletionCallbackRunner(CompletionCallback* callback)
        : callback_(callback) {
      DCHECK(callback_);
    }
    virtual ~CompletionCallbackRunner() {}
    void Run() {
      callback_->Run(OK);
    }
   private:
    CompletionCallback* callback_;

    DISALLOW_COPY_AND_ASSIGN(CompletionCallbackRunner);
  };

  const AddressList& address_list_;

  CompletionCallback* callback_;
  // True if waiting another websocket connection is established.
  // False if the websocket is performing handshaking.
  bool waiting_;

  // True if the websocket handshake is completed.
  // If true, it will be removed from queue and deleted from the SocketStream
  // UserData soon.
  bool handshake_finished_;

  // Buffer for read data to check handshake response message.
  scoped_refptr<GrowableIOBuffer> buffer_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketState);
};

const char* WebSocketThrottle::WebSocketState::kKeyName = "WebSocketState";

WebSocketThrottle::WebSocketThrottle() {
  SocketStreamThrottle::RegisterSocketStreamThrottle("ws", this);
  SocketStreamThrottle::RegisterSocketStreamThrottle("wss", this);
}

WebSocketThrottle::~WebSocketThrottle() {
  DCHECK(queue_.empty());
  DCHECK(addr_map_.empty());
}

int WebSocketThrottle::OnStartOpenConnection(
    SocketStream* socket, CompletionCallback* callback) {
  WebSocketState* state = new WebSocketState(socket->address_list());
  PutInQueue(socket, state);
  return state->OnStartOpenConnection(callback);
}

int WebSocketThrottle::OnRead(SocketStream* socket,
                              const char* data, int len,
                              CompletionCallback* callback) {
  WebSocketState* state = static_cast<WebSocketState*>(
      socket->GetUserData(WebSocketState::kKeyName));
  // If no state, handshake was already completed. Do nothing.
  if (!state)
    return OK;

  int result = state->OnRead(data, len, callback);
  if (state->HandshakeFinished()) {
    RemoveFromQueue(socket, state);
    WakeupSocketIfNecessary();
  }
  return result;
}

int WebSocketThrottle::OnWrite(SocketStream* socket,
                               const char* data, int len,
                               CompletionCallback* callback) {
  // Do nothing.
  return OK;
}

void WebSocketThrottle::OnClose(SocketStream* socket) {
  WebSocketState* state = static_cast<WebSocketState*>(
      socket->GetUserData(WebSocketState::kKeyName));
  if (!state)
    return;
  RemoveFromQueue(socket, state);
  WakeupSocketIfNecessary();
}

void WebSocketThrottle::PutInQueue(SocketStream* socket,
                                   WebSocketState* state) {
  socket->SetUserData(WebSocketState::kKeyName, state);
  queue_.push_back(state);
  const AddressList& address_list = socket->address_list();
  for (const struct addrinfo* addrinfo = address_list.head();
       addrinfo != NULL;
       addrinfo = addrinfo->ai_next) {
    std::string addrkey = AddrinfoToHashkey(addrinfo);
    ConnectingAddressMap::iterator iter = addr_map_.find(addrkey);
    if (iter == addr_map_.end()) {
      ConnectingQueue* queue = new ConnectingQueue();
      queue->push_back(state);
      addr_map_[addrkey] = queue;
    } else {
      iter->second->push_back(state);
      state->SetWaiting();
    }
  }
}

void WebSocketThrottle::RemoveFromQueue(SocketStream* socket,
                                        WebSocketState* state) {
  const AddressList& address_list = socket->address_list();
  for (const struct addrinfo* addrinfo = address_list.head();
       addrinfo != NULL;
       addrinfo = addrinfo->ai_next) {
    std::string addrkey = AddrinfoToHashkey(addrinfo);
    ConnectingAddressMap::iterator iter = addr_map_.find(addrkey);
    DCHECK(iter != addr_map_.end());
    ConnectingQueue* queue = iter->second;
    DCHECK(state == queue->front());
    queue->pop_front();
    if (queue->empty())
      addr_map_.erase(iter);
  }
  for (ConnectingQueue::iterator iter = queue_.begin();
       iter != queue_.end();
       ++iter) {
    if (*iter == state) {
      queue_.erase(iter);
      break;
    }
  }
  socket->SetUserData(WebSocketState::kKeyName, NULL);
}

void WebSocketThrottle::WakeupSocketIfNecessary() {
  for (ConnectingQueue::iterator iter = queue_.begin();
       iter != queue_.end();
       ++iter) {
    WebSocketState* state = *iter;
    if (!state->IsWaiting())
      continue;

    bool should_wakeup = true;
    const AddressList& address_list = state->address_list();
    for (const struct addrinfo* addrinfo = address_list.head();
         addrinfo != NULL;
         addrinfo = addrinfo->ai_next) {
      std::string addrkey = AddrinfoToHashkey(addrinfo);
      ConnectingAddressMap::iterator iter = addr_map_.find(addrkey);
      DCHECK(iter != addr_map_.end());
      ConnectingQueue* queue = iter->second;
      if (state != queue->front()) {
        should_wakeup = false;
        break;
      }
    }
    if (should_wakeup)
      state->Wakeup();
  }
}

/* static */
void WebSocketThrottle::Init() {
  Singleton<WebSocketThrottle>::get();
}

}  // namespace net
