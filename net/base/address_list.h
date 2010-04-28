// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_LIST_H_
#define NET_BASE_ADDRESS_LIST_H_

#include <string>

#include "base/ref_counted.h"

struct addrinfo;

namespace net {

// An AddressList object contains a linked list of addrinfo structures.  This
// class is designed to be copied around by value.
class AddressList {
 public:
  // Constructs an empty address list.
  AddressList() {}

  // Adopt the given addrinfo list in place of the existing one if any.  This
  // hands over responsibility for freeing the addrinfo list to the AddressList
  // object.
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

  // Used by unit-tests to manually create an IPv4 AddressList. |data| should
  // be an IPv4 address in network order (big endian).
  // If |canonical_name| is non-empty, it will be duplicated in the
  // ai_canonname field of the addrinfo struct.
  static AddressList CreateIPv4Address(unsigned char data[4],
                                       const std::string& canonical_name);

  // Used by unit-tests to manually create an IPv6 AddressList. |data| should
  // be an IPv6 address in network order (big endian).
  // If |canonical_name| is non-empty, it will be duplicated in the
  // ai_canonname field of the addrinfo struct.
  static AddressList CreateIPv6Address(unsigned char data[16],
                                       const std::string& canonical_name);

  // Get access to the head of the addrinfo list.
  const struct addrinfo* head() const { return data_->head; }

 private:
  struct Data : public base::RefCountedThreadSafe<Data> {
    Data(struct addrinfo* ai, bool is_system_created);
    struct addrinfo* head;

    // Indicates which free function to use for |head|.
    bool is_system_created;

   private:
    friend class base::RefCountedThreadSafe<Data>;

    ~Data();
  };

  explicit AddressList(Data* data) : data_(data) {}

  scoped_refptr<Data> data_;
};

}  // namespace net

#endif  // NET_BASE_ADDRESS_LIST_H_
