// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_FACTORY_H_
#define NET_SOCKET_CLIENT_SOCKET_FACTORY_H_

#include <string>

namespace net {

class AddressList;
class ClientSocket;
class SSLClientSocket;
struct SSLConfig;

// Callback function to create new SSLClientSocket objects.
typedef SSLClientSocket* (*SSLClientSocketFactory)(
    ClientSocket* transport_socket,
    const std::string& hostname,
    const SSLConfig& ssl_config);

// Creates SSLClientSocketNSS objects.
SSLClientSocket* SSLClientSocketNSSFactory(
    ClientSocket* transport_socket,
    const std::string& hostname,
    const SSLConfig& ssl_config);

// An interface used to instantiate ClientSocket objects.  Used to facilitate
// testing code with mock socket implementations.
class ClientSocketFactory {
 public:
  virtual ~ClientSocketFactory() {}

  virtual ClientSocket* CreateTCPClientSocket(
      const AddressList& addresses) = 0;

  virtual SSLClientSocket* CreateSSLClientSocket(
      ClientSocket* transport_socket,
      const std::string& hostname,
      const SSLConfig& ssl_config) = 0;

  // Returns the default ClientSocketFactory.
  static ClientSocketFactory* GetDefaultFactory();

  // Instructs the default ClientSocketFactory to use |factory| to create
  // SSLClientSocket objects.
  static void SetSSLClientSocketFactory(SSLClientSocketFactory factory);
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_FACTORY_H_
