// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/fake_dmserver.h"

#include <vector>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/request_handler_for_policy.h"
#include "components/policy/test_support/test_server_helpers.h"

namespace fakedms {

namespace {

constexpr char kPolicyTypeKey[] = "policy_type";
constexpr char kEntityIdKey[] = "entity_id";
constexpr char kPolicyValueKey[] = "value";
constexpr char kDeviceIdKey[] = "device_id";
constexpr char kDeviceTokenKey[] = "device_token";
constexpr char kMachineNameKey[] = "machine_name";
constexpr char kUsernameKey[] = "username";
constexpr char kStateKeysKey[] = "state_keys";
constexpr char kAllowedPolicyTypesKey[] = "allowed_policy_types";
constexpr char kPoliciesKey[] = "policies";
constexpr char kExternalPoliciesKey[] = "external_policies";
constexpr char kManagedUsersKey[] = "managed_users";
constexpr char kDeviceAffiliationIdsKey[] = "device_affiliation_ids";
constexpr char kUserAffiliationIdsKey[] = "user_affiliation_ids";
constexpr char kDirectoryApiIdKey[] = "directory_api_id";
constexpr char kRequestErrorsKey[] = "request_errors";
constexpr char kRobotApiAuthCodeKey[] = "robot_api_auth_code";
constexpr char kAllowSetDeviceAttributesKey[] = "allow_set_device_attributes";
constexpr char kUseUniversalSigningKeysKey[] = "use_universal_signing_keys";
constexpr char kInitialEnrollmentStateKey[] = "initial_enrollment_state";
constexpr char kManagementDomainKey[] = "management_domain";
constexpr char kInitialEnrollmentModeKey[] = "initial_enrollment_mode";
constexpr char kCurrentKeyIndexKey[] = "current_key_index";
constexpr char kPolicyUserKey[] = "policy_user";

constexpr char kDefaultPolicyBlobFilename[] = "policy.json";
constexpr char kDefaultClientStateFilename[] = "state.json";
constexpr int kDefaultMinLogLevel = logging::LOGGING_INFO;
constexpr bool kDefaultLogToConsole = false;

constexpr char kPolicyBlobPathSwitch[] = "policy-blob-path";
constexpr char kClientStatePathSwitch[] = "client-state-path";
constexpr char kLogPathSwitch[] = "log-path";
constexpr char kStartupPipeSwitch[] = "startup-pipe";
constexpr char kMinLogLevelSwitch[] = "min-log-level";
constexpr char kLogToConsoleSwitch[] = "log-to-console";

}  // namespace

void InitLogging(const absl::optional<std::string>& log_path,
                 bool log_to_console,
                 int min_log_level) {
  logging::LoggingSettings settings;
  if (log_path.has_value()) {
    settings.log_file_path = log_path.value().c_str();
    settings.logging_dest = logging::LOG_TO_FILE;
  } else {
    settings.logging_dest = logging::LOG_TO_STDERR;
  }
  // If log_to_console exists then log to everything.
  if (log_to_console) {
    settings.logging_dest |=
        logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  }
  logging::SetMinLogLevel(min_log_level);
  logging::InitLogging(settings);
}

void ParseFlags(const base::CommandLine& command_line,
                std::string& policy_blob_path,
                std::string& client_state_path,
                absl::optional<std::string>& log_path,
                base::ScopedFD& startup_pipe,
                bool& log_to_console,
                int& min_log_level) {
  policy_blob_path = kDefaultPolicyBlobFilename;
  client_state_path = kDefaultClientStateFilename;
  log_to_console = kDefaultLogToConsole;
  min_log_level = kDefaultMinLogLevel;

  if (command_line.HasSwitch(kPolicyBlobPathSwitch)) {
    policy_blob_path = command_line.GetSwitchValueASCII(kPolicyBlobPathSwitch);
  }

  if (command_line.HasSwitch(kLogPathSwitch)) {
    log_path = command_line.GetSwitchValueASCII(kLogPathSwitch);
  }

  if (command_line.HasSwitch(kClientStatePathSwitch)) {
    client_state_path =
        command_line.GetSwitchValueASCII(kClientStatePathSwitch);
  }

  if (command_line.HasSwitch(kStartupPipeSwitch)) {
    std::string pipe_str = command_line.GetSwitchValueASCII(kStartupPipeSwitch);
    int pipe_val;
    CHECK(base::StringToInt(pipe_str, &pipe_val))
        << "Expected an int value for --startup-pipe switch, but got: "
        << pipe_str;
    startup_pipe = base::ScopedFD(pipe_val);
  }

  if (command_line.HasSwitch(kMinLogLevelSwitch)) {
    std::string log_str = command_line.GetSwitchValueASCII(kMinLogLevelSwitch);
    CHECK(base::StringToInt(log_str, &min_log_level))
        << "Expected an int value for --min-log-level switch, but got: "
        << log_str;
  }

  if (command_line.HasSwitch(kLogToConsoleSwitch)) {
    log_to_console = true;
  }
}

FakeDMServer::FakeDMServer(const std::string& policy_blob_path,
                           const std::string& client_state_path,
                           base::OnceClosure shutdown_cb)
    : policy_blob_path_(policy_blob_path),
      client_state_path_(client_state_path),
      shutdown_cb_(std::move(shutdown_cb)) {}

FakeDMServer::~FakeDMServer() = default;

bool FakeDMServer::Start() {
  LOG(INFO) << "Starting the FakeDMServer with args policy_blob_path="
            << policy_blob_path_ << " client_state_path=" << client_state_path_;

  if (!policy::EmbeddedPolicyTestServer::Start()) {
    LOG(ERROR) << "Failed to start the EmbeddedPolicyTestServer";
    return false;
  }
  LOG(INFO) << "Server started running on URL: "
            << EmbeddedPolicyTestServer::GetServiceURL();
  return true;
}

bool FakeDMServer::WriteURLToPipe(base::ScopedFD&& startup_pipe) {
  GURL server_url = EmbeddedPolicyTestServer::GetServiceURL();
  std::string server_data =
      base::StringPrintf("{\"host\": \"%s\", \"port\": %s}",
                         server_url.host().c_str(), server_url.port().c_str());

  base::File pipe_writer(startup_pipe.release());
  if (!pipe_writer.WriteAtCurrentPosAndCheck(
          base::as_bytes(base::make_span(server_data)))) {
    LOG(ERROR) << "Failed to write the server url data to the pipe, data: "
               << server_data;
    return false;
  }
  return true;
}

std::unique_ptr<net::test_server::HttpResponse> FakeDMServer::HandleRequest(
    const net::test_server::HttpRequest& request) {
  GURL url = request.GetURL();

  if (url.path() == "/test/exit") {
    LOG(INFO) << "Stopping the FakeDMServer";
    CHECK(shutdown_cb_);
    std::move(shutdown_cb_).Run();
    return policy::CreateHttpResponse(net::HTTP_OK, "Policy Server exited.");
  }

  if (url.path() == "/test/ping")
    return policy::CreateHttpResponse(net::HTTP_OK, "Pong.");

  EmbeddedPolicyTestServer::ResetServerState();

  if (!ReadPolicyBlobFile()) {
    return policy::CreateHttpResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                                      "Failed to read policy blob file.");
  }

  if (!ReadClientStateFile()) {
    return policy::CreateHttpResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                                      "Failed to read client state file.");
  }
  auto resp = policy::EmbeddedPolicyTestServer::HandleRequest(request);
  if (!WriteClientStateFile()) {
    return policy::CreateHttpResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                                      "Failed to write client state file.");
  }
  return resp;
}

bool FakeDMServer::SetPolicyPayload(const std::string* policy_type,
                                    const std::string* entity_id,
                                    const std::string* serialized_proto) {
  if (!policy_type || !serialized_proto) {
    LOG(ERROR) << "Couldn't find the policy type or value fields";
    return false;
  }
  std::string decoded_proto;
  if (!base::Base64Decode(*serialized_proto, &decoded_proto)) {
    LOG(ERROR) << "Unable to base64 decode validation value from "
               << *serialized_proto;
    return false;
  }
  if (entity_id) {
    policy_storage()->SetPolicyPayload(*policy_type, *entity_id, decoded_proto);
  } else {
    policy_storage()->SetPolicyPayload(*policy_type, decoded_proto);
  }
  return true;
}

bool FakeDMServer::SetExternalPolicyPayload(
    const std::string* policy_type,
    const std::string* entity_id,
    const std::string* serialized_raw_policy) {
  if (!policy_type || !entity_id || !serialized_raw_policy) {
    LOG(ERROR) << "Couldn't find the policy type or entity id or value fields";
    return false;
  }
  std::string decoded_raw_policy;
  if (!base::Base64Decode(*serialized_raw_policy, &decoded_raw_policy)) {
    LOG(ERROR) << "Unable to base64 decode validation value from "
               << *serialized_raw_policy;
    return false;
  }
  EmbeddedPolicyTestServer::UpdateExternalPolicy(*policy_type, *entity_id,
                                                 decoded_raw_policy);
  return true;
}

bool FakeDMServer::ReadPolicyBlobFile() {
  base::FilePath policy_blob_file(policy_blob_path_);
  if (!base::PathExists(policy_blob_file)) {
    LOG(INFO) << "Policy blob file doesn't exist yet.";
    return true;
  }
  JSONFileValueDeserializer deserializer(policy_blob_file);
  int error_code = 0;
  std::string error_msg;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_msg);
  if (!value) {
    LOG(ERROR) << "Failed to read the policy blob file "
               << policy_blob_file.value() << ": " << error_msg;
    return false;
  }
  LOG(INFO) << "Deserialized value of the policy blob: " << *value;
  if (!value->is_dict()) {
    LOG(ERROR) << "Policy blob isn't a dict";
    return false;
  }
  base::Value::Dict& dict = value->GetDict();

  std::string* policy_user = dict.FindString(kPolicyUserKey);
  if (policy_user) {
    LOG(INFO) << "Adding " << *policy_user << " as a policy user";
    policy_storage()->set_policy_user(*policy_user);
  } else {
    LOG(INFO) << "The policy_user key isn't found and the default policy "
                 "user "
              << policy::kDefaultUsername << " will be used";
  }

  base::Value::List* managed_users = dict.FindList(kManagedUsersKey);
  if (managed_users) {
    for (const base::Value& managed_user : *managed_users) {
      const std::string* managed_val = managed_user.GetIfString();
      if (managed_val) {
        LOG(INFO) << "Adding " << *managed_val << " as a managed user";
        policy_storage()->add_managed_user(*managed_val);
      }
    }
  }

  base::Value::List* device_affiliation_ids =
      dict.FindList(kDeviceAffiliationIdsKey);
  if (device_affiliation_ids) {
    for (const base::Value& device_affiliation_id : *device_affiliation_ids) {
      const std::string* device_affiliation_id_val =
          device_affiliation_id.GetIfString();
      if (device_affiliation_id_val) {
        LOG(INFO) << "Adding " << *device_affiliation_id_val
                  << " as a device affiliation id";
        policy_storage()->add_device_affiliation_id(*device_affiliation_id_val);
      }
    }
  }

  base::Value::List* user_affiliation_ids =
      dict.FindList(kUserAffiliationIdsKey);
  if (user_affiliation_ids) {
    for (const base::Value& user_affiliation_id : *user_affiliation_ids) {
      const std::string* user_affiliation_id_val =
          user_affiliation_id.GetIfString();
      if (user_affiliation_id_val) {
        LOG(INFO) << "Adding " << *user_affiliation_id_val
                  << " as a user affiliation id";
        policy_storage()->add_user_affiliation_id(*user_affiliation_id_val);
      }
    }
  }

  std::string* directory_api_id = dict.FindString(kDirectoryApiIdKey);
  if (directory_api_id) {
    LOG(INFO) << "Adding " << *directory_api_id << " as a directory API ID";
    policy_storage()->set_directory_api_id(*directory_api_id);
  }

  if (dict.contains(kAllowSetDeviceAttributesKey)) {
    absl::optional<bool> allow_set_device_attributes =
        dict.FindBool(kAllowSetDeviceAttributesKey);
    if (!allow_set_device_attributes.has_value()) {
      base::Value* v = dict.Find(kAllowSetDeviceAttributesKey);
      LOG(ERROR)
          << "The allow_set_device_attributes key isn't a bool, found type "
          << v->type() << ", found value " << *v;
      return false;
    }
    policy_storage()->set_allow_set_device_attributes(
        allow_set_device_attributes.value());
  }

  base::Value* use_universal_signing_keys =
      dict.Find(kUseUniversalSigningKeysKey);
  if (use_universal_signing_keys) {
    if (!use_universal_signing_keys->is_bool()) {
      LOG(ERROR)
          << "The use_universal_signing_keys key isn't a bool, found type "
          << use_universal_signing_keys->type() << ", found value "
          << *use_universal_signing_keys;
      return false;
    }
    if (use_universal_signing_keys->GetBool()) {
      policy_storage()->signature_provider()->SetUniversalSigningKeys();
    }
  }

  std::string* robot_api_auth_code = dict.FindString(kRobotApiAuthCodeKey);
  if (robot_api_auth_code) {
    LOG(INFO) << "Adding " << *robot_api_auth_code
              << " as a robot api auth code";
    policy_storage()->set_robot_api_auth_code(*robot_api_auth_code);
  }

  base::Value::Dict* request_errors = dict.FindDict(kRequestErrorsKey);
  if (request_errors) {
    for (auto request_error : *request_errors) {
      absl::optional<int> net_error_code = request_error.second.GetIfInt();
      if (!net_error_code.has_value()) {
        LOG(ERROR) << "The error code isn't an int";
        return false;
      }
      LOG(INFO) << "Configuring request " << request_error.first << " to error "
                << net_error_code.value();
      EmbeddedPolicyTestServer::ConfigureRequestError(
          request_error.first,
          static_cast<net::HttpStatusCode>(net_error_code.value()));
    }
  }

  base::Value::Dict* initial_enrollment_state =
      dict.FindDict(kInitialEnrollmentStateKey);
  if (initial_enrollment_state) {
    for (base::detail::dict_iterator::reference state :
         *initial_enrollment_state) {
      if (!state.second.is_dict()) {
        LOG(ERROR) << "The current state value for key " << state.first
                   << " isn't a dict";
        return false;
      }
      base::Value::Dict& state_val = state.second.GetDict();
      std::string* management_domain =
          state_val.FindString(kManagementDomainKey);
      if (!management_domain) {
        LOG(ERROR) << "The management_domain key isn't a string";
        return false;
      }
      absl::optional<int> initial_enrollment_mode =
          state_val.FindInt(kInitialEnrollmentModeKey);
      if (!initial_enrollment_mode.has_value()) {
        LOG(ERROR) << "The initial_enrollment_mode key isn't an int";
        return false;
      }
      policy::PolicyStorage::InitialEnrollmentState initial_value;
      initial_value.management_domain = *management_domain;
      initial_value.initial_enrollment_mode = static_cast<
          enterprise_management::DeviceInitialEnrollmentStateResponse::
              InitialEnrollmentMode>(initial_enrollment_mode.value());
      policy_storage()->SetInitialEnrollmentState(state.first, initial_value);
    }
  }

  if (dict.contains(kCurrentKeyIndexKey)) {
    absl::optional<int> current_key_index = dict.FindInt(kCurrentKeyIndexKey);
    if (!current_key_index.has_value()) {
      base::Value* v = dict.Find(kCurrentKeyIndexKey);
      LOG(ERROR) << "The current_key_index key isn't an int, found type "
                 << v->type() << ", found value " << *v;
      return false;
    }
    policy_storage()->signature_provider()->set_current_key_version(
        current_key_index.value());
  }

  base::Value::List* policies = dict.FindList(kPoliciesKey);
  if (policies) {
    for (const base::Value& policy : *policies) {
      if (!policy.is_dict()) {
        LOG(ERROR) << "The current policy isn't a dict";
        return false;
      }
      if (!SetPolicyPayload(policy.GetDict().FindString(kPolicyTypeKey),
                            policy.GetDict().FindString(kEntityIdKey),
                            policy.GetDict().FindString(kPolicyValueKey))) {
        LOG(ERROR) << "Failed to set the policy";
        return false;
      }
    }
  }

  base::Value::List* external_policies = dict.FindList(kExternalPoliciesKey);
  if (external_policies) {
    for (const base::Value& policy : *external_policies) {
      if (!policy.is_dict()) {
        LOG(ERROR) << "The current external policy isn't a dict";
        return false;
      }
      if (!SetExternalPolicyPayload(
              policy.GetDict().FindString(kPolicyTypeKey),
              policy.GetDict().FindString(kEntityIdKey),
              policy.GetDict().FindString(kPolicyValueKey))) {
        LOG(ERROR) << "Failed to set the external policy";
        return false;
      }
    }
  }

  return true;
}

base::Value::Dict FakeDMServer::GetValueFromClient(
    const policy::ClientStorage::ClientInfo& c) {
  base::Value::Dict dict;
  dict.Set(kDeviceIdKey, c.device_id);
  dict.Set(kDeviceTokenKey, c.device_token);
  dict.Set(kMachineNameKey, c.machine_name);
  dict.Set(kUsernameKey, c.username.value_or(""));
  base::Value::List state_keys, allowed_policy_types;
  for (auto& key : c.state_keys)
    state_keys.Append(key);
  dict.Set(kStateKeysKey, std::move(state_keys));
  for (auto& policy_type : c.allowed_policy_types)
    allowed_policy_types.Append(policy_type);
  dict.Set(kAllowedPolicyTypesKey, std::move(allowed_policy_types));
  return dict;
}

bool FakeDMServer::WriteClientStateFile() {
  base::FilePath client_state_file(client_state_path_);
  std::vector<policy::ClientStorage::ClientInfo> clients =
      client_storage()->GetAllClients();
  base::Value::Dict dict_clients;
  for (auto& c : clients)
    dict_clients.Set(c.device_id, GetValueFromClient(c));

  JSONFileValueSerializer serializer(client_state_file);
  return serializer.Serialize(base::ValueView(dict_clients));
}

bool FakeDMServer::FindKey(const base::Value::Dict& dict,
                           const std::string& key,
                           base::Value::Type type) {
  switch (type) {
    case base::Value::Type::STRING: {
      const std::string* str_val = dict.FindString(key);
      if (!str_val) {
        LOG(ERROR) << "Key `" << key << "` is missing or not a string.";
        return false;
      }
      return true;
    }
    case base::Value::Type::LIST: {
      const base::Value::List* list_val = dict.FindList(key);
      if (!list_val) {
        LOG(ERROR) << "Key `" << key << "` is missing or not a list.";
        return false;
      }
      return true;
    }
    default: {
      NOTREACHED() << "Unsupported type for client file key";
      return false;
    }
  }
}

absl::optional<policy::ClientStorage::ClientInfo>
FakeDMServer::GetClientFromValue(const base::Value& v) {
  policy::ClientStorage::ClientInfo client_info;
  if (!v.is_dict()) {
    LOG(ERROR) << "Client value isn't a dict";
    return absl::nullopt;
  }

  const base::Value::Dict& dict = v.GetDict();
  if (!FindKey(dict, kDeviceIdKey, base::Value::Type::STRING) ||
      !FindKey(dict, kDeviceTokenKey, base::Value::Type::STRING) ||
      !FindKey(dict, kMachineNameKey, base::Value::Type::STRING) ||
      !FindKey(dict, kUsernameKey, base::Value::Type::STRING) ||
      !FindKey(dict, kStateKeysKey, base::Value::Type::LIST) ||
      !FindKey(dict, kAllowedPolicyTypesKey, base::Value::Type::LIST)) {
    return absl::nullopt;
  }

  client_info.device_id = *dict.FindString(kDeviceIdKey);
  client_info.device_token = *dict.FindString(kDeviceTokenKey);
  client_info.machine_name = *dict.FindString(kMachineNameKey);
  client_info.username = *dict.FindString(kUsernameKey);
  const base::Value::List* state_keys = dict.FindList(kStateKeysKey);
  for (const auto& it : *state_keys) {
    const std::string* key = it.GetIfString();
    if (!key) {
      LOG(ERROR) << "State key list entry is not a string: " << it;
      return absl::nullopt;
    }
    client_info.state_keys.emplace_back(*key);
  }
  const base::Value::List* policy_types = dict.FindList(kAllowedPolicyTypesKey);
  for (const auto& it : *policy_types) {
    const std::string* key = it.GetIfString();
    if (!key) {
      LOG(ERROR) << "Policy type list entry is not a string: " << it;
      return absl::nullopt;
    }
    client_info.allowed_policy_types.insert(*key);
  }
  return client_info;
}

bool FakeDMServer::ReadClientStateFile() {
  base::FilePath client_state_file(client_state_path_);
  if (!base::PathExists(client_state_file)) {
    LOG(INFO) << "Client state file doesn't exist yet.";
    return true;
  }
  JSONFileValueDeserializer deserializer(client_state_file);
  int error_code = 0;
  std::string error_msg;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_msg);
  if (!value) {
    LOG(ERROR) << "Failed to read client state file "
               << client_state_file.value() << ": " << error_msg;
    return false;
  }
  if (!value->is_dict()) {
    LOG(ERROR) << "The client state file isn't a dict.";
    return false;
  }
  base::Value::Dict& dict = value->GetDict();
  for (auto it : dict) {
    absl::optional<policy::ClientStorage::ClientInfo> c =
        GetClientFromValue(it.second);
    if (!c.has_value()) {
      LOG(ERROR) << "The client isn't configured correctly.";
      return false;
    }
    client_storage()->RegisterClient(c.value());
  }
  return true;
}

}  // namespace fakedms
