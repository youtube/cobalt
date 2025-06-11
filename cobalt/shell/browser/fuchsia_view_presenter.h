// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_SHELL_BROWSER_FUCHSIA_VIEW_PRESENTER_H_
#define COBALT_SHELL_BROWSER_FUCHSIA_VIEW_PRESENTER_H_

#include <fuchsia/element/cpp/fidl.h>

namespace content {

class FuchsiaViewPresenter final {
 public:
  FuchsiaViewPresenter();
  ~FuchsiaViewPresenter();

  FuchsiaViewPresenter(const FuchsiaViewPresenter&) = delete;
  FuchsiaViewPresenter& operator=(const FuchsiaViewPresenter&) = delete;

 private:
  fuchsia::element::ViewControllerPtr PresentScenicView(
      fuchsia::ui::views::ViewHolderToken view_holder_token,
      fuchsia::ui::views::ViewRef view_ref);

  fuchsia::element::ViewControllerPtr PresentFlatlandView(
      fuchsia::ui::views::ViewportCreationToken viewport_creation_token);

  bool callbacks_were_set_ = false;
  fuchsia::element::GraphicalPresenterPtr graphical_presenter_;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_FUCHSIA_VIEW_PRESENTER_H_
