// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/linux/shared/pre_app_recommendation_service.h"

#include <memory>
#include <string>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/linux/shared/platform_service.h"
#include "starboard/shared/starboard/application.h"

// Omit namespace 'linux' due to symbol name conflict with macro 'linux'
namespace starboard {
namespace shared {
namespace {
typedef struct PreAppRecommendationsPlatformServiceImpl
    : public PlatformServiceImpl {
  // Define additional data field.
  // variable_1, variable_2,...
  PreAppRecommendationsPlatformServiceImpl(
      void* context,
      ReceiveMessageCallback receive_callback)
      : PlatformServiceImpl(context, receive_callback) {}

  // Default constructor.
  PreAppRecommendationsPlatformServiceImpl() = default;

} PreAppRecommendationsPlatformServiceImpl;

// Use HTTP status code in response to YouTube application's method recommend
// call.
const char kSuccess[] = "200";
const char kBadRequest[] = "400";
// Methods supported
const char kGetPartnerIdMethod[] = "getPartnerId";
const char kRecommendMethod[] = "recommend";
// Operations supported
const char kUpsertOp[] = "upsert";
const char kDeleteOp[] = "delete";
// Configure partner Id
const char kPartnerId[] = "dummy_partner_id";

bool Has(const char* name) {
  // Check if platform has service name.
  return strcmp(name, kPreappRecommendationServiceName) == 0;
}

PlatformServiceImpl* Open(void* context,
                          ReceiveMessageCallback receive_callback) {
  SB_DCHECK(context);
  SB_LOG(INFO) << "Open() service created: "
               << kPreappRecommendationServiceName;

  return new PreAppRecommendationsPlatformServiceImpl(context,
                                                      receive_callback);
}

void Close(PlatformServiceImpl* service) {
  // Function Close shouldn't manually delete PlatformServiceImpl pointer,
  // because it is managed by unique_ptr in Platform Service.
  SB_LOG(INFO)
      << kPreappRecommendationServiceName
      << " Perform actions before gracefully shutting down the service";
}

std::string extractJsonValue(const std::string& jsonLikeString,
                             const std::string& key) {
  std::string result;
  std::size_t start = jsonLikeString.find("\"" + key + "\"");

  if (start != std::string::npos) {
    start = jsonLikeString.find(":", start);
    if (start != std::string::npos) {
      start = jsonLikeString.find_first_of("\"", start);
      if (start != std::string::npos) {
        ++start;  // Skip the opening quote
        std::size_t end = jsonLikeString.find("\"", start);
        if (end != std::string::npos) {
          result = jsonLikeString.substr(start, end - start);
        }
      }
    }
  }

  return result;
}

void* Send(PlatformServiceImpl* service,
           void* data,
           uint64_t length,
           uint64_t* output_length,
           bool* invalid_state) {
  SB_DCHECK(service);
  SB_DCHECK(data);
  SB_DCHECK(output_length);

  char message[length + 1];
  std::memcpy(message, data, length);
  message[length] = '\0';

  std::string response = "";

  // TODO: Replace extractJsonValue function with a robust JSON parsing
  // library for production use. The current implementation has limited
  // capabilities that nested object isn't supported.
  std::string method_name = extractJsonValue(message, "method");

  if (method_name.length() == 0) {
    SB_LOG(ERROR) << "Could not extract method value from the input JSON file:"
                  << message;
  }

  // When method_name = getPartnerId, platform returns partner Id to get
  // authenticated by YouTube app.
  if (method_name == kGetPartnerIdMethod) {
    // Populate Partner Id in the response.
    // Partner Id will be used by YouTube app to authenticate platforms and
    // retrieve specified topics for a platform.
    // auto partner_id = "\"dummmy_partner_id\"";
    response = FormatString("{\"partner_id\": \"%s\"}", kPartnerId);
  }

  // When method_name = recommend, platform processes data according to
  // operation field.
  if (method_name == kRecommendMethod) {
    std::string operation = extractJsonValue(message, "operation");
    if (operation.length() != 0) {
      // When operation = upsert, platform parses recs_response field and
      // insert/update data in local storage.
      if (operation == kUpsertOp) {
        std::string recs_response = extractJsonValue(message, "recs_response");

        if (recs_response.length() != 0) {
          // Store recommendations data in the local storage, such as local
          // database or local filesystem.
          SB_LOG(INFO) << "operation = " << operation
                       << ", parse recs_response and store data locally";
        } else {
          SB_LOG(ERROR)
              << "Could not extract recsResponse from the input JSON file:"
              << message;
        }
      } else if (operation == kDeleteOp) {
        // When operation = delete, platform delete YouTube data in local
        // storage.
        SB_LOG(INFO) << "operation = " << operation
                     << ", data in local storage is deleted";
      }
      // Populate response with 200 to indicate data is processed
      // successfully.
      response = kSuccess;
    } else {
      // Populate response with error code if data isn't processed successfully.
      response = kBadRequest;
    }
  }

  *output_length = response.length();
  auto ptr = malloc(*output_length);
  response.copy(reinterpret_cast<char*>(ptr), response.length());
  return ptr;
}

const CobaltPlatformServiceApi kGetPreappRecommendationServiceApi = {
    kPreappRecommendationServiceName,
    1,  // API version that's implemented.
    &Has,
    &Open,
    &Close,
    &Send};

}  // namespace

const void* GetPreappRecommendationServiceApi() {
  return &kGetPreappRecommendationServiceApi;
}

}  // namespace shared
}  // namespace starboard
