// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_QT_QT_UI_H_
#define UI_QT_QT_UI_H_

#include <memory>

#include "base/component_export.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/font_render_params.h"
#include "ui/linux/linux_ui.h"
#include "ui/qt/qt_interface.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "printing/printing_context_linux.h"  // nogncheck
#endif

namespace qt {

class QtNativeTheme;

// Interface to QT desktop features.
class QtUi : public ui::LinuxUiAndTheme, QtInterface::Delegate {
 public:
  explicit QtUi(ui::LinuxUi* fallback_linux_ui);

  QtUi(const QtUi&) = delete;
  QtUi& operator=(const QtUi&) = delete;

  ~QtUi() override;

  // ui::LinuxUi:
  bool Initialize() override;
  base::TimeDelta GetCursorBlinkInterval() const override;
  gfx::Image GetIconForContentType(const std::string& content_type,
                                   int size,
                                   float scale) const override;
  float GetDeviceScaleFactor() const override;
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;
#if BUILDFLAG(ENABLE_PRINTING)
  printing::PrintDialogLinuxInterface* CreatePrintDialog(
      printing::PrintingContextLinux* context) override;
  gfx::Size GetPdfPaperSize(printing::PrintingContextLinux* context) override;
#endif
  ui::SelectFileDialog* CreateSelectFileDialog(
      void* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) const override;
  std::string GetCursorThemeName() override;
  int GetCursorThemeSize() override;
  std::unique_ptr<ui::LinuxInputMethodContext> CreateInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate) const override;
  bool GetTextEditCommandsForEvent(
      const ui::Event& event,
      std::vector<ui::TextEditCommandAuraLinux>* commands) override;
  gfx::FontRenderParams GetDefaultFontRenderParams() const override;
  void GetDefaultFontDescription(
      std::string* family_out,
      int* size_pixels_out,
      int* style_out,
      int* weight_out,
      gfx::FontRenderParams* params_out) const override;
  bool AnimationsEnabled() const override;
  void AddWindowButtonOrderObserver(
      ui::WindowButtonOrderObserver* observer) override;
  void RemoveWindowButtonOrderObserver(
      ui::WindowButtonOrderObserver* observer) override;
  WindowFrameAction GetWindowFrameAction(
      WindowFrameActionSource source) override;

  // ui::LinuxUiTheme:
  ui::NativeTheme* GetNativeTheme() const override;
  bool GetColor(int id, SkColor* color, bool use_custom_frame) const override;
  bool GetDisplayProperty(int id, int* result) const override;
  void GetFocusRingColor(SkColor* color) const override;
  void GetActiveSelectionBgColor(SkColor* color) const override;
  void GetActiveSelectionFgColor(SkColor* color) const override;
  void GetInactiveSelectionBgColor(SkColor* color) const override;
  void GetInactiveSelectionFgColor(SkColor* color) const override;
  bool PreferDarkTheme() const override;
  std::unique_ptr<ui::NavButtonProvider> CreateNavButtonProvider() override;
  ui::WindowFrameProvider* GetWindowFrameProvider(bool solid_frame) override;

  // QtInterface::Delegate:
  void FontChanged() override;
  void ThemeChanged() override;

 private:
  void AddNativeColorMixer(ui::ColorProvider* provider,
                           const ui::ColorProviderManager::Key& key);

  absl::optional<SkColor> GetColor(int id, bool use_custom_frame) const;

  // TODO(https://crbug.com/1317782): This is a fallback for any unimplemented
  // functionality in the QT backend and should eventually be removed.
  ui::LinuxUi* const fallback_linux_ui_;

  // QT modifies argc and argv, and they must be kept alive while
  // `shim_` is alive.
  CmdLineArgs cmd_line_;

  // Cached default font settings.
  std::string font_family_;
  int font_size_pixels_ = 0;
  int font_size_points_ = 0;
  gfx::Font::FontStyle font_style_ = gfx::Font::NORMAL;
  int font_weight_;
  gfx::FontRenderParams font_params_;

  std::unique_ptr<QtInterface> shim_;

  std::unique_ptr<QtNativeTheme> native_theme_;
};

// This should be the only symbol exported from this component.
COMPONENT_EXPORT(QT)
std::unique_ptr<ui::LinuxUiAndTheme> CreateQtUi(ui::LinuxUi* fallback_linux_ui);

}  // namespace qt

#endif  // UI_QT_QT_UI_H_
