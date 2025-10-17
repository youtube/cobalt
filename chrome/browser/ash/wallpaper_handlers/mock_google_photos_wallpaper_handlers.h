// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_MOCK_GOOGLE_PHOTOS_WALLPAPER_HANDLERS_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_MOCK_GOOGLE_PHOTOS_WALLPAPER_HANDLERS_H_

#include <optional>
#include <string>

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "chrome/browser/ash/wallpaper_handlers/google_photos_wallpaper_handlers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace wallpaper_handlers {

// Fetcher that returns an empty album list and no resume token in response to a
// request for the user's Google Photos albums. Used to avoid network requests
// in unit tests.
class MockGooglePhotosAlbumsFetcher : public GooglePhotosAlbumsFetcher {
 public:
  explicit MockGooglePhotosAlbumsFetcher(Profile* profile);

  MockGooglePhotosAlbumsFetcher(const MockGooglePhotosAlbumsFetcher&) = delete;
  MockGooglePhotosAlbumsFetcher& operator=(
      const MockGooglePhotosAlbumsFetcher&) = delete;

  ~MockGooglePhotosAlbumsFetcher() override;

  // GooglePhotosAlbumsFetcher:
  MOCK_METHOD(void,
              AddRequestAndStartIfNecessary,
              (const std::optional<std::string>& resume_token,
               base::OnceCallback<void(GooglePhotosAlbumsCbkArgs)> callback),
              (override));

  MOCK_METHOD(GooglePhotosAlbumsCbkArgs,
              ParseResponse,
              (const base::Value::Dict* response),
              (override));

  // Overridden to increase visibility.
  std::optional<size_t> GetResultCount(
      const GooglePhotosAlbumsCbkArgs& result) override;
};

// Fetcher that returns an empty album list and no resume token in response to a
// request for the user's Google Photos shared albums. Used to avoid network
// requests in unit tests.
class MockGooglePhotosSharedAlbumsFetcher
    : public GooglePhotosSharedAlbumsFetcher {
 public:
  explicit MockGooglePhotosSharedAlbumsFetcher(Profile* profile);

  MockGooglePhotosSharedAlbumsFetcher(
      const MockGooglePhotosSharedAlbumsFetcher&) = delete;
  MockGooglePhotosSharedAlbumsFetcher& operator=(
      const MockGooglePhotosSharedAlbumsFetcher&) = delete;

  ~MockGooglePhotosSharedAlbumsFetcher() override;

  // GooglePhotosSharedAlbumsFetcher:
  MOCK_METHOD(void,
              AddRequestAndStartIfNecessary,
              (const std::optional<std::string>& resume_token,
               base::OnceCallback<void(GooglePhotosAlbumsCbkArgs)> callback),
              (override));

  MOCK_METHOD(GooglePhotosAlbumsCbkArgs,
              ParseResponse,
              (const base::Value::Dict* response),
              (override));

  // Overridden to increase visibility.
  std::optional<size_t> GetResultCount(
      const GooglePhotosAlbumsCbkArgs& result) override;
};

// Fetcher that claims the user is allowed to access Google Photos data. Used to
// avoid network requests in unit tests.
class MockGooglePhotosEnabledFetcher : public GooglePhotosEnabledFetcher {
 public:
  explicit MockGooglePhotosEnabledFetcher(Profile* profile);

  MockGooglePhotosEnabledFetcher(const MockGooglePhotosEnabledFetcher&) =
      delete;
  MockGooglePhotosEnabledFetcher& operator=(
      const MockGooglePhotosEnabledFetcher&) = delete;

  ~MockGooglePhotosEnabledFetcher() override;

  // GooglePhotosEnabledFetcher:
  MOCK_METHOD(void,
              AddRequestAndStartIfNecessary,
              (base::OnceCallback<void(GooglePhotosEnablementState)> callback),
              (override));

  MOCK_METHOD(GooglePhotosEnablementState,
              ParseResponse,
              (const base::Value::Dict* response),
              (override));

  // Overridden to increase visibility.
  std::optional<size_t> GetResultCount(
      const GooglePhotosEnablementState& result) override;
};

// Fetcher that returns an empty photo list and no resume token in response to a
// request for photos from the user's Google Photos library. Used to avoid
// network requests in unit tests.
class MockGooglePhotosPhotosFetcher : public GooglePhotosPhotosFetcher {
 public:
  explicit MockGooglePhotosPhotosFetcher(Profile* profile);

  MockGooglePhotosPhotosFetcher(const MockGooglePhotosPhotosFetcher&) = delete;
  MockGooglePhotosPhotosFetcher& operator=(
      const MockGooglePhotosPhotosFetcher&) = delete;

  ~MockGooglePhotosPhotosFetcher() override;

  // GooglePhotosPhotosFetcher:
  MOCK_METHOD(void,
              AddRequestAndStartIfNecessary,
              (const std::optional<std::string>& item_id,
               const std::optional<std::string>& album_id,
               const std::optional<std::string>& resume_token,
               bool shuffle,
               base::OnceCallback<void(GooglePhotosPhotosCbkArgs)> callback),
              (override));

  MOCK_METHOD(GooglePhotosPhotosCbkArgs,
              ParseResponse,
              (const base::Value::Dict* response),
              (override));

  // Overridden to increase visibility.
  std::optional<size_t> GetResultCount(
      const GooglePhotosPhotosCbkArgs& result) override;
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_MOCK_GOOGLE_PHOTOS_WALLPAPER_HANDLERS_H_
