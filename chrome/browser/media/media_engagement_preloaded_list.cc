// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_preloaded_list.h"

#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/media/media_engagement_preload.pb.h"
#include "net/base/lookup_string_in_fixed_set.h"
#include "url/origin.h"
#include "url/url_canon.h"
#include "url/url_constants.h"

// static
MediaEngagementPreloadedList* MediaEngagementPreloadedList::GetInstance() {
  static base::NoDestructor<MediaEngagementPreloadedList> instance;
  return instance.get();
}

MediaEngagementPreloadedList::MediaEngagementPreloadedList() {
  // This class is allowed to be instantiated on any thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

MediaEngagementPreloadedList::~MediaEngagementPreloadedList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool MediaEngagementPreloadedList::loaded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_loaded_;
}

bool MediaEngagementPreloadedList::empty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return dafsa_.empty();
}

bool MediaEngagementPreloadedList::LoadFromFile(const base::FilePath& path) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  // Check the file exists.
  if (!base::PathExists(path)) {
    return false;
  }

  // Read the file to a string.
  std::string file_data;
  if (!base::ReadFileToString(path, &file_data)) {
    return false;
  }

  // Load the preloaded list into a proto message.
  chrome_browser_media::PreloadedData message;
  if (!message.ParseFromString(file_data)) {
    return false;
  }

  // Copy data from the protobuf message.
  dafsa_ = std::vector<unsigned char>(
      message.dafsa().c_str(),
      message.dafsa().c_str() + message.dafsa().length());

  is_loaded_ = true;
  return true;
}

bool MediaEngagementPreloadedList::CheckOriginIsPresent(
    const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if we have loaded the data.
  if (!loaded()) {
    return false;
  }

  // Check if the data is empty.
  if (empty()) {
    return false;
  }

  // By default we are just searching for the hostname.
  std::string location(origin.host());

  // Add :<port> if we use a non-default port.
  if (origin.port() != url::DefaultPortForScheme(origin.scheme().data(),
                                                 origin.scheme().length())) {
    location.push_back(':');
    std::string port(base::NumberToString(origin.port()));
    location.append(std::move(port));
  }

  // Check the string is present and check the scheme matches.
  DafsaResult result = CheckStringIsPresent(location);
  switch (result) {
    case DafsaResult::kFoundHttpsOnly:
      // Only HTTPS is allowed by default.
      if (origin.scheme() == url::kHttpsScheme) {
        return true;
      }
      break;
    case DafsaResult::kFoundHttpOrHttps:
      // Allow either HTTP or HTTPS.
      if (origin.scheme() == url::kHttpScheme ||
          origin.scheme() == url::kHttpsScheme) {
        return true;
      }
      break;
    case DafsaResult::kNotFound:
      break;
  }

  // If we do not match then we should return false.
  return false;
}

MediaEngagementPreloadedList::DafsaResult
MediaEngagementPreloadedList::CheckStringIsPresent(
    const std::string& input) const {
  return static_cast<MediaEngagementPreloadedList::DafsaResult>(
      net::LookupStringInFixedSet(dafsa_.data(), dafsa_.size(), input.c_str(),
                                  input.size()));
}
