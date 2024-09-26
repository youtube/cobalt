// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_MANAGED_PHOTO_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_MANAGED_PHOTO_CONTROLLER_H_

#include <vector>

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// Class to handle policy-set photos in ambient mode. This is a barebones
// controller which takes in a list of file paths and adds them to the backend
// model one by one.
class ASH_EXPORT AmbientManagedPhotoController
    : public AmbientViewDelegateObserver {
 public:
  AmbientManagedPhotoController(AmbientViewDelegate& view_delegate,
                                AmbientPhotoConfig photo_config);

  AmbientManagedPhotoController(const AmbientManagedPhotoController&) = delete;
  AmbientManagedPhotoController& operator=(
      const AmbientManagedPhotoController&) = delete;

  ~AmbientManagedPhotoController() override;

  // Start/stop updating the screen contents.
  void StartScreenUpdate();
  void StopScreenUpdate();
  bool IsScreenUpdateActive() const;

  // Updates the image file paths, if controller is active, this method will
  // also load these files from disk and put them in backend model in case the
  // controller is in the active state.
  void UpdateImageFilePaths(const std::vector<base::FilePath>& path_to_images);

  AmbientBackendModel* ambient_backend_model() {
    return &ambient_backend_model_;
  }
  // AmbientViewDelegateObserver:
  void OnMarkerHit(AmbientPhotoConfig::Marker marker) override;

 private:
  enum class State {
    kStarted,
    // The controller was started but we failed to load a sufficient number of
    // images to continue even after trying to load all the provided images from
    // disk.
    kStartedPhotoLoadFailure,
    kStopped
  };

  // Load and decode images
  void LoadImages();
  size_t GetMaxImageAttempts() const;
  // Loads `images_to_load` no of images in an asynchronous but sequential
  // manner and waits for the previous image to load before starting to load the
  // next image. `success` denotes that the previous load was successful. Note:
  // In case `success` is false or `images_to_load` is 0 this method will be  a
  // no-op.
  void LoadImagesInternal(size_t images_to_load, bool success);
  void LoadNextImage(base::OnceCallback<void(bool success)> callback);
  void OnPhotoDecoded(base::OnceCallback<void(bool success)> callback,
                      const gfx::ImageSkia& image);
  void HandlePhotoDecodingFailure(
      base::OnceCallback<void(bool success)> callback);

  AmbientBackendModel ambient_backend_model_;

  // The current number of tries for loading the next image, once it reaches the
  // max tries, we will log an error and stop retrying. This is reset as soon as
  // an image is decoded successfully.
  size_t image_attempt_no_ = 0;

  // Current index of cached file to read and display.
  size_t current_image_index_ = 0;

  // State used to determine whether the controller is active.
  State state_ = State::kStopped;

  // The list of image filepaths that are used as the sources for the images to
  // show.
  std::vector<base::FilePath> images_file_paths_;

  base::ScopedObservation<AmbientViewDelegate, AmbientViewDelegateObserver>
      scoped_view_delegate_observation_{this};

  base::WeakPtrFactory<AmbientManagedPhotoController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_MANAGED_PHOTO_CONTROLLER_H_
