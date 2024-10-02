// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_DELEGATE_H_
#define ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace aura {
class Window;
}  // namespace aura

namespace base {
class FilePath;
}  // namespace base

namespace gfx {
class Rect;
}  // namespace gfx

namespace media::mojom {
class AudioStreamFactory;
}  // namespace media::mojom

namespace recording::mojom {
class RecordingService;
}  // namespace recording::mojom

namespace video_capture::mojom {
class VideoSourceProvider;
}  // namespace video_capture::mojom

namespace ash {

class RecordingOverlayView;

// Defines the type of the callback that will be invoked when the DLP (Data Leak
// Prevention) manager is checked for any restricted content related to screen
// capture. DLP is checked multiple times (before entering a capture session,
// when performing the capture, during video recording, and at the end when
// video recording ends). If the callback was invoked with `proceed` set to
// true, then capture mode will proceed with any operation that triggered the
// check. Otherwise, capture mode will abort the operation.
using OnCaptureModeDlpRestrictionChecked =
    base::OnceCallback<void(bool proceed)>;

// Defines the type of the callback that will be invoked when the remaining free
// space on Drive is retrieved. `free_remaining_bytes` will be set to -1 if
// there is an error in computing the DriveFS quota.
using OnGotDriveFsFreeSpace =
    base::OnceCallback<void(int64_t free_remaining_bytes)>;

// Defines the interface for the delegate of CaptureModeController, that can be
// implemented by an ash client (e.g. Chrome). The CaptureModeController owns
// the instance of this delegate.
class ASH_PUBLIC_EXPORT CaptureModeDelegate {
 public:
  virtual ~CaptureModeDelegate() = default;

  // Returns the path to the default downloads directory of the currently active
  // user. This function can only be called if the user is logged in.
  virtual base::FilePath GetUserDefaultDownloadsFolder() const = 0;

  // Shows the screenshot or screen recording item in the screen capture folder.
  virtual void ShowScreenCaptureItemInFolder(
      const base::FilePath& file_path) = 0;

  // Opens the screenshot item in an image editor.
  virtual void OpenScreenshotInImageEditor(const base::FilePath& file_path) = 0;

  // Returns true if the current user is using the 24-hour format (i.e. 14:00
  // vs. 2:00 PM). This is used to build the file name of the captured image or
  // video.
  virtual bool Uses24HourFormat() const = 0;

  // Called when capture mode is being started to check if there are any content
  // currently on the screen that are restricted by DLP. `callback` will be
  // triggered by the DLP manager with `proceed` set to true if capture mode
  // initialization is allowed to continue, or set to false if it should be
  // aborted.
  virtual void CheckCaptureModeInitRestrictionByDlp(
      OnCaptureModeDlpRestrictionChecked callback) = 0;

  // Checks whether capture of the region defined by |window| and |bounds|
  // is currently allowed by the Data Leak Prevention feature. `callback` will
  // be triggered by the DLP manager with `proceed` set to true if capture of
  // that region is allowed, or set to false otherwise.
  virtual void CheckCaptureOperationRestrictionByDlp(
      const aura::Window* window,
      const gfx::Rect& bounds,
      OnCaptureModeDlpRestrictionChecked callback) = 0;

  // Returns whether screen capture is allowed by an enterprise policy.
  virtual bool IsCaptureAllowedByPolicy() const = 0;

  // Called when a video capture for |window| and |bounds| area is started, so
  // that Data Leak Prevention can start observing the area.
  // |on_area_restricted_callback| will be called when the area becomes
  // restricted so that the capture should be interrupted.
  virtual void StartObservingRestrictedContent(
      const aura::Window* window,
      const gfx::Rect& bounds,
      base::OnceClosure on_area_restricted_callback) = 0;

  // Called when the running video capture is stopped. DLP will be checked to
  // determine if there were any restricted content warnings during the
  // recording, which didn't merit force-stopping it via the above
  // `on_area_restricted_callback`. In this case, DLP shows a warning dialog and
  // delegates the decision to the user to decide whether to keep the video (if
  // `proceed` is set to true), or delete it (if `proceed` is set
  // to false).
  virtual void StopObservingRestrictedContent(
      OnCaptureModeDlpRestrictionChecked callback) = 0;

  // Notifies DLP that taking a screenshot was attempted. Called after checking
  // DLP restrictions.
  virtual void OnCaptureImageAttempted(const aura::Window* window,
                                       const gfx::Rect& bounds) = 0;

  // Launches the Recording Service into a separate utility process.
  virtual mojo::Remote<recording::mojom::RecordingService>
  LaunchRecordingService() = 0;

  // Binds the given AudioStreamFactory |receiver| to the audio service.
  virtual void BindAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) = 0;

  // Called when a capture mode session starts or stops.
  virtual void OnSessionStateChanged(bool started) = 0;

  // Called after the controller resets its |mojo::Remote| instance of the
  // service.
  virtual void OnServiceRemoteReset() = 0;

  // Gets the DriveFS mount point. Returns true if the Drive is mounted false
  // otherwise.
  // TODO(michelefan): Now we have both CaptureModeDelegate and ProjectorClient
  // expose the GetDriveFsMountPointPath. Add the APIs in ShellDelegate which is
  // implemented by ChromeShellDelegate in chrome and TestShellDelegate in
  // ash_unittests to reduce the duplication.
  virtual bool GetDriveFsMountPointPath(base::FilePath* path) const = 0;

  // Returns the absolute path for the user's Android Play files.
  virtual base::FilePath GetAndroidFilesPath() const = 0;

  // Returns the absolute path for the user's Linux Files.
  virtual base::FilePath GetLinuxFilesPath() const = 0;

  // Creates and returns the view that will be used as the contents view of the
  // overlay widget, which is added as a child of the recorded surface to host
  // contents rendered in a web view that are meant to be part of the recording
  // such as annotations.
  virtual std::unique_ptr<RecordingOverlayView> CreateRecordingOverlayView()
      const = 0;

  // Connects the given `receiver` to the VideoSourceProvider implementation in
  // the video capture service.
  virtual void ConnectToVideoSourceProvider(
      mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider>
          receiver) = 0;

  // Gets the remaining free space on DriveFS and invokes `callback` with that
  // value, or -1 if there's an error in computing the DriveFS quota.
  virtual void GetDriveFsFreeSpaceBytes(OnGotDriveFsFreeSpace callback) = 0;

  // Returns true if camera support is disabled by admins via
  // the `SystemFeaturesDisableList` policy, false otherwise.
  virtual bool IsCameraDisabledByPolicy() const = 0;

  // Returns true if audio recording is disabled by admins via the
  // `AudioCaptureAllowed` policy.
  virtual bool IsAudioCaptureDisabledByPolicy() const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_DELEGATE_H_
