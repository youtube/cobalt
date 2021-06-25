// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/system.h"

#include <sys/utsname.h>

#include <cstring>
#include <memory>
#include <string>

#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace {

const char* kBrandName = "Raspberry Pi Foundation";

const char* kModelRevision = "Rev";

const char* kFriendlyName = "Raspberry Pi";

// Read device model from /proc/device-tree/model
const char* GetModelName() {
  const char* file_path = "/proc/device-tree/model";
  // Size of the raw data
  size_t file_data_size;

  file_data_size = 0;
  // Get the size of the file by reading it until the end. This is
  // required because files under /proc do not always return a valid size
  // when using fseek(0, SEEK_END) + ftell(). Nor can the be mmap()-ed.
  FILE* file = fopen(file_path, "r");
  if (file != nullptr) {
    for (;;) {
      char file_buffer[256];
      size_t data_size = fread(file_buffer, 1, sizeof(file_buffer), file);
      if (data_size == 0) {
        break;
      }
      file_data_size += data_size;
    }
    fclose(file);
  }

  const size_t kFileSizeCap = 1000;
  if (file_data_size > kFileSizeCap) {
    SB_LOG(ERROR)
        << "File too large: /proc/device-tree/model is larger than 1KB.";
    return "";
  }

  // Read the contents of the /proc/device-tree/model file.
  const size_t kFileDataSize = file_data_size + 1;
  char file_data[kFileDataSize];
  char* file_data_ptr = file_data;
  memset(file_data_ptr, 0, file_data_size + 1);

  file = fopen(file_path, "r");
  if (file != nullptr) {
    for (size_t offset = 0; offset < file_data_size;) {
      size_t data_size =
          fread(file_data_ptr + offset, 1, file_data_size - offset, file);
      if (data_size == 0) {
        break;
      }
      offset += data_size;
    }
    fclose(file);
  }

  // Zero-terminate the first line of the file because the model name is in the
  // first line
  for (int i = 0; i < file_data_size; i++) {
    if (file_data_ptr[i] == '\n') {
      file_data_ptr[i] = '\0';
      break;
    }
  }

  // Zero-terminate the file data
  file_data_ptr[file_data_size] = '\0';

  // Get Model name
  std::string model_string(const_cast<char*>(file_data));
  std::string model_name;

  // Find the revision number of the model name and remove it if present
  int model_rev_pos = model_string.find(kModelRevision);
  if (model_rev_pos != std::string::npos) {
    model_name = model_string.substr(0, model_rev_pos);
  }

  // Trim trailing spaces
  size_t end = model_name.find_last_not_of(" \t");
  if (end != std::string::npos) {
    return model_name.substr(0, end + 1).c_str();
  } else {
    return "";
  }
}

bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const char* from_value) {
  if (strlen(from_value) + 1 > value_length)
    return false;
  starboard::strlcpy(out_value, from_value, value_length);
  return true;
}

}  // namespace

bool SbSystemGetProperty(SbSystemPropertyId property_id,
                         char* out_value,
                         int value_length) {
  if (!out_value || !value_length) {
    return false;
  }

  switch (property_id) {
    case kSbSystemPropertyBrandName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kBrandName);

    case kSbSystemPropertyModelName:
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        GetModelName());

    case kSbSystemPropertyChipsetModelNumber:
    case kSbSystemPropertyFirmwareVersion:
    case kSbSystemPropertyModelYear:
#if SB_API_VERSION >= 12
    case kSbSystemPropertySystemIntegratorName:
#else
    case kSbSystemPropertyOriginalDesignManufacturerName:
#endif
    case kSbSystemPropertySpeechApiKey:
      return false;

    case kSbSystemPropertyFriendlyName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kFriendlyName);

    case kSbSystemPropertyPlatformName: {
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        "X11; Linux armv7l");
    }

    default:
      SB_DLOG(WARNING) << __FUNCTION__
                       << ": Unrecognized property: " << property_id;
      break;
  }

  return false;
}
