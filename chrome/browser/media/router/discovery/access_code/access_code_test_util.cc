// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_test_util.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/port_util.h"
#include "net/base/url_util.h"

namespace media_router {
DiscoveryDevice BuildDiscoveryDeviceProto(const char* display_name,
                                          const char* sink_id,
                                          const char* port,
                                          const char* ip_v4,
                                          const char* ip_v6,
                                          bool set_device_capabilities,
                                          bool set_network_info) {
  DiscoveryDevice discovery_proto;
  discovery_proto.set_display_name(display_name);
  discovery_proto.set_id(sink_id);

  chrome_browser_media::proto::DeviceCapabilities device_capabilities_proto;
  device_capabilities_proto.set_video_out(true);
  device_capabilities_proto.set_video_in(true);
  device_capabilities_proto.set_audio_out(true);
  device_capabilities_proto.set_audio_in(true);
  device_capabilities_proto.set_dev_mode(true);

  chrome_browser_media::proto::NetworkInfo network_info_proto;
  network_info_proto.set_host_name("GoogleNet");
  network_info_proto.set_port(port);
  network_info_proto.set_ip_v4_address(ip_v4);
  network_info_proto.set_ip_v6_address(ip_v6);

  if (set_device_capabilities) {
    *discovery_proto.mutable_device_capabilities() = device_capabilities_proto;
  }
  if (set_network_info) {
    *discovery_proto.mutable_network_info() = network_info_proto;
  }

  return discovery_proto;
}

// static
std::unique_ptr<KeyedService> MockAccessCodeCastSinkService::Create(
    content::BrowserContext* context) {
  auto* profile = Profile::FromBrowserContext(context);
  return std::make_unique<testing::NiceMock<MockAccessCodeCastSinkService>>(
      profile, MediaRouterFactory::GetApiForBrowserContext(profile), nullptr,
      nullptr);
}

MockAccessCodeCastSinkService::MockAccessCodeCastSinkService(
    Profile* profile,
    MediaRouter* media_router,
    CastMediaSinkServiceImpl* cast_media_sink_service_impl,
    DiscoveryNetworkMonitor* network_monitor)
    : AccessCodeCastSinkService(profile,
                                media_router,
                                cast_media_sink_service_impl,
                                network_monitor,
                                profile->GetPrefs()) {}

MediaRoute CreateRouteForTesting(const MediaSink::Id& sink_id) {
  std::string route_id =
      "urn:x-org.chromium:media:route:1/" + sink_id + "/http://foo.com";
  return MediaRoute(route_id, MediaSource("access_code"), sink_id,
                    "access_sink", true);
}

MockAccessCodeCastSinkService::~MockAccessCodeCastSinkService() = default;

}  // namespace media_router
