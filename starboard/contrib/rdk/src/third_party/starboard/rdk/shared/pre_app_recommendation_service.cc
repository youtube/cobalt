// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/starboard/rdk/shared/pre_app_recommendation_service.h"

#include <strings.h>
#include <sys/stat.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/extension/platform_service.h"
#include "starboard/shared/starboard/application.h"
#include "third_party/jsoncpp/source/include/json/reader.h"
#include "third_party/jsoncpp/source/include/json/value.h"
#include "third_party/jsoncpp/source/include/json/writer.h"
#include "third_party/starboard/rdk/shared/log_override.h"
#include "third_party/starboard/rdk/shared/platform_service.h"

namespace starboard {
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

constexpr uint64_t kMaxMessageLength =
    kCobaltExtensionPlatformServiceMaxMessageLength;

// Use HTTP status code in response to YouTube application's method recommend
// call.
const char kSuccess[] = "{\"response_code\": \"200\"}";
const char kBadRequest[] = "{\"response_code\": \"400\"}";
// Methods supported
const char kGetPartnerIdMethod[] = "getPartnerId";
const char kRecommendMethod[] = "recommend";
// Operations supported
const char kUpsertOp[] = "upsert";
const char kDeleteOp[] = "delete";
// Configure partner Id and storage paths
const char kPartnerId[] = "rdk-demo";
const char kPreAppStorageDir[] = "/opt/persistent/rdkservices/Cobalt";
const char kPartnerIdFilePath[] =
    "/opt/persistent/rdkservices/Cobalt/youtube_partner_id";
const char kTargetRecommendationPath[] =
    "/opt/persistent/rdkservices/Cobalt/youtube_pre_app_recs.json";

bool EnsureDirectoryExists(const std::string& dir_path) {
  struct stat st;
  if (stat(dir_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
    return true;
  }
  std::string current;
  for (size_t i = 0; i < dir_path.size(); ++i) {
    current.push_back(dir_path[i]);
    if (dir_path[i] == '/' && current.size() > 1) {
      mkdir(current.c_str(), 0755);
    }
  }
  if (mkdir(dir_path.c_str(), 0755) != 0 && errno != EEXIST) {
    SB_LOG(ERROR) << "Failed to create directory: " << dir_path;
    return false;
  }
  return true;
}

std::string GetPartnerId() {
  FILE* fp = fopen(kPartnerIdFilePath, "r");
  if (fp) {
    char buf[128] = {0};
    if (fgets(buf, sizeof(buf), fp)) {
      char* p = buf + strlen(buf) - 1;
      while (p >= buf &&
             (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t')) {
        *p-- = '\0';
      }
      if (buf[0] != '\0') {
        fclose(fp);
        return buf;
      }
    }
    fclose(fp);
  }
  return kPartnerId;
}

bool Has(const char* name) {
  // Check if platform has service name.
  return strcasecmp(name, kPreappRecommendationServiceName) == 0;
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
  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  Json::Value root;
  std::string errs;
  if (reader->parse(jsonLikeString.data(),
                    jsonLikeString.data() + jsonLikeString.size(), &root,
                    &errs) &&
      root.isObject() && root.isMember(key)) {
    const Json::Value& val = root[key];
    if (val.isString()) {
      return val.asString();
    }
    if (val.isObject() || val.isArray()) {
      Json::StreamWriterBuilder writer_builder;
      writer_builder["indentation"] = "  ";
      return Json::writeString(writer_builder, val);
    }
  }
  return "";
}

// Copies |response| into a buffer allocated with malloc(), as the caller of
// Send() in cobalt/browser/h5vcc_platform_service/platform_service_impl.cc
// frees it. Returns nullptr if the allocation fails.
void* AllocateResponse(const std::string& response, uint64_t* output_length) {
  *output_length = response.length();
  void* ptr = malloc(*output_length);
  if (ptr == nullptr) {
    SB_LOG(ERROR) << "Send() failed to allocate " << *output_length
                  << " bytes for the response.";
    return nullptr;
  }
  response.copy(reinterpret_cast<char*>(ptr), response.length());
  return ptr;
}

void* Send(PlatformServiceImpl* service,
           const void* data,
           uint64_t length,
           uint64_t* output_length,
           bool* invalid_state) {
  SB_DCHECK(service);
  SB_DCHECK(output_length);

  std::string response = kBadRequest;

  if (data == nullptr || length > kMaxMessageLength) {
    SB_LOG(ERROR) << "Send() rejecting a message of " << length
                  << " bytes, the limit is " << kMaxMessageLength << " bytes.";
    return AllocateResponse(response, output_length);
  }

  const std::string message(static_cast<const char*>(data), length);
  std::string method_name = extractJsonValue(message, "command");
  if (method_name.empty()) {
    method_name = extractJsonValue(message, "method");
  }

  if (method_name.empty()) {
    SB_LOG(ERROR) << "Could not extract method value from the input JSON file: "
                  << message;
    return AllocateResponse(response, output_length);
  }

  // When method_name = getPartnerId, platform returns partner Id to get
  // authenticated by YouTube app.
  if (method_name == kGetPartnerIdMethod) {
    std::string partner_id = GetPartnerId();
    response =
        FormatString("{\"partner_id\": \"%s\", \"response_code\": \"200\"}",
                     partner_id.c_str());
    SB_LOG(INFO) << "getPartnerId -> " << response;
  } else if (method_name == kRecommendMethod) {
    std::string operation = extractJsonValue(message, "operation");
    if (operation == kUpsertOp) {
      std::string recs_response = extractJsonValue(message, "recs_response");
      if (!recs_response.empty()) {
        if (EnsureDirectoryExists(kPreAppStorageDir)) {
          FILE* fp = fopen(kTargetRecommendationPath, "w");
          if (fp) {
            size_t written =
                fwrite(recs_response.c_str(), 1, recs_response.size(), fp);
            fflush(fp);
            fclose(fp);
            if (written == recs_response.size()) {
              SB_LOG(INFO) << "Successfully stored recommendations locally.";
              response = kSuccess;
            } else {
              SB_LOG(ERROR) << "Failed to write complete recommendations data.";
            }
          } else {
            SB_LOG(ERROR) << "Failed to open " << kTargetRecommendationPath
                          << " for writing.";
          }
        }
      } else {
        SB_LOG(ERROR)
            << "Could not extract recs_response from the input JSON file: "
            << message;
      }
    } else if (operation == kDeleteOp) {
      std::remove(kTargetRecommendationPath);
      SB_LOG(INFO) << "operation = " << operation
                   << ", data in local storage is deleted";
      response = kSuccess;
    } else {
      SB_LOG(ERROR) << "Unsupported or missing operation: " << operation;
    }
  } else {
    SB_LOG(ERROR) << "Unsupported method: " << method_name;
  }

  return AllocateResponse(response, output_length);
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

}  // namespace starboard
