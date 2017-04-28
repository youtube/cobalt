// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dial/dial_http_server.h"

#include <vector>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dial/dial_service.h"
#include "net/dial/dial_service_handler.h"
#include "net/dial/dial_system_config.h"
#include "net/server/http_server_request_info.h"

#if defined(__LB_SHELL__)
#include "lb_network_helpers.h"
#endif

namespace net {

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
}  // namespace

const int kDialHttpServerPort = 0;  // Random Port.

DialHttpServer::DialHttpServer(DialService* dial_service)
    : factory_("0.0.0.0", kDialHttpServerPort),
      dial_service_(dial_service),
      message_loop_proxy_(base::MessageLoopProxy::current()) {
  DCHECK(dial_service);
  DCHECK(message_loop_proxy_);

  http_server_ = new HttpServer(factory_, this);
  ConfigureApplicationUrl();
}

DialHttpServer::~DialHttpServer() {
  // Stop() must have been called prior to destruction, to ensure the
  // server was destroyed on the right thread.
  DCHECK(!http_server_);
}

void DialHttpServer::Stop() {
  DCHECK_EQ(message_loop_proxy_, base::MessageLoopProxy::current());
  http_server_ = NULL;
}

int DialHttpServer::GetLocalAddress(IPEndPoint* addr) {
  DCHECK_EQ(message_loop_proxy_, base::MessageLoopProxy::current());
  // get the port information
  int ret = http_server_->GetLocalAddress(addr);

#if defined(OS_STARBOARD)

#if SB_API_VERSION >= 4
  if (ret != 0) {
    return ERR_FAILED;
  }

  SbSocketAddress local_ip = {0};

  // Now get the IPAddress of the network card.
  SbSocketAddress destination = {0};
  SbSocketAddress netmask = {0};

  // Dial only works with IPv4.
  destination.type = kSbSocketAddressTypeIpv4;
  if (!SbSocketGetInterfaceAddress(&destination, &local_ip, NULL)) {
    return ERR_FAILED;
  }
  local_ip.port = addr->port();

  if (addr->FromSbSocketAddress(&local_ip)) {
    return OK;
  }

  return ERR_FAILED;
#else
  SbSocketAddress address;
  ret |= SbSocketGetLocalInterfaceAddress(&address) ? 0 : -1;
  address.port = addr->port();
  return (ret == 0 && addr->FromSbSocketAddress(&address)) ? OK : ERR_FAILED;
#endif  // SB_API_VERSION >= 4

#else
  // Now get the IPAddress of the network card.
  SockaddrStorage sock_addr;
  struct sockaddr_in *in = (struct sockaddr_in *)sock_addr.addr;
  ret |= lb_get_local_ip_address(&in->sin_addr);
  in->sin_family = AF_INET;
  in->sin_port = htons(addr->port());

  return (ret == 0 && addr->FromSockAddr(sock_addr.addr, sock_addr.addr_len))
      ? OK : ERR_FAILED;
#endif
}

void DialHttpServer::OnHttpRequest(int conn_id,
                                   const HttpServerRequestInfo& info) {
  TRACE_EVENT0("net::dial", "DialHttpServer::OnHttpRequest");
  DCHECK_EQ(message_loop_proxy_, base::MessageLoopProxy::current());
  if (info.method == "GET" && LowerCaseEqualsASCII(info.path, "/dd.xml")) {
    // If dd.xml request
    SendDeviceDescriptionManifest(conn_id);

  } else if (strstr(info.path.c_str(), kAppsPrefix)) {
    if (info.method == "GET" && info.path.length() == strlen(kAppsPrefix)) {
      // If /apps/ request, send 302 to current application.
      http_server_->Send302(conn_id, application_url() + "YouTube");

    } else if (!DispatchToHandler(conn_id, info)) {
      // If handled, let it pass. Otherwise, send 404.
      http_server_->Send404(conn_id);
    }
  } else {
    // For all other cases, send 404.
    http_server_->Send404(conn_id);
  }
}

void DialHttpServer::OnClose(int conn_id) {}

void DialHttpServer::ConfigureApplicationUrl() {
  IPEndPoint end_point;
  if (OK != GetLocalAddress(&end_point)) {
    LOG(ERROR) << "Could not get the local URL!";
    return;
  }
  std::string addr = end_point.ToString();
  DCHECK(!addr.empty());

  server_url_ = base::StringPrintf("http://%s/", addr.c_str());
}

void DialHttpServer::SendDeviceDescriptionManifest(int conn_id) {
  DCHECK_EQ(message_loop_proxy_, base::MessageLoopProxy::current());
  DialSystemConfig* system_config = DialSystemConfig::GetInstance();
#if defined(__LB_SHELL__FOR_RELEASE__)
  const char* friendly_name = system_config->friendly_name();
#else
  // For non-Gold builds, append the IP to the friendly name
  // to help differentiate the devices.
  std::string friendly_name_str = system_config->friendly_name();
  IPEndPoint end_point;
  if (OK == GetLocalAddress(&end_point)) {
    friendly_name_str += " ";
    friendly_name_str += end_point.ToStringWithoutPort();
  }
  const char* friendly_name = friendly_name_str.c_str();
#endif

  std::string data = base::StringPrintf(
      kDdXmlFormat, friendly_name, system_config->manufacturer_name(),
      system_config->model_name(), system_config->model_uuid());

  std::vector<std::string> headers;
  headers.push_back(
      base::StringPrintf("Application-URL: %s", application_url().c_str()));
  http_server_->Send(conn_id, HTTP_OK, data, kXmlMimeType, headers);
}

bool DialHttpServer::DispatchToHandler(int conn_id,
                                       const HttpServerRequestInfo& info) {
  DCHECK_EQ(message_loop_proxy_, base::MessageLoopProxy::current());
  // See if DialService has a handler for this request.
  TRACE_EVENT0("net::dial", __FUNCTION__);
  std::string handler_path;
  scoped_refptr<DialServiceHandler> handler =
      dial_service_->GetHandler(info.path, &handler_path);
  if (handler == NULL) {
    return false;
  }

  handler->HandleRequest(
      handler_path, info,
      base::Bind(&DialHttpServer::OnReceivedResponse, this, conn_id));
  return true;
}

void DialHttpServer::OnReceivedResponse(
    int conn_id,
    scoped_ptr<HttpServerResponseInfo> response) {
  if (message_loop_proxy_ != base::MessageLoopProxy::current()) {
    message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&DialHttpServer::OnReceivedResponse, this,
                   conn_id, base::Passed(&response)));
    return;
  }

  DCHECK_EQ(message_loop_proxy_, base::MessageLoopProxy::current());
  TRACE_EVENT0("net::dial", __FUNCTION__);
  if (response) {
    http_server_->Send(conn_id,
                       static_cast<HttpStatusCode>(response->response_code),
                       response->body,
                       response->mime_type,
                       response->headers);
  } else {
    http_server_->Send404(conn_id);
  }
}

}  // namespace net
