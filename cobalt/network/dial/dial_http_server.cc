// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/network/dial/dial_http_server.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/network/dial/dial_service.h"
#include "cobalt/network/dial/dial_service_handler.h"
#include "cobalt/network/dial/dial_system_config.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/server/http_connection.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"

namespace cobalt {
namespace network {

namespace {
const char* kXmlMimeType = "text/xml; charset=\"utf-8\"";

const char* kDdXmlFormat =
    "<?xml version=\"1.0\"?>"
    "<root"
    " xmlns=\"urn:schemas-upnp-org:device-1-0\""
    " xmlns:r=\"urn:restful-tv-org:schemas:upnp-dd\">"
    "<specVersion>"
    "<major>1</major>"
    "<minor>0</minor>"
    "</specVersion>"
    "<device>"
    "<deviceType>urn:schemas-upnp-org:device:tvdevice:1</deviceType>"
    "<friendlyName>%s</friendlyName>"
    "<manufacturer>%s</manufacturer>"
    "<modelName>%s</modelName>"
    "<UDN>uuid:%s</UDN>"
    "</device>"
    "</root>";

const char* kAppsPrefix = "/apps/";

constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("dial_http_server", "dial_http_server");

base::Optional<net::IPEndPoint> GetLocalIpAddress() {
  net::IPEndPoint ip_addr;
  SbSocketAddress local_ip;
  memset(&local_ip, 0, sizeof(local_ip));
  bool result = false;

  // DIAL Server only supports Ipv4 now.
  SbSocketAddressType address_types = {kSbSocketAddressTypeIpv4};
  SbSocketAddress destination;
  memset(&(destination.address), 0, sizeof(destination.address));
  destination.type = address_types;
  if (!SbSocketGetInterfaceAddress(&destination, &local_ip, NULL) ||
      !ip_addr.FromSbSocketAddress(&local_ip)) {
    DLOG(WARNING) << "Unable to get a local interface address.";
    return base::nullopt;
  }

  return ip_addr;
}
}  // namespace

const int kDialHttpServerPort = 0;  // Random Port.

DialHttpServer::DialHttpServer(DialService* dial_service)
    : dial_service_(dial_service),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(dial_service);
  DCHECK(task_runner_);

  auto* server_socket =
      new net::TCPServerSocket(NULL /*net_log*/, net::NetLogSource());
  base::Optional<net::IPEndPoint> ip_addr = GetLocalIpAddress();
  if (!ip_addr) {
    LOG(ERROR) << "Can not get a local address for DIAL HTTP Server";
  } else {
    server_socket->ListenWithAddressAndPort(
        ip_addr.value().address().ToString(), ip_addr.value().port(),
        1 /*backlog*/);
  }
  http_server_.reset(new net::HttpServer(
      std::unique_ptr<net::ServerSocket>(server_socket), this));
  ConfigureApplicationUrl();
}

DialHttpServer::~DialHttpServer() {
  // Stop() must have been called prior to destruction, to ensure the
  // server was destroyed on the right thread.
  DCHECK(!http_server_);
}

void DialHttpServer::Stop() {
  DCHECK_EQ(task_runner_, base::ThreadTaskRunnerHandle::Get());
  http_server_.reset();
}

int DialHttpServer::GetLocalAddress(net::IPEndPoint* addr) {
  DCHECK_EQ(task_runner_, base::ThreadTaskRunnerHandle::Get());
  // We want to give second screen the IPv4 address, but we still need to
  // get http_server_'s address for its port number.
  int ret = http_server_->GetLocalAddress(addr);

  if (ret != 0) {
    return net::ERR_FAILED;
  }

  SbSocketAddress local_ip = {0};

  // Now get the IPAddress of the network card.
  SbSocketAddress destination = {0};
  SbSocketAddress netmask = {0};

  // DIAL only works with IPv4.
  destination.type = kSbSocketAddressTypeIpv4;
  if (!SbSocketGetInterfaceAddress(&destination, &local_ip, NULL)) {
    return net::ERR_FAILED;
  }
  local_ip.port = addr->port();
  LOG_ONCE(INFO) << "In-App DIAL Address http://" << addr->address().ToString()
                 << ":" << addr->port();

  if (addr->FromSbSocketAddress(&local_ip)) {
    return net::OK;
  }

  return net::ERR_FAILED;
}

void DialHttpServer::OnHttpRequest(int conn_id,
                                   const net::HttpServerRequestInfo& info) {
  TRACE_EVENT0("net::dial", "DialHttpServer::OnHttpRequest");
  DCHECK_EQ(task_runner_, base::ThreadTaskRunnerHandle::Get());
  if (info.method == "GET" &&
      base::LowerCaseEqualsASCII(info.path, "/dd.xml")) {
    // If dd.xml request
    SendDeviceDescriptionManifest(conn_id);

  } else if (strstr(info.path.c_str(), kAppsPrefix)) {
    if (info.method == "GET" && info.path.length() == strlen(kAppsPrefix)) {
      // If /apps/ request, send 302 to current application.
      http_server_->SendRaw(
          conn_id,
          base::StringPrintf("HTTP/1.1 %d %s\r\n"
                             "Location: %s\r\n"
                             "Content-Length: 0\r\n"
                             "\r\n",
                             net::HTTP_FOUND, "Found",
                             (application_url() + "YouTube").c_str()),
          kNetworkTrafficAnnotation);

    } else if (!DispatchToHandler(conn_id, info)) {
      // If handled, let it pass. Otherwise, send 404.
      http_server_->Send404(conn_id, kNetworkTrafficAnnotation);
    }
  } else {
    // For all other cases, send 404.
    http_server_->Send404(conn_id, kNetworkTrafficAnnotation);
  }
}

void DialHttpServer::OnClose(int conn_id) {}

void DialHttpServer::ConfigureApplicationUrl() {
  net::IPEndPoint end_point;
  if (net::OK != GetLocalAddress(&end_point)) {
    LOG(ERROR) << "Could not get the local URL!";
    return;
  }
  std::string addr = end_point.ToString();
  DCHECK(!addr.empty());

  server_url_ = base::StringPrintf("http://%s/", addr.c_str());
}

void DialHttpServer::SendDeviceDescriptionManifest(int conn_id) {
  DCHECK_EQ(task_runner_, base::ThreadTaskRunnerHandle::Get());
  DialSystemConfig* system_config = DialSystemConfig::GetInstance();
#if defined(COBALT_BUILD_TYPE_GOLD)
  const char* friendly_name = system_config->friendly_name();
#else
  // For non-Gold builds, append the IP to the friendly name
  // to help differentiate the devices.
  std::string friendly_name_str = system_config->friendly_name();
  net::IPEndPoint end_point;
  if (net::OK == GetLocalAddress(&end_point)) {
    friendly_name_str += " ";
    friendly_name_str += end_point.ToStringWithoutPort();
  }
  const char* friendly_name = friendly_name_str.c_str();
#endif

  std::string response_body = base::StringPrintf(
      kDdXmlFormat, friendly_name, system_config->manufacturer_name(),
      system_config->model_name(), system_config->model_uuid());

  net::HttpServerResponseInfo response_info(net::HTTP_OK);
  response_info.SetBody(response_body, kXmlMimeType);
  response_info.AddHeader("Application-URL", application_url().c_str());

  http_server_->SendResponse(conn_id, response_info, kNetworkTrafficAnnotation);
}

bool DialHttpServer::DispatchToHandler(int conn_id,
                                       const net::HttpServerRequestInfo& info) {
  DCHECK_EQ(task_runner_, base::ThreadTaskRunnerHandle::Get());
  // See if DialService has a handler for this request.
  TRACE_EVENT0("net::dial", __FUNCTION__);
  std::string handler_path;
  scoped_refptr<DialServiceHandler> handler =
      dial_service_->GetHandler(info.path, &handler_path);
  if (handler.get() == NULL) {
    return false;
  }

  handler->HandleRequest(
      handler_path, info,
      base::Bind(&DialHttpServer::OnReceivedResponse, this, conn_id));
  return true;
}

void DialHttpServer::OnReceivedResponse(
    int conn_id, std::unique_ptr<net::HttpServerResponseInfo> response) {
  if (task_runner_ != base::ThreadTaskRunnerHandle::Get()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&DialHttpServer::OnReceivedResponse, this,
                                      conn_id, base::Passed(&response)));
    return;
  }

  DCHECK_EQ(task_runner_, base::ThreadTaskRunnerHandle::Get());
  TRACE_EVENT0("net::dial", __FUNCTION__);
  if (response) {
    http_server_->SendResponse(conn_id, *(response.get()),
                               kNetworkTrafficAnnotation);
  } else {
    http_server_->Send404(conn_id, kNetworkTrafficAnnotation);
  }
}

}  // namespace network
}  // namespace cobalt
