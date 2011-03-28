// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_throttle.h"

#include <string>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "net/base/io_buffer.h"
#include "net/base/sys_addrinfo.h"
#include "net/socket_stream/socket_stream.h"
#include "net/websockets/websocket_job.h"

namespace net {

static std::string AddrinfoToHashkey(const struct addrinfo* addrinfo) {
  switch (addrinfo->ai_family) {
    case AF_INET: {
      const struct sockaddr_in* const addr =
          reinterpret_cast<const sockaddr_in*>(addrinfo->ai_addr);
      return base::StringPrintf("%d:%s",
                                addrinfo->ai_family,
                                base::HexEncode(&addr->sin_addr, 4).c_str());
      }
    case AF_INET6: {
      const struct sockaddr_in6* const addr6 =
          reinterpret_cast<const sockaddr_in6*>(addrinfo->ai_addr);
      return base::StringPrintf(
          "%d:%s",
          addrinfo->ai_family,
          base::HexEncode(&addr6->sin6_addr,
                          sizeof(addr6->sin6_addr)).c_str());
      }
    default:
      return base::StringPrintf("%d:%s",
                                addrinfo->ai_family,
                                base::HexEncode(addrinfo->ai_addr,
                                                addrinfo->ai_addrlen).c_str());
  }
}

WebSocketThrottle::WebSocketThrottle() {
}

WebSocketThrottle::~WebSocketThrottle() {
  DCHECK(queue_.empty());
  DCHECK(addr_map_.empty());
}

// static
WebSocketThrottle* WebSocketThrottle::GetInstance() {
  return Singleton<WebSocketThrottle>::get();
}

void WebSocketThrottle::PutInQueue(WebSocketJob* job) {
  queue_.push_back(job);
  const AddressList& address_list = job->address_list();
  base::hash_set<std::string> address_set;
  for (const struct addrinfo* addrinfo = address_list.head();
       addrinfo != NULL;
       addrinfo = addrinfo->ai_next) {
    std::string addrkey = AddrinfoToHashkey(addrinfo);

    // If |addrkey| is already processed, don't do it again.
    if (address_set.find(addrkey) != address_set.end())
      continue;
    address_set.insert(addrkey);

    ConnectingAddressMap::iterator iter = addr_map_.find(addrkey);
    if (iter == addr_map_.end()) {
      ConnectingQueue* queue = new ConnectingQueue();
      queue->push_back(job);
      addr_map_[addrkey] = queue;
    } else {
      iter->second->push_back(job);
      job->SetWaiting();
      DVLOG(1) << "Waiting on " << addrkey;
    }
  }
}

void WebSocketThrottle::RemoveFromQueue(WebSocketJob* job) {
  bool in_queue = false;
  for (ConnectingQueue::iterator iter = queue_.begin();
       iter != queue_.end();
       ++iter) {
    if (*iter == job) {
      queue_.erase(iter);
      in_queue = true;
      break;
    }
  }
  if (!in_queue)
    return;
  const AddressList& address_list = job->address_list();
  base::hash_set<std::string> address_set;
  for (const struct addrinfo* addrinfo = address_list.head();
       addrinfo != NULL;
       addrinfo = addrinfo->ai_next) {
    std::string addrkey = AddrinfoToHashkey(addrinfo);
    // If |addrkey| is already processed, don't do it again.
    if (address_set.find(addrkey) != address_set.end())
      continue;
    address_set.insert(addrkey);

    ConnectingAddressMap::iterator iter = addr_map_.find(addrkey);
    DCHECK(iter != addr_map_.end());

    ConnectingQueue* queue = iter->second;
    // Job may not be front of queue when job is closed early while waiting.
    for (ConnectingQueue::iterator iter = queue->begin();
         iter != queue->end();
         ++iter) {
      if (*iter == job) {
        queue->erase(iter);
        break;
      }
    }
    if (queue->empty()) {
      delete queue;
      addr_map_.erase(iter);
    }
  }
}

void WebSocketThrottle::WakeupSocketIfNecessary() {
  for (ConnectingQueue::iterator iter = queue_.begin();
       iter != queue_.end();
       ++iter) {
    WebSocketJob* job = *iter;
    if (!job->IsWaiting())
      continue;

    bool should_wakeup = true;
    const AddressList& address_list = job->address_list();
    for (const struct addrinfo* addrinfo = address_list.head();
         addrinfo != NULL;
         addrinfo = addrinfo->ai_next) {
      std::string addrkey = AddrinfoToHashkey(addrinfo);
      ConnectingAddressMap::iterator iter = addr_map_.find(addrkey);
      DCHECK(iter != addr_map_.end());
      ConnectingQueue* queue = iter->second;
      if (job != queue->front()) {
        should_wakeup = false;
        break;
      }
    }
    if (should_wakeup)
      job->Wakeup();
  }
}

}  // namespace net
