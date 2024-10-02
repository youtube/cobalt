// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/network_location_request.h"

#include <stdint.h>

#include <limits>
#include <set>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/device_event_log/device_event_log.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/device/geolocation/location_arbitrator.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {
namespace {

const char kNetworkLocationBaseUrl[] =
    "https://www.googleapis.com/geolocation/v1/geolocate";

const char kLocationString[] = "location";
const char kLatitudeString[] = "lat";
const char kLongitudeString[] = "lng";
const char kAccuracyString[] = "accuracy";

enum NetworkLocationRequestEvent {
  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync.
  NETWORK_LOCATION_REQUEST_EVENT_REQUEST_START = 0,
  NETWORK_LOCATION_REQUEST_EVENT_REQUEST_CANCEL = 1,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_SUCCESS = 2,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_NOT_OK = 3,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_EMPTY = 4,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_MALFORMED = 5,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_INVALID_FIX = 6,

  // NOTE: Add entries only immediately above this line.
  NETWORK_LOCATION_REQUEST_EVENT_COUNT = 7
};

void RecordUmaEvent(NetworkLocationRequestEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Geolocation.NetworkLocationRequest.Event", event,
                            NETWORK_LOCATION_REQUEST_EVENT_COUNT);
}

void RecordUmaResponseCode(int code) {
  base::UmaHistogramSparse("Geolocation.NetworkLocationRequest.ResponseCode",
                           code);
}

void RecordUmaAccessPoints(int count) {
  const int min = 1;
  const int max = 20;
  const int buckets = 21;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Geolocation.NetworkLocationRequest.AccessPoints",
                              count, min, max, buckets);
}

void RecordUmaRequestInterval(base::TimeDelta time_delta) {
  const int kMin = 1;
  const int kMax = 11;
  const int kBuckets = 10;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Geolocation.NetworkLocationRequest.RequestInterval",
      time_delta.InMinutes(), kMin, kMax, kBuckets);
}

// Local functions

// Returns a URL for a request to the Google Maps geolocation API. If the
// specified |api_key| is not empty, it is escaped and included as a query
// string parameter.
GURL FormRequestURL(const std::string& api_key);

void FormUploadData(const WifiData& wifi_data,
                    const base::Time& wifi_timestamp,
                    std::string* upload_data);

// Attempts to extract a position from the response. Detects and indicates
// various failure cases.
mojom::GeopositionResultPtr CreateGeopositionResultFromResponse(
    int net_error,
    int status_code,
    std::unique_ptr<std::string> response_body,
    const base::Time& wifi_timestamp,
    const GURL& server_url);

// Parses the server response body. Returns true if parsing was successful.
// Returns a `mojom::GeopositionPtr` or `nullptr` if no valid fix was received.
mojom::GeopositionPtr ParseServerResponse(const std::string& response_body,
                                          const base::Time& wifi_timestamp);
void AddWifiData(const WifiData& wifi_data,
                 int age_milliseconds,
                 base::Value::Dict& request);
}  // namespace

NetworkLocationRequest::NetworkLocationRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& api_key,
    LocationResponseCallback callback)
    : url_loader_factory_(std::move(url_loader_factory)),
      api_key_(api_key),
      location_response_callback_(std::move(callback)) {}

NetworkLocationRequest::~NetworkLocationRequest() = default;

bool NetworkLocationRequest::MakeRequest(
    const WifiData& wifi_data,
    const base::Time& wifi_timestamp,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  GEOLOCATION_LOG(DEBUG)
      << "Sending a network location request: Number of Wi-Fi APs="
      << wifi_data.access_point_data.size();
  RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_REQUEST_START);
  RecordUmaAccessPoints(wifi_data.access_point_data.size());
  if (url_loader_) {
    GEOLOCATION_LOG(DEBUG) << "Cancelling pending network location request";
    DVLOG(1) << "NetworkLocationRequest : Cancelling pending request";
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_REQUEST_CANCEL);
    url_loader_.reset();
  }
  wifi_data_ = wifi_data;

  if (!wifi_timestamp_.is_null()) {
    RecordUmaRequestInterval(wifi_timestamp - wifi_timestamp_);
  }
  wifi_timestamp_ = wifi_timestamp;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::CompleteNetworkTrafficAnnotation("network_location_request",
                                            partial_traffic_annotation,
                                            R"(
        semantics {
          description:
            "Obtains geo position based on current IP address and local "
            "network information including Wi-Fi access points (even if you’re "
            "not using them)."
          trigger:
            "Location requests are sent when the page requests them or new "
            "IP address is available."
          data: "Wi-Fi data, IP address."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
      })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
  resource_request->url = FormRequestURL(api_key_);
  DCHECK(resource_request->url.is_valid());
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->SetAllowHttpErrorResults(true);

  std::string upload_data;
  FormUploadData(wifi_data, wifi_timestamp, &upload_data);
  url_loader_->AttachStringForUpload(upload_data, "application/json");

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&NetworkLocationRequest::OnRequestComplete,
                     base::Unretained(this)),
      1024 * 1024 /* 1 MiB */);
  return true;
}

void NetworkLocationRequest::OnRequestComplete(
    std::unique_ptr<std::string> data) {
  int net_error = url_loader_->NetError();

  int response_code = 0;
  if (url_loader_->ResponseInfo())
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  RecordUmaResponseCode(response_code);
  GEOLOCATION_LOG(DEBUG) << "Got network location response: response_code="
                         << response_code;

  auto result = CreateGeopositionResultFromResponse(
      net_error, response_code, std::move(data), wifi_timestamp_,
      url_loader_->GetFinalURL());

  bool server_error =
      net_error != net::OK || (response_code >= 500 && response_code < 600);

  url_loader_.reset();

  DVLOG(1) << "NetworkLocationRequest::OnURLFetchComplete() : run callback.";
  location_response_callback_.Run(std::move(result), server_error, wifi_data_);
}

// Local functions.
namespace {

struct AccessPointLess {
  bool operator()(const AccessPointData* ap1,
                  const AccessPointData* ap2) const {
    return ap2->radio_signal_strength < ap1->radio_signal_strength;
  }
};

GURL FormRequestURL(const std::string& api_key) {
  GURL url(kNetworkLocationBaseUrl);
  if (!api_key.empty()) {
    std::string query(url.query());
    if (!query.empty())
      query += "&";
    query += "key=" + base::EscapeQueryParamValue(api_key, true);
    GURL::Replacements replacements;
    replacements.SetQueryStr(query);
    return url.ReplaceComponents(replacements);
  }
  return url;
}

void FormUploadData(const WifiData& wifi_data,
                    const base::Time& wifi_timestamp,
                    std::string* upload_data) {
  int age = std::numeric_limits<int32_t>::min();  // Invalid so AddInteger()
                                                  // will ignore.
  if (!wifi_timestamp.is_null()) {
    // Convert absolute timestamps into a relative age.
    int64_t delta_ms = (base::Time::Now() - wifi_timestamp).InMilliseconds();
    if (delta_ms >= 0 && delta_ms < std::numeric_limits<int32_t>::max())
      age = static_cast<int>(delta_ms);
  }

  base::Value::Dict request;
  AddWifiData(wifi_data, age, request);
  base::JSONWriter::Write(request, upload_data);
}

void AddString(const std::string& property_name,
               const std::string& value,
               base::Value::Dict& dict) {
  if (!value.empty())
    dict.Set(property_name, value);
}

void AddInteger(const std::string& property_name,
                int value,
                base::Value::Dict& dict) {
  if (value != std::numeric_limits<int32_t>::min())
    dict.Set(property_name, value);
}

void AddWifiData(const WifiData& wifi_data,
                 int age_milliseconds,
                 base::Value::Dict& request) {
  if (wifi_data.access_point_data.empty())
    return;

  typedef std::multiset<const AccessPointData*, AccessPointLess> AccessPointSet;
  AccessPointSet access_points_by_signal_strength;

  for (const auto& ap_data : wifi_data.access_point_data)
    access_points_by_signal_strength.insert(&ap_data);

  base::Value::List wifi_access_point_list;
  for (auto* ap_data : access_points_by_signal_strength) {
    base::Value::Dict wifi_dict;
    auto macAddress = base::UTF16ToUTF8(ap_data->mac_address);
    if (macAddress.empty())
      continue;
    AddString("macAddress", macAddress, wifi_dict);
    AddInteger("signalStrength", ap_data->radio_signal_strength, wifi_dict);
    AddInteger("age", age_milliseconds, wifi_dict);
    AddInteger("channel", ap_data->channel, wifi_dict);
    AddInteger("signalToNoiseRatio", ap_data->signal_to_noise, wifi_dict);
    wifi_access_point_list.Append(std::move(wifi_dict));
  }
  if (!wifi_access_point_list.empty())
    request.Set("wifiAccessPoints", std::move(wifi_access_point_list));
}

mojom::GeopositionResultPtr CreateGeopositionErrorResult(
    const GURL& server_url,
    const std::string& error_message,
    const std::string& error_technical) {
  auto error = mojom::GeopositionError::New();
  error->error_code = mojom::GeopositionErrorCode::kPositionUnavailable;
  error->error_message = error_message;
  VLOG(1) << "NetworkLocationRequest::CreateGeopositionErrorResult() : "
          << error->error_message;
  if (!error_technical.empty()) {
    error->error_technical = "Network location provider at '";
    error->error_technical += server_url.DeprecatedGetOriginAsURL().spec();
    error->error_technical += "' : ";
    error->error_technical += error_technical;
    error->error_technical += ".";
    VLOG(1) << "NetworkLocationRequest::CreateGeopositionErrorResult() : "
            << error->error_technical;
  }
  return mojom::GeopositionResult::NewError(std::move(error));
}

mojom::GeopositionResultPtr CreateGeopositionResultFromResponse(
    int net_error,
    int status_code,
    std::unique_ptr<std::string> response_body,
    const base::Time& wifi_timestamp,
    const GURL& server_url) {
  // HttpPost can fail for a number of reasons. Most likely this is because
  // we're offline, or there was no response.
  if (net_error != net::OK) {
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_EMPTY);
    return CreateGeopositionErrorResult(
        server_url,
        "Network error. Check "
        "DevTools console for more information.",
        net::ErrorToShortString(net_error));
  }

  if (status_code != 200) {  // HTTP OK.
    std::string message = "Returned error code ";
    message += base::NumberToString(status_code);
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_NOT_OK);
    return CreateGeopositionErrorResult(
        server_url,
        "Failed to query location from network service. Check "
        "the DevTools console for more information.",
        message);
  }

  // We use the timestamp from the wifi data that was used to generate
  // this position fix.
  DCHECK(response_body);
  mojom::GeopositionPtr position =
      ParseServerResponse(*response_body, wifi_timestamp);
  if (!position) {
    // We failed to parse the response.
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_MALFORMED);
    return CreateGeopositionErrorResult(server_url, "Response was malformed",
                                        /*error_technical=*/"");
  }

  // The response was successfully parsed, but it may not be a valid
  // position fix.
  if (!ValidateGeoposition(*position)) {
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_INVALID_FIX);
    return CreateGeopositionErrorResult(server_url,
                                        "Did not provide a good position fix",
                                        /*error_technical=*/"");
  }

  RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_SUCCESS);
  return mojom::GeopositionResult::NewPosition(std::move(position));
}

mojom::GeopositionPtr ParseServerResponse(const std::string& response_body,
                                          const base::Time& wifi_timestamp) {
  DCHECK(!wifi_timestamp.is_null());

  if (response_body.empty()) {
    LOG(WARNING) << "ParseServerResponse() : Response was empty.";
    return nullptr;
  }
  DVLOG(1) << "ParseServerResponse() : Parsing response " << response_body;

  // Parse the response, ignoring comments.
  auto response_result =
      base::JSONReader::ReadAndReturnValueWithError(response_body);
  if (!response_result.has_value()) {
    LOG(WARNING) << "ParseServerResponse() : JSONReader failed : "
                 << response_result.error().message;
    return nullptr;
  }
  base::Value response_value = std::move(*response_result);

  const base::Value::Dict* response_object = response_value.GetIfDict();
  if (!response_object) {
    VLOG(1) << "ParseServerResponse() : Unexpected response type "
            << response_value.type();
    return nullptr;
  }

  // Get the location
  const base::Value* location_value = response_object->Find(kLocationString);
  if (!location_value) {
    VLOG(1) << "ParseServerResponse() : Missing location attribute.";
    // GLS returns a response with no location property to represent
    // no fix available; return an invalid geoposition to indicate successful
    // parse.
    // TODO(mattreynolds): Return an appropriate error instead of a
    // default-initialized Geoposition.
    return mojom::Geoposition::New();
  }

  const base::Value::Dict* location_object = location_value->GetIfDict();
  if (!location_object) {
    if (!location_value->is_none()) {
      VLOG(1) << "ParseServerResponse() : Unexpected location type "
              << location_value->type();
      // If the network provider was unable to provide a position fix, it should
      // return a HTTP 200, with "location" : null. Otherwise it's an error.
      return nullptr;
    }
    // Successfully parsed response containing no fix.
    // TODO(mattreynolds): Return an appropriate error instead of a
    // default-initialized Geoposition.
    return mojom::Geoposition::New();
  }

  // latitude and longitude fields are always required.
  absl::optional<double> latitude =
      location_object->FindDouble(kLatitudeString);
  absl::optional<double> longitude =
      location_object->FindDouble(kLongitudeString);
  if (!latitude || !longitude) {
    VLOG(1) << "ParseServerResponse() : location lacks lat and/or long.";
    return nullptr;
  }
  // All error paths covered.
  auto position = mojom::Geoposition::New();
  position->latitude = *latitude;
  position->longitude = *longitude;
  position->timestamp = wifi_timestamp;

  // Other fields are optional.
  absl::optional<double> accuracy =
      response_object->FindDouble(kAccuracyString);
  if (accuracy) {
    position->accuracy = *accuracy;
  }

  return position;
}

}  // namespace

}  // namespace device
