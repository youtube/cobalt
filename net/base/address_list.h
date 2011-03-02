// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_LIST_H_
#define NET_BASE_ADDRESS_LIST_H_
#pragma once

#include <string>

#include "base/ref_counted.h"
#include "net/base/net_util.h"

struct addrinfo;

namespace net {

// An AddressList object contains a linked list of addrinfo structures.  This
// class is designed to be copied around by value.
class AddressList {
 public:
  // Constructs an empty address list.
  AddressList();

  // Constructs an address list for a single IP literal.  If
  // |canonicalize_name| is true, fill the ai_canonname field with the
  // canonicalized IP address.
  AddressList(const IPAddressNumber& address, int port, bool canonicalize_name);

  AddressList(const AddressList& addresslist);
  ~AddressList();
  AddressList& operator=(const AddressList& addresslist);

  // Adopt the given addrinfo list (assumed to have been created by
  // the system, e.g. returned by getaddrinfo()) in place of the
  // existing one if any.  This hands over responsibility for freeing
  // the addrinfo list to the AddressList object.
  void Adopt(struct addrinfo* head);

  // Copies the given addrinfo rather than adopting it. If |recursive| is true,
  // all linked struct addrinfos will be copied as well. Otherwise only the head
  // will be copied, and the rest of linked entries will be ignored.
  void Copy(const struct addrinfo* head, bool recursive);

  // Appends a copy of |head| and all its linked addrinfos to the stored
  // addrinfo.
  void Append(const struct addrinfo* head);

  // Sets the port of all addresses in the list to |port| (that is the
  // sin[6]_port field for the sockaddrs).
  void SetPort(int port);

  // Retrieves the port number of the first sockaddr in the list. (If SetPort()
  // was previously used on this list, then all the addresses will have this
  // same port number.)
  int GetPort() const;

  // Sets the address to match |src|, and have each sockaddr's port be |port|.
  // If |src| already has the desired port this operation is cheap (just adds
  // a reference to |src|'s data.) Otherwise we will make a copy.
  void SetFrom(const AddressList& src, int port);

  // Gets the canonical name for the address.
  // If the canonical name exists, |*canonical_name| is filled in with the
  // value and true is returned. If it does not exist, |*canonical_name| is
  // not altered and false is returned.
  // |canonical_name| must be a non-null value.
  bool GetCanonicalName(std::string* canonical_name) const;

  // Clears all data from this address list. This leaves the list in the same
  // empty state as when first constructed.
  void Reset();

  // Get access to the head of the addrinfo list.
  const struct addrinfo* head() const;

  // Constructs an address list for a single socket address.
  // |address| the sockaddr to copy.
  // |socket_type| is either SOCK_STREAM or SOCK_DGRAM.
  // |protocol| is either IPPROTO_TCP or IPPROTO_UDP.
  static AddressList* CreateAddressListFromSockaddr(
      const struct sockaddr* address,
      socklen_t address_length,
      int socket_type,
      int protocol);

 private:
  struct Data;

  explicit AddressList(Data* data);

  scoped_refptr<Data> data_;
};

}  // namespace net

#endif  // NET_BASE_ADDRESS_LIST_H_
