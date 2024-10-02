// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/picture_in_picture_window_options/picture_in_picture_window_options.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
enum class PictureInPictureResult;
class PictureInPictureWindowController;
class WebContents;
}  // namespace content

namespace display {
class Display;
}  // namespace display

// PictureInPictureWindowManager is a singleton that handles the lifetime of the
// current Picture-in-Picture window and its PictureInPictureWindowController.
// The class also guarantees that only one window will be present per Chrome
// instances regardless of the number of windows, tabs, profiles, etc.
class PictureInPictureWindowManager {
 public:
  // Returns the singleton instance.
  static PictureInPictureWindowManager* GetInstance();

  PictureInPictureWindowManager(const PictureInPictureWindowManager&) = delete;
  PictureInPictureWindowManager& operator=(
      const PictureInPictureWindowManager&) = delete;

  // Shows a PIP window using the window controller for a video element.
  //
  // This mode is triggered through WebContentsDelegate::EnterPictureInPicture,
  // and the default implementation of that fails with a kNotSupported
  // result. For compatibility, this method must also return a
  // content::PictureInPictureResult even though it doesn't fail.
  content::PictureInPictureResult EnterVideoPictureInPicture(
      content::WebContents*);

#if !BUILDFLAG(IS_ANDROID)
  // Shows a PIP window using the window controller for document picture in
  // picture.
  //
  // Document picture-in-picture mode is triggered from the Renderer via
  // WindowOpenDisposition::NEW_PICTURE_IN_PICTURE, and the browser
  // (i.e. Chrome's BrowserNavigator) then calls this method to create the
  // window. There's no corresponding path through the WebContentsDelegate, so
  // it doesn't have a failure state.
  void EnterDocumentPictureInPicture(content::WebContents* parent_web_contents,
                                     content::WebContents* child_web_contents);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Shows a PIP window with an explicitly provided window controller. This is
  // used by ChromeOS ARC windows which do not have a WebContents as the source.
  void EnterPictureInPictureWithController(
      content::PictureInPictureWindowController* pip_window_controller);

  // Closes any existing picture-in-picture windows (video or document pip).
  // Returns true if a picture-in-picture window was closed, and false if there
  // were no picture-in-picture windows to close.
  bool ExitPictureInPicture();

  // Called to notify that the initiator web contents should be focused.
  void FocusInitiator();

  // Gets the web contents in the opener browser window.
  content::WebContents* GetWebContents() const;

  // Gets the web contents in the PiP window. This only applies to document PiP
  // and will be null for video PiP.
  content::WebContents* GetChildWebContents() const;

  // Helper method that will check if the given WebContents is hosted in a
  // document PiP window.
  static bool IsChildWebContents(content::WebContents*);

  // Returns the window bounds of the video picture-in-picture or the document
  // picture-in-picture if either of them is present.
  absl::optional<gfx::Rect> GetPictureInPictureWindowBounds() const;

  // Used for Document picture-in-picture windows only.
  static gfx::Rect CalculateInitialPictureInPictureWindowBounds(
      const blink::mojom::PictureInPictureWindowOptions& pip_options,
      const display::Display& display);

  // Used for Document picture-in-picture windows only.
  // Note that this is meant to represent the inner window bounds. When the pip
  // window is drawn, outer bounds may be greater than kMinWindowSize to
  // accommodate window decorations and ensure the inner bound minimum size
  // respects kMinWindowSize.
  static gfx::Size GetMinimumInnerWindowSize();

  // Used for Document picture-in-picture windows only.
  static gfx::Size GetMaximumWindowSize(const display::Display& display);

 private:
  friend struct base::DefaultSingletonTraits<PictureInPictureWindowManager>;
  class VideoWebContentsObserver;
#if !BUILDFLAG(IS_ANDROID)
  class DocumentWebContentsObserver;
#endif  // !BUILDFLAG(IS_ANDROID)

  // Create a Picture-in-Picture window and register it in order to be closed
  // when needed.
  // This is suffixed with "Internal" because `CreateWindow` is part of the
  // Windows API.
  void CreateWindowInternal(content::WebContents*);

  // Closes the active Picture-in-Picture window.
  // There MUST be a window open.
  // This is suffixed with "Internal" to keep consistency with the method above.
  void CloseWindowInternal();

#if !BUILDFLAG(IS_ANDROID)
  // Called when the document PiP parent web contents is being destroyed.
  void DocumentWebContentsDestroyed();
#endif  // !BUILDFLAG(IS_ANDROID)

  PictureInPictureWindowManager();
  ~PictureInPictureWindowManager();

  std::unique_ptr<VideoWebContentsObserver> video_web_contents_observer_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<DocumentWebContentsObserver> document_web_contents_observer_;
#endif  //! BUILDFLAG(IS_ANDROID)

  raw_ptr<content::PictureInPictureWindowController, DanglingUntriaged>
      pip_window_controller_ = nullptr;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_MANAGER_H_
