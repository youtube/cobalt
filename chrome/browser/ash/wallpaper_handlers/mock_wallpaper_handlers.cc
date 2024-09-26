// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/mock_wallpaper_handlers.h"

#include <vector>

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace wallpaper_handlers {

namespace {

constexpr char kDataUrlPrefix[] = "data:image/png;base64,";

// Images used in test must have a unique `asset_id` for Personalization App to
// function correctly. Make sure that the fake `collection_id` values used in
// browser tests map to unique `asset_id` values.
int GetStartingAssetId(const std::string& collection_id) {
  if (collection_id == "fake_collection_id_0") {
    return 10;
  } else if (collection_id == "fake_collection_id_1") {
    return 20;
  } else if (collection_id == "fake_collection_id_2") {
    return 30;
  } else {
    return 100;
  }
}

backdrop::Collection GenerateFakeBackdropCollection(int number) {
  backdrop::Collection collection;
  collection.set_collection_id(
      base::StringPrintf("fake_collection_id_%i", number));
  collection.set_collection_name(
      base::StringPrintf("Test Collection %i", number));
  backdrop::Image* image = collection.add_preview();
  // Needs a data url so that it loads
  image->set_image_url(kDataUrlPrefix);
  return collection;
}

backdrop::Image GenerateFakeBackdropImage(const std::string& collection_id,
                                          int asset_id) {
  backdrop::Image image;
  image.set_asset_id(asset_id);
  image.set_image_url(kDataUrlPrefix + base::NumberToString(asset_id));
  for (auto line = 0; line < 2; line++) {
    image.add_attribution()->set_text(
        base::StringPrintf("fake_attribution_%s_asset_id_%i_line_%i",
                           collection_id.c_str(), asset_id, line));
  }
  image.set_unit_id(asset_id);
  image.set_image_type(backdrop::Image_ImageType_IMAGE_TYPE_UNKNOWN);
  return image;
}

ash::personalization_app::mojom::GooglePhotosPhotoPtr
CreateFakeGooglePhotosPhoto(const std::string& id) {
  auto photo = ash::personalization_app::mojom::GooglePhotosPhoto::New();
  photo->id = id;
  photo->name = id;
  photo->dedup_key = id;
  photo->url = GURL(kDataUrlPrefix + id);
  return photo;
}

ash::personalization_app::mojom::FetchGooglePhotosPhotosResponsePtr
CreateFakeGooglePhotosPhotosResponse(
    const absl::optional<std::string>& item_id) {
  auto response =
      ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse::New();
  std::vector<ash::personalization_app::mojom::GooglePhotosPhotoPtr> photos;
  if (item_id.has_value()) {
    // Request for a specific photo with matching id.
    photos.push_back(CreateFakeGooglePhotosPhoto(item_id.value()));
  } else {
    // Request for list of photos.
    for (auto i = 0; i < 3; i++) {
      auto photo = ash::personalization_app::mojom::GooglePhotosPhoto::New();
      std::string id = base::StringPrintf("fake_google_photos_photo_id_%i", i);
      photo->id = id;
      photo->name = id;
      photo->dedup_key = id;
      photo->url = GURL(kDataUrlPrefix + base::NumberToString(i));
      photos.push_back(std::move(photo));
    }
  }

  response->photos = std::move(photos);
  response->resume_token = absl::nullopt;
  return response;
}

ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponsePtr
CreateFakeGooglePhotosSharedAlbumsResponse() {
  std::vector<ash::personalization_app::mojom::GooglePhotosAlbumPtr> result;
  for (int i = 0; i < 3; i++) {
    auto album = ash::personalization_app::mojom::GooglePhotosAlbum::New();
    std::string id =
        base::StringPrintf("fake_google_photos_shared_album_id_%i", i);
    album->id = id;
    album->is_shared = true;
    // Shared albums always have `photo_count == 0` due to technical debt on
    // server side.
    album->photo_count = 0;
    album->preview = GURL(kDataUrlPrefix + base::NumberToString(i));
    album->timestamp = base::Time::Now();
    album->title = id;
    result.push_back(std::move(album));
  }
  return ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse::New(
      std::move(result), absl::nullopt);
}

}  // namespace

MockBackdropCollectionInfoFetcher::MockBackdropCollectionInfoFetcher() {
  ON_CALL(*this, Start).WillByDefault([](OnCollectionsInfoFetched callback) {
    std::vector<backdrop::Collection> collections;
    for (auto i = 0; i < 3; i++) {
      collections.push_back(GenerateFakeBackdropCollection(i));
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*success=*/true, collections));
  });
}

MockBackdropCollectionInfoFetcher::~MockBackdropCollectionInfoFetcher() =
    default;

MockBackdropImageInfoFetcher::MockBackdropImageInfoFetcher(
    const std::string& collection_id)
    : BackdropImageInfoFetcher(collection_id) {
  ON_CALL(*this, Start)
      .WillByDefault([&collection_id](OnImagesInfoFetched callback) {
        std::vector<backdrop::Image> images;
        const auto starting_asset_id = GetStartingAssetId(collection_id);
        for (auto asset_id = starting_asset_id;
             asset_id < starting_asset_id + 3; asset_id++) {
          images.push_back(GenerateFakeBackdropImage(collection_id, asset_id));
        }

        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), /*success=*/true,
                                      collection_id, images));
      });
}

MockBackdropImageInfoFetcher::~MockBackdropImageInfoFetcher() = default;

MockGooglePhotosAlbumsFetcher::MockGooglePhotosAlbumsFetcher(Profile* profile)
    : GooglePhotosAlbumsFetcher(profile) {
  using ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse;
  using ash::personalization_app::mojom::GooglePhotosAlbumPtr;

  ON_CALL(*this, AddRequestAndStartIfNecessary)
      .WillByDefault(
          [](const absl::optional<std::string>& resume_token,
             base::OnceCallback<void(GooglePhotosAlbumsCbkArgs)> callback) {
            auto response = FetchGooglePhotosAlbumsResponse::New(
                std::vector<GooglePhotosAlbumPtr>(), absl::nullopt);
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), std::move(response)));
          });

  ON_CALL(*this, ParseResponse)
      .WillByDefault([this](const base::Value::Dict* response) {
        return GooglePhotosAlbumsFetcher::ParseResponse(response);
      });
}

MockGooglePhotosAlbumsFetcher::~MockGooglePhotosAlbumsFetcher() = default;

absl::optional<size_t> MockGooglePhotosAlbumsFetcher::GetResultCount(
    const GooglePhotosAlbumsCbkArgs& result) {
  return GooglePhotosAlbumsFetcher::GetResultCount(result);
}

MockGooglePhotosSharedAlbumsFetcher::MockGooglePhotosSharedAlbumsFetcher(
    Profile* profile)
    : GooglePhotosSharedAlbumsFetcher(profile) {
  using ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse;
  using ash::personalization_app::mojom::GooglePhotosAlbumPtr;

  ON_CALL(*this, AddRequestAndStartIfNecessary)
      .WillByDefault(
          [](const absl::optional<std::string>& resume_token,
             base::OnceCallback<void(ash::personalization_app::mojom::
                                         FetchGooglePhotosAlbumsResponsePtr)>
                 callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               CreateFakeGooglePhotosSharedAlbumsResponse()));
          });

  ON_CALL(*this, ParseResponse)
      .WillByDefault([this](const base::Value::Dict* response) {
        return GooglePhotosSharedAlbumsFetcher::ParseResponse(response);
      });
}

MockGooglePhotosSharedAlbumsFetcher::~MockGooglePhotosSharedAlbumsFetcher() =
    default;

absl::optional<size_t> MockGooglePhotosSharedAlbumsFetcher::GetResultCount(
    const GooglePhotosAlbumsCbkArgs& result) {
  return GooglePhotosSharedAlbumsFetcher::GetResultCount(result);
}

MockGooglePhotosEnabledFetcher::MockGooglePhotosEnabledFetcher(Profile* profile)
    : GooglePhotosEnabledFetcher(profile) {
  ON_CALL(*this, AddRequestAndStartIfNecessary)
      .WillByDefault(
          [](base::OnceCallback<void(GooglePhotosEnablementState)> callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               GooglePhotosEnablementState::kEnabled));
          });

  ON_CALL(*this, ParseResponse)
      .WillByDefault([this](const base::Value::Dict* response) {
        return GooglePhotosEnabledFetcher::ParseResponse(response);
      });
}

MockGooglePhotosEnabledFetcher::~MockGooglePhotosEnabledFetcher() = default;

absl::optional<size_t> MockGooglePhotosEnabledFetcher::GetResultCount(
    const GooglePhotosEnablementState& result) {
  return GooglePhotosEnabledFetcher::GetResultCount(result);
}

MockGooglePhotosPhotosFetcher::MockGooglePhotosPhotosFetcher(Profile* profile)
    : GooglePhotosPhotosFetcher(profile) {
  using ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse;
  using ash::personalization_app::mojom::GooglePhotosPhotoPtr;

  ON_CALL(*this, AddRequestAndStartIfNecessary)
      .WillByDefault(
          [](const absl::optional<std::string>& item_id,
             const absl::optional<std::string>& album_id,
             const absl::optional<std::string>& resume_token, bool shuffle,
             base::OnceCallback<void(GooglePhotosPhotosCbkArgs)> callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               CreateFakeGooglePhotosPhotosResponse(item_id)));
          });

  ON_CALL(*this, ParseResponse)
      .WillByDefault([this](const base::Value::Dict* response) {
        return GooglePhotosPhotosFetcher::ParseResponse(response);
      });
}

MockGooglePhotosPhotosFetcher::~MockGooglePhotosPhotosFetcher() = default;

absl::optional<size_t> MockGooglePhotosPhotosFetcher::GetResultCount(
    const GooglePhotosPhotosCbkArgs& result) {
  return GooglePhotosPhotosFetcher::GetResultCount(result);
}

}  // namespace wallpaper_handlers
