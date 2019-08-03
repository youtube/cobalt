// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/mru_cache.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/http_pipelined_host_capability.h"
#include "net/http/http_server_properties.h"

namespace base {
class ListValue;
}

namespace net {

// The implementation for setting/retrieving the HTTP server properties.
class NET_EXPORT HttpServerPropertiesImpl
    : public HttpServerProperties,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  HttpServerPropertiesImpl();
  virtual ~HttpServerPropertiesImpl();

  // Initializes |spdy_servers_table_| with the servers (host/port) from
  // |spdy_servers| that either support SPDY or not.
  void InitializeSpdyServers(std::vector<std::string>* spdy_servers,
                             bool support_spdy);

  void InitializeAlternateProtocolServers(
      AlternateProtocolMap* alternate_protocol_servers);

  void InitializeSpdySettingsServers(SpdySettingsMap* spdy_settings_map);

  // Initializes |pipeline_capability_map_| with the servers (host/port) from
  // |pipeline_capability_map| that either support HTTP pipelining or not.
  void InitializePipelineCapabilities(
      const PipelineCapabilityMap* pipeline_capability_map);

  // Get the list of servers (host/port) that support SPDY.
  void GetSpdyServerList(base::ListValue* spdy_server_list) const;

  // Returns flattened string representation of the |host_port_pair|. Used by
  // unittests.
  static std::string GetFlattenedSpdyServer(
      const net::HostPortPair& host_port_pair);

  // Debugging to simulate presence of an AlternateProtocol.
  // If we don't have an alternate protocol in the map for any given host/port
  // pair, force this ProtocolPortPair.
  static void ForceAlternateProtocol(const PortAlternateProtocolPair& pair);
  static void DisableForcedAlternateProtocol();

  // Changes the number of host/port pairs we remember pipelining capability
  // for. A larger number means we're more likely to be able to pipeline
  // immediately if a host is known good, but uses more memory. This function
  // can only be called if |pipeline_capability_map_| is empty.
  void SetNumPipelinedHostsToRemember(int max_size);

  // -----------------------------
  // HttpServerProperties methods:
  // -----------------------------

  // Deletes all data.
  virtual void Clear() OVERRIDE;

  // Returns true if |server| supports SPDY.
  virtual bool SupportsSpdy(const HostPortPair& server) const OVERRIDE;

  // Add |server| into the persistent store.
  virtual void SetSupportsSpdy(const HostPortPair& server,
                               bool support_spdy) OVERRIDE;

  // Returns true if |server| has an Alternate-Protocol header.
  virtual bool HasAlternateProtocol(const HostPortPair& server) const OVERRIDE;

  // Returns the Alternate-Protocol and port for |server|.
  // HasAlternateProtocol(server) must be true.
  virtual PortAlternateProtocolPair GetAlternateProtocol(
      const HostPortPair& server) const OVERRIDE;

  // Sets the Alternate-Protocol for |server|.
  virtual void SetAlternateProtocol(
      const HostPortPair& server,
      uint16 alternate_port,
      AlternateProtocol alternate_protocol) OVERRIDE;

  // Sets the Alternate-Protocol for |server| to be BROKEN.
  virtual void SetBrokenAlternateProtocol(const HostPortPair& server) OVERRIDE;

  // Returns all Alternate-Protocol mappings.
  virtual const AlternateProtocolMap& alternate_protocol_map() const OVERRIDE;

  // Gets a reference to the SettingsMap stored for a host.
  // If no settings are stored, returns an empty SettingsMap.
  virtual const SettingsMap& GetSpdySettings(
      const HostPortPair& host_port_pair) const OVERRIDE;

  // Saves an individual SPDY setting for a host. Returns true if SPDY setting
  // is to be persisted.
  virtual bool SetSpdySetting(const HostPortPair& host_port_pair,
                              SpdySettingsIds id,
                              SpdySettingsFlags flags,
                              uint32 value) OVERRIDE;

  // Clears all entries in |spdy_settings_map_|.
  virtual void ClearSpdySettings() OVERRIDE;

  // Returns all persistent SPDY settings.
  virtual const SpdySettingsMap& spdy_settings_map() const OVERRIDE;

  virtual HttpPipelinedHostCapability GetPipelineCapability(
      const HostPortPair& origin) OVERRIDE;

  virtual void SetPipelineCapability(
      const HostPortPair& origin,
      HttpPipelinedHostCapability capability) OVERRIDE;

  virtual void ClearPipelineCapabilities() OVERRIDE;

  virtual PipelineCapabilityMap GetPipelineCapabilityMap() const OVERRIDE;

 private:
  typedef base::MRUCache<
      HostPortPair, HttpPipelinedHostCapability> CachedPipelineCapabilityMap;
  // |spdy_servers_table_| has flattened representation of servers (host/port
  // pair) that either support or not support SPDY protocol.
  typedef base::hash_map<std::string, bool> SpdyServerHostPortTable;
  SpdyServerHostPortTable spdy_servers_table_;

  AlternateProtocolMap alternate_protocol_map_;
  SpdySettingsMap spdy_settings_map_;
  scoped_ptr<CachedPipelineCapabilityMap> pipeline_capability_map_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesImpl);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_
