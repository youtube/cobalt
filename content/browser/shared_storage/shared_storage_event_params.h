// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_EVENT_PARAMS_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_EVENT_PARAMS_H_

#include <map>

#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

// Bundles the varying possible parameters for DevTools shared storage access
// events.
class CONTENT_EXPORT SharedStorageEventParams {
 public:
  // Bundles a URL's spec along with a map of any accompanying reporting
  // metadata for DevTools integration.
  struct CONTENT_EXPORT SharedStorageUrlSpecWithMetadata {
    std::string url;
    std::map<std::string, std::string> reporting_metadata;
    SharedStorageUrlSpecWithMetadata();
    SharedStorageUrlSpecWithMetadata(
        const GURL& url,
        std::map<std::string, std::string> reporting_metadata);
    SharedStorageUrlSpecWithMetadata(const SharedStorageUrlSpecWithMetadata&);
    ~SharedStorageUrlSpecWithMetadata();
    SharedStorageUrlSpecWithMetadata& operator=(
        const SharedStorageUrlSpecWithMetadata&);
    bool operator==(const SharedStorageUrlSpecWithMetadata&) const;
  };

  static SharedStorageEventParams CreateForAddModule(
      const GURL& script_source_url);

  static SharedStorageEventParams CreateForRun(
      const std::string& operation_name,
      const std::vector<uint8_t>& serialized_data);
  static SharedStorageEventParams CreateForSelectURL(
      const std::string& operation_name,
      const std::vector<uint8_t>& serialized_data,
      std::vector<SharedStorageUrlSpecWithMetadata> urls_with_metadata);
  static SharedStorageEventParams CreateForSet(const std::string& key,
                                               const std::string& value,
                                               bool ignore_if_present);
  static SharedStorageEventParams CreateForAppend(const std::string& key,
                                                  const std::string& value);
  static SharedStorageEventParams CreateForGetOrDelete(const std::string& key);
  static SharedStorageEventParams CreateDefault();

  SharedStorageEventParams(const SharedStorageEventParams&);
  ~SharedStorageEventParams();
  SharedStorageEventParams& operator=(const SharedStorageEventParams&);

  absl::optional<std::string> script_source_url;
  absl::optional<std::string> operation_name;
  absl::optional<std::string> serialized_data;
  absl::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
      urls_with_metadata;
  absl::optional<std::string> key;
  absl::optional<std::string> value;
  absl::optional<bool> ignore_if_present;

 private:
  SharedStorageEventParams();
  SharedStorageEventParams(
      absl::optional<std::string> script_source_url,
      absl::optional<std::string> operation_name,
      absl::optional<std::string> serialized_data,
      absl::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
          urls_with_metadata,
      absl::optional<std::string> key,
      absl::optional<std::string> value,
      absl::optional<bool> ignore_if_present);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_EVENT_PARAMS_H_
