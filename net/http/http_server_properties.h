// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_H_

#include <map>
#include <string>
#include "base/basictypes.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/spdy/spdy_framer.h"  // TODO(willchan): Reconsider this.

namespace net {

enum AlternateProtocol {
  NPN_SPDY_1 = 0,
  NPN_SPDY_2,
  NUM_ALTERNATE_PROTOCOLS,
  ALTERNATE_PROTOCOL_BROKEN,  // The alternate protocol is known to be broken.
  UNINITIALIZED_ALTERNATE_PROTOCOL,
};

struct NET_EXPORT PortAlternateProtocolPair {
  bool Equals(const PortAlternateProtocolPair& other) const {
    return port == other.port && protocol == other.protocol;
  }

  std::string ToString() const;

  uint16 port;
  AlternateProtocol protocol;
};

typedef std::map<HostPortPair, PortAlternateProtocolPair> AlternateProtocolMap;
typedef std::map<HostPortPair, spdy::SpdySettings> SpdySettingsMap;

extern const char kAlternateProtocolHeader[];
extern const char* const kAlternateProtocolStrings[NUM_ALTERNATE_PROTOCOLS];

// The interface for setting/retrieving the HTTP server properties.
// Currently, this class manages servers':
// * SPDY support (based on NPN results)
// * Alternate-Protocol support
// * Spdy Settings (like CWND ID field)
class NET_EXPORT HttpServerProperties {
 public:
  HttpServerProperties() {}
  virtual ~HttpServerProperties() {}

  // Deletes all data.
  virtual void Clear() = 0;

  // Returns true if |server| supports SPDY.
  virtual bool SupportsSpdy(const HostPortPair& server) const = 0;

  // Add |server| into the persistent store. Should only be called from IO
  // thread.
  virtual void SetSupportsSpdy(const HostPortPair& server,
                               bool support_spdy) = 0;

  // Returns true if |server| has an Alternate-Protocol header.
  virtual bool HasAlternateProtocol(const HostPortPair& server) const = 0;

  // Returns the Alternate-Protocol and port for |server|.
  // HasAlternateProtocol(server) must be true.
  virtual PortAlternateProtocolPair GetAlternateProtocol(
      const HostPortPair& server) const = 0;

  // Sets the Alternate-Protocol for |server|.
  virtual void SetAlternateProtocol(const HostPortPair& server,
                                    uint16 alternate_port,
                                    AlternateProtocol alternate_protocol) = 0;

  // Sets the Alternate-Protocol for |server| to be BROKEN.
  virtual void SetBrokenAlternateProtocol(const HostPortPair& server) = 0;

  // Returns all Alternate-Protocol mappings.
  virtual const AlternateProtocolMap& alternate_protocol_map() const = 0;

  // Gets a reference to the SpdySettings stored for a host.
  // If no settings are stored, returns an empty set of settings.
  virtual const spdy::SpdySettings& GetSpdySettings(
      const HostPortPair& host_port_pair) const = 0;

  // Saves settings for a host. Returns true if SpdySettings are to be
  // persisted.
  virtual bool SetSpdySettings(const HostPortPair& host_port_pair,
                               const spdy::SpdySettings& settings) = 0;

  // Clears all spdy_settings.
  virtual void ClearSpdySettings() = 0;

  // Returns all persistent SpdySettings.
  virtual const SpdySettingsMap& spdy_settings_map() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpServerProperties);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_H_
