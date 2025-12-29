// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/shell/browser/h5vcc_scheme_url_loader_factory.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {
// TODO - b/456482732: remove unsafe-inline.
const char kH5vccContentSecurityPolicy[] =
    "default-src 'self'; "
    "script-src 'self' 'unsafe-inline'; "
    "style-src 'self' 'unsafe-inline'; "
    "img-src 'self' data: blob:; "
    "media-src 'self' data: blob:; "
    "connect-src 'self' blob: data:;";
}  // namespace

class H5vccSchemeURLLoader : public network::mojom::URLLoader {
 public:
  H5vccSchemeURLLoader(
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const GeneratedResourceMap* resource_map_test)
      : client_(std::move(client)),
        url_(request.url),
        resource_map_test_(resource_map_test) {
    std::string key = url_.host();

    // Get the embedded header resource
    GeneratedResourceMap resource_map;
    if (resource_map_test_) {
      resource_map = *resource_map_test_;
    } else {
      LoaderEmbeddedResources::GenerateMap(resource_map);
    }

    if (resource_map.find(key) == resource_map.end()) {
      LOG(WARNING) << "URL: " << url_.spec() << ", host: " << key
                   << " not found.";
      SendResponse("Resource not found", "text/plain", net::HTTP_NOT_FOUND);
      return;
    }

    FileContents file_contents = resource_map[key];
    std::string mime_type = "application/octet-stream";

    if (base::EndsWith(key, ".html", base::CompareCase::SENSITIVE)) {
      mime_type = "text/html";
    } else if (base::EndsWith(key, ".webm", base::CompareCase::SENSITIVE)) {
      mime_type = "video/webm";
    }
    std::string content(reinterpret_cast<const char*>(file_contents.data),
                        file_contents.size);
    SendResponse(content, mime_type);
  }
  ~H5vccSchemeURLLoader() override = default;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}

 private:
  void SendResponse(const std::string& data_content,
                    const std::string& mime_type,
                    int http_status = net::HTTP_OK) {
    if (!client_) {
      LOG(ERROR) << "URLLoaderClient is not connected.";
      return;
    }

    auto response_head = network::mojom::URLResponseHead::New();
    response_head->mime_type = mime_type;
    response_head->content_length = data_content.size();

    std::string status_line =
        "HTTP/1.1 " + std::to_string(http_status) + " " +
        net::GetHttpReasonPhrase(static_cast<net::HttpStatusCode>(http_status));
    response_head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(status_line);
    response_head->headers->AddHeader(net::HttpRequestHeaders::kContentType,
                                      mime_type);
    response_head->headers->AddHeader(net::HttpRequestHeaders::kContentLength,
                                      std::to_string(data_content.size()));
    response_head->headers->AddHeader("Content-Security-Policy",
                                      kH5vccContentSecurityPolicy);
    // Range requests are not supported.
    response_head->headers->AddHeader("Accept-Ranges", "none");

    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = data_content.size();
    if (mojo::CreateDataPipe(&options, producer_handle, consumer_handle) !=
        MOJO_RESULT_OK) {
      client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    client_->OnReceiveResponse(std::move(response_head),
                               std::move(consumer_handle), std::nullopt);

    size_t bytes_written = 0;
    MojoResult result = producer_handle->WriteData(
        base::as_byte_span(data_content), MOJO_WRITE_DATA_FLAG_ALL_OR_NONE,
        bytes_written);

    if (result != MOJO_RESULT_OK || bytes_written != data_content.size()) {
      client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    network::URLLoaderCompletionStatus status(net::OK);
    status.encoded_data_length = data_content.size();
    status.encoded_body_length = data_content.size();
    client_->OnComplete(status);
  }

  mojo::Remote<network::mojom::URLLoaderClient> client_;
  GURL url_;
  const GeneratedResourceMap* resource_map_test_ = nullptr;
};

H5vccSchemeURLLoaderFactory::H5vccSchemeURLLoaderFactory() = default;

H5vccSchemeURLLoaderFactory::~H5vccSchemeURLLoaderFactory() = default;

void H5vccSchemeURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<H5vccSchemeURLLoader>(url_request, std::move(client),
                                             resource_map_test_),
      std::move(receiver));
}

void H5vccSchemeURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<H5vccSchemeURLLoaderFactory>(),
                              std::move(receiver));
}

void H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(
    const GeneratedResourceMap* resource_map_test) {
  resource_map_test_ = resource_map_test;
}

}  // namespace content
