// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_list.h"

#include <stdlib.h>

#include "base/logging.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"

namespace net {

namespace {

char* do_strdup(const char* src) {
#if defined(OS_WIN)
  return _strdup(src);
#else
  return strdup(src);
#endif
}

// Make a copy of |info| (the dynamically-allocated parts are copied as well).
// If |recursive| is true, chained entries via ai_next are copied too.
// Copy returned by this function should be deleted using
// DeleteCopyOfAddrinfo(), and NOT freeaddrinfo().
struct addrinfo* CreateCopyOfAddrinfo(const struct addrinfo* info,
                                      bool recursive) {
  DCHECK(info);
  struct addrinfo* copy = new addrinfo;

  // Copy all the fields (some of these are pointers, we will fix that next).
  memcpy(copy, info, sizeof(addrinfo));

  // ai_canonname is a NULL-terminated string.
  if (info->ai_canonname) {
    copy->ai_canonname = do_strdup(info->ai_canonname);
  }

  // ai_addr is a buffer of length ai_addrlen.
  if (info->ai_addr) {
    copy->ai_addr = reinterpret_cast<sockaddr *>(new char[info->ai_addrlen]);
    memcpy(copy->ai_addr, info->ai_addr, info->ai_addrlen);
  }

  // Recursive copy.
  if (recursive && info->ai_next)
    copy->ai_next = CreateCopyOfAddrinfo(info->ai_next, recursive);
  else
    copy->ai_next = NULL;

  return copy;
}

// Free an addrinfo that was created by CreateCopyOfAddrinfo().
void FreeMyAddrinfo(struct addrinfo* info) {
  DCHECK(info);
  if (info->ai_canonname)
    free(info->ai_canonname);  // Allocated by strdup.

  if (info->ai_addr)
    delete [] reinterpret_cast<char*>(info->ai_addr);

  struct addrinfo* next = info->ai_next;

  delete info;

  // Recursive free.
  if (next)
    FreeMyAddrinfo(next);
}

// Assign the port for all addresses in the list.
void SetPortRecursive(struct addrinfo* info, int port) {
  uint16* port_field = GetPortFieldFromAddrinfo(info);
  if (port_field)
    *port_field = htons(port);

  // Assign recursively.
  if (info->ai_next)
    SetPortRecursive(info->ai_next, port);
}

}  // namespace

struct AddressList::Data : public base::RefCountedThreadSafe<Data> {
  Data(struct addrinfo* ai, bool is_system_created);
  struct addrinfo* head;

  // Indicates which free function to use for |head|.
  bool is_system_created;

 private:
  friend class base::RefCountedThreadSafe<Data>;

  ~Data();
};

AddressList::AddressList() {
}

AddressList::AddressList(const IPAddressNumber& address, int port,
                         bool canonicalize_name) {
  struct addrinfo* ai = new addrinfo;
  memset(ai, 0, sizeof(addrinfo));
  ai->ai_socktype = SOCK_STREAM;

  switch (address.size()) {
    case 4: {
      ai->ai_family = AF_INET;
      const size_t sockaddr_in_size = sizeof(struct sockaddr_in);
      ai->ai_addrlen = sockaddr_in_size;

      struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(
          new char[sockaddr_in_size]);
      memset(addr, 0, sockaddr_in_size);
      addr->sin_family = AF_INET;
      memcpy(&addr->sin_addr, &address[0], 4);
      ai->ai_addr = reinterpret_cast<struct sockaddr*>(addr);
      break;
    }
    case 16: {
      ai->ai_family = AF_INET6;
      const size_t sockaddr_in6_size = sizeof(struct sockaddr_in6);
      ai->ai_addrlen = sockaddr_in6_size;

      struct sockaddr_in6* addr6 = reinterpret_cast<struct sockaddr_in6*>(
          new char[sockaddr_in6_size]);
      memset(addr6, 0, sockaddr_in6_size);
      addr6->sin6_family = AF_INET6;
      memcpy(&addr6->sin6_addr, &address[0], 16);
      ai->ai_addr = reinterpret_cast<struct sockaddr*>(addr6);
      break;
    }
    default: {
      NOTREACHED() << "Bad IP address";
      break;
    }
  }

  if (canonicalize_name) {
    std::string name = NetAddressToString(ai);
    ai->ai_canonname = do_strdup(name.c_str());
  }
  data_ = new Data(ai, false /*is_system_created*/);
  SetPort(port);
}

AddressList::AddressList(const AddressList& addresslist)
    : data_(addresslist.data_) {
}

AddressList::~AddressList() {
}

AddressList& AddressList::operator=(const AddressList& addresslist) {
  data_ = addresslist.data_;
  return *this;
}

void AddressList::Adopt(struct addrinfo* head) {
  data_ = new Data(head, true /*is_system_created*/);
}

void AddressList::Copy(const struct addrinfo* head, bool recursive) {
  data_ = new Data(CreateCopyOfAddrinfo(head, recursive),
                   false /*is_system_created*/);
}

void AddressList::Append(const struct addrinfo* head) {
  DCHECK(head);
  struct addrinfo* new_head;
  if (data_->is_system_created) {
    new_head = CreateCopyOfAddrinfo(data_->head, true);
    data_ = new Data(new_head, false /*is_system_created*/);
  } else {
    new_head = data_->head;
  }
  // Find the end of current linked list and append new data there.
  struct addrinfo* copy_ptr = new_head;
  while (copy_ptr->ai_next)
    copy_ptr = copy_ptr->ai_next;
  copy_ptr->ai_next = CreateCopyOfAddrinfo(head, true);

  // Only the head of the list should have a canonname.  Strip any
  // canonical name in the appended data.
  copy_ptr = copy_ptr->ai_next;
  while (copy_ptr) {
    if (copy_ptr->ai_canonname) {
      free(copy_ptr->ai_canonname);
      copy_ptr->ai_canonname = NULL;
    }
    copy_ptr = copy_ptr->ai_next;
  }
}

void AddressList::SetPort(int port) {
  SetPortRecursive(data_->head, port);
}

int AddressList::GetPort() const {
  return GetPortFromAddrinfo(data_->head);
}

void AddressList::SetFrom(const AddressList& src, int port) {
  if (src.GetPort() == port) {
    // We can reference the data from |src| directly.
    *this = src;
  } else {
    // Otherwise we need to make a copy in order to change the port number.
    Copy(src.head(), true);
    SetPort(port);
  }
}

bool AddressList::GetCanonicalName(std::string* canonical_name) const {
  DCHECK(canonical_name);
  if (!data_ || !data_->head->ai_canonname)
    return false;
  canonical_name->assign(data_->head->ai_canonname);
  return true;
}

void AddressList::Reset() {
  data_ = NULL;
}

const struct addrinfo* AddressList::head() const {
  if (!data_)
    return NULL;
  return data_->head;
}

AddressList::AddressList(Data* data) : data_(data) {}

// static
AddressList* AddressList::CreateAddressListFromSockaddr(
    const struct sockaddr* address,
    socklen_t address_length,
    int socket_type,
    int protocol) {
  // Do sanity checking on socket_type and protocol.
  DCHECK(socket_type == SOCK_DGRAM || socket_type == SOCK_STREAM);
  DCHECK(protocol == IPPROTO_TCP || protocol == IPPROTO_UDP);

  struct addrinfo* ai = new addrinfo;
  memset(ai, 0, sizeof(addrinfo));
  switch (address_length) {
    case sizeof(struct sockaddr_in):
      {
        const struct sockaddr_in* sin =
            reinterpret_cast<const struct sockaddr_in*>(address);
        ai->ai_family = sin->sin_family;
        DCHECK_EQ(AF_INET, ai->ai_family);
      }
      break;
    case sizeof(struct sockaddr_in6):
      {
        const struct sockaddr_in6* sin6 =
            reinterpret_cast<const struct sockaddr_in6*>(address);
        ai->ai_family = sin6->sin6_family;
        DCHECK_EQ(AF_INET6, ai->ai_family);
      }
      break;
    default:
      NOTREACHED() << "Bad IP address";
      break;
  }
  ai->ai_socktype = socket_type;
  ai->ai_protocol = protocol;
  ai->ai_addrlen = address_length;
  ai->ai_addr = reinterpret_cast<struct sockaddr*>(new char[address_length]);
  memcpy(ai->ai_addr, address, address_length);
  return new AddressList(new Data(ai, false /*is_system_created*/));
}


AddressList::Data::Data(struct addrinfo* ai, bool is_system_created)
    : head(ai), is_system_created(is_system_created) {
  DCHECK(head);
}

AddressList::Data::~Data() {
  // Call either freeaddrinfo(head), or FreeMyAddrinfo(head), depending who
  // created the data.
  if (is_system_created)
    freeaddrinfo(head);
  else
    FreeMyAddrinfo(head);
}

}  // namespace net
