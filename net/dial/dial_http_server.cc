// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dial_http_server.h"

#include "base/stringprintf.h"
#include "base/string_util.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dial/dial_system_config.h"
#include "net/server/http_server_request_info.h"

#if defined(__LB_SHELL__)
#include "lb_platform.h"
#endif

namespace net {

#if defined(__LB_PS3__)
  const char* DialSystemConfig::kDefaultFriendlyName = "Sony PS3";
  const char* DialSystemConfig::kDefaultManufacturerName = "Sony";
  const char* DialSystemConfig::kDefaultModelName = "PS3";
  const char* DialSystemConfig::kDefaultModelUuid = "";
#elif defined(__LB_LINUX__)
  const char* DialSystemConfig::kDefaultFriendlyName = "Linux Dummy";
  const char* DialSystemConfig::kDefaultManufacturerName = "GNU";
  const char* DialSystemConfig::kDefaultModelName = "Linux";
  const char* DialSystemConfig::kDefaultModelUuid = "";
#endif

const static char* kXmlMimeType = "text/xml; charset=\"utf-8\"";

const static char* kDdXmlFormat =
    "<?xml version=\"1.0\"?>"
    "<root"
    "  xmlns=\"urn:schemas-upnp-org:device-1-0\""
    "  xmlns:r=\"urn:restful-tv-org:schemas:upnp-dd\">"
    "  <specVersion>"
    "    <major>1</major>"
    "    <minor>0</minor>"
    "  </specVersion>"
    "  <device>"
    "    <deviceType>urn:schemas-upnp-org:device:tvdevice:1</deviceType>"
    "    <friendlyName>%s</friendlyName>"
    "    <manufacturer>%s</manufacturer>"
    "    <modelName>%s</modelName>"
    "    <UDN>uuid:%s</UDN>"
    "  </device>"
    "</root>";

// TODO: Make this application agnostic.
const static char* kYouTubeAppInfo =
    "<?xml version=\"1.0\" enconding=\"UTF-8\"?>"
    "<service xmlns=\"urn:dial-multiscreen-org:schemas:dial\">"
    "  <name>YouTube</name>"
    "  <options allowStop=\"false\"/>"
    "  <state>running</state>"
    "</service>";

DialHttpServer::DialHttpServer()
    : factory_(new TCPListenSocketFactory("0.0.0.0", 0)) {

}

DialHttpServer::~DialHttpServer() {
}

bool DialHttpServer::Start() {
  if (http_server_ != NULL) {
    // already running.
    return true;
  }
  DCHECK(server_url_.empty());

  http_server_ = new HttpServer(*factory_, this);
  if (http_server_ == NULL) {
    return false;
  }

  ConfigureApplicationUrl();
  DCHECK(!server_url_.empty());
  return true;
}

bool DialHttpServer::Stop() {
  if (http_server_ == NULL) {
    // not running
    return true;
  }
  DCHECK(!server_url_.empty());

  http_server_ = NULL;
  server_url_.clear();
  return true;
}

int DialHttpServer::GetLocalAddress(IPEndPoint* addr) {
  if (http_server_ == NULL) {
    return ERR_FAILED;
  }

  // get the port information
  int ret = http_server_->GetLocalAddress(addr);

  // Now get the IPAddress of the network card.
  SockaddrStorage sock_addr;
  ret |= LB::Platform::GetLocalIpAddress(&sock_addr.addr_storage.sin_addr);
  sock_addr.addr_storage.sin_family = AF_INET;
  sock_addr.addr_storage.sin_port = htons(addr->port());

  return (ret == 0 && addr->FromSockAddr(sock_addr.addr, sock_addr.addr_len))
      ? OK : ERR_FAILED;
}

void DialHttpServer::OnHttpRequest(int conn_id,
                                   const HttpServerRequestInfo& info) {
  DLOG(INFO) << "Http Request: "
             << info.method << " " << info.path << " HTTP/1.1";
  if (info.method == "GET" && LowerCaseEqualsASCII(info.path, "/dd.xml")) {
    SendDeviceDescriptionManifest(conn_id);
  } else if (info.path == "/apps/YouTube" && info.method == "GET") {
    SendApplicationInformationResponse(conn_id);
  } else {
    bool callback_registered = false;
    if (info.path.find("/apps/YouTube") == 0)
      callback_registered = CallbackJsHttpRequest(conn_id, info);

    if (!callback_registered)
      http_server_->Send404(conn_id);
  }
}

void DialHttpServer::OnClose(int conn_id) {
}

void DialHttpServer::ConfigureApplicationUrl() {
  IPEndPoint end_point;
  if (OK != GetLocalAddress(&end_point)) {
    LOG(ERROR) << "Could not get the local URL!";
    return;
  }
  std::string addr = end_point.ToString();
  DCHECK(!addr.empty());

  server_url_ = base::StringPrintf("http://%s/", addr.c_str());
  LOG(INFO) << "DIAL HTTP server URL: " << server_url_;
}

void DialHttpServer::SendDeviceDescriptionManifest(int conn_id) {
  DialSystemConfig* system_config = DialSystemConfig::GetInstance();
  std::string data = base::StringPrintf(kDdXmlFormat,
      system_config->friendly_name_,
      system_config->manufacturer_name_,
      system_config->model_name_,
      system_config->model_uuid_);

  std::vector<std::string> headers;
  headers.push_back(
      base::StringPrintf("Application-URL: %s", application_url().c_str()));
  http_server_->Send(conn_id, HTTP_OK, data, kXmlMimeType, headers);
}

void DialHttpServer::SendApplicationInformationResponse(int conn_id) {
  http_server_->Send200(conn_id, std::string(kYouTubeAppInfo), kXmlMimeType);
}

} // namespace net

