// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_initializer_win.h"

#include <windows.h>

#include "printing/backend/win_helper.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printing_utils.h"

namespace printing {

namespace {

bool HasEscapeSupport(HDC hdc, DWORD escape) {
  const char* ptr = reinterpret_cast<const char*>(&escape);
  return ExtEscape(hdc, QUERYESCSUPPORT, sizeof(escape), ptr, 0, nullptr) > 0;
}

bool IsTechnology(HDC hdc, const char* technology) {
  if (::GetDeviceCaps(hdc, TECHNOLOGY) != DT_RASPRINTER)
    return false;

  if (!HasEscapeSupport(hdc, GETTECHNOLOGY))
    return false;

  char buf[256];
  memset(buf, 0, sizeof(buf));
  if (ExtEscape(hdc, GETTECHNOLOGY, 0, nullptr, sizeof(buf) - 1, buf) <= 0)
    return false;
  return strcmp(buf, technology) == 0;
}

void SetPrinterToGdiMode(HDC hdc) {
  // Try to set to GDI centric mode
  DWORD mode = PSIDENT_GDICENTRIC;
  const char* ptr = reinterpret_cast<const char*>(&mode);
  ExtEscape(hdc, POSTSCRIPT_IDENTIFY, sizeof(DWORD), ptr, 0, nullptr);
}

int GetPrinterPostScriptLevel(HDC hdc) {
  constexpr int param = FEATURESETTING_PSLEVEL;
  const char* param_char_ptr = reinterpret_cast<const char*>(&param);
  int param_out = 0;
  char* param_out_char_ptr = reinterpret_cast<char*>(&param_out);
  if (ExtEscape(hdc, GET_PS_FEATURESETTING, sizeof(param), param_char_ptr,
                sizeof(param_out), param_out_char_ptr) > 0) {
    return param_out;
  }
  return 0;
}

bool IsPrinterPostScript(HDC hdc, int* level) {
  static constexpr char kPostScriptDriver[] = "PostScript";

  // If printer does not support POSTSCRIPT_IDENTIFY, it cannot be set to GDI
  // mode to check the language level supported. See if it looks like a
  // postscript printer and supports the postscript functions that are
  // supported in compatability mode. If so set to level 2 postscript.
  if (!HasEscapeSupport(hdc, POSTSCRIPT_IDENTIFY)) {
    if (!IsTechnology(hdc, kPostScriptDriver))
      return false;
    if (!HasEscapeSupport(hdc, POSTSCRIPT_PASSTHROUGH) ||
        !HasEscapeSupport(hdc, POSTSCRIPT_DATA)) {
      return false;
    }
    *level = 2;
    return true;
  }

  // Printer supports POSTSCRIPT_IDENTIFY so we can assume it has a postscript
  // driver. Set the printer to GDI mode in order to query the postscript
  // level. Use GDI mode instead of PostScript mode so that if level detection
  // fails or returns language level < 2 we can fall back to normal printing.
  // Note: This escape must be called before other escapes.
  SetPrinterToGdiMode(hdc);
  if (!HasEscapeSupport(hdc, GET_PS_FEATURESETTING)) {
    // Can't query the level, use level 2 to be safe
    *level = 2;
    return true;
  }

  // Get the language level. If invalid or < 2, return false to set printer to
  // normal printing mode.
  *level = GetPrinterPostScriptLevel(hdc);
  return *level == 2 || *level == 3;
}

bool IsPrinterXPS(HDC hdc) {
  static constexpr char kXPSDriver[] =
      "http://schemas.microsoft.com/xps/2005/06";
  return IsTechnology(hdc, kXPSDriver);
}

bool IsPrinterTextOnly(HDC hdc) {
  return ::GetDeviceCaps(hdc, TECHNOLOGY) == DT_CHARSTREAM;
}
}  // namespace

// static
void PrintSettingsInitializerWin::InitPrintSettings(
    HDC hdc,
    const DEVMODE& dev_mode,
    PrintSettings* print_settings) {
  DCHECK(hdc);
  DCHECK(print_settings);

  print_settings->SetOrientation(dev_mode.dmOrientation == DMORIENT_LANDSCAPE);
  int dpi_x = GetDeviceCaps(hdc, LOGPIXELSX);
  int dpi_y = GetDeviceCaps(hdc, LOGPIXELSY);
  print_settings->set_dpi_xy(dpi_x, dpi_y);
  int dpi = print_settings->dpi();
  DCHECK_EQ(print_settings->device_units_per_inch(), dpi);

  const int kAlphaCaps = SB_CONST_ALPHA | SB_PIXEL_ALPHA;
  print_settings->set_supports_alpha_blend(
      (GetDeviceCaps(hdc, SHADEBLENDCAPS) & kAlphaCaps) == kAlphaCaps);

  DCHECK_EQ(GetDeviceCaps(hdc, SCALINGFACTORX), 0);
  DCHECK_EQ(GetDeviceCaps(hdc, SCALINGFACTORY), 0);

  // Blink doesn't support different dpi settings in X and Y axis. However,
  // some printers use them. So, to avoid a bad page calculation, scale page
  // size components based on the dpi in the appropriate dimension.
  gfx::Size physical_size_scaled(
      GetDeviceCaps(hdc, PHYSICALWIDTH) * dpi / dpi_x,
      GetDeviceCaps(hdc, PHYSICALHEIGHT) * dpi / dpi_y);
  gfx::Rect printable_area_device_units = GetPrintableAreaDeviceUnits(hdc);
  gfx::Rect printable_area_scaled =
      gfx::Rect(printable_area_device_units.x() * dpi / dpi_x,
                printable_area_device_units.y() * dpi / dpi_y,
                printable_area_device_units.width() * dpi / dpi_x,
                printable_area_device_units.height() * dpi / dpi_y);
  print_settings->SetPrinterPrintableArea(physical_size_scaled,
                                          printable_area_scaled, false);

  print_settings->set_color(IsDevModeWithColor(&dev_mode)
                                ? mojom::ColorModel::kColor
                                : mojom::ColorModel::kGray);

  // Check for postscript first so that we can change the mode with the
  // first command.
  int level;
  if (IsPrinterPostScript(hdc, &level)) {
    if (level == 2) {
      print_settings->set_printer_language_type(
          mojom::PrinterLanguageType::kPostscriptLevel2);
      return;
    }
    DCHECK_EQ(3, level);
    print_settings->set_printer_language_type(
        mojom::PrinterLanguageType::kPostscriptLevel3);
    return;
  }
  // Detects the generic / text only driver.
  if (IsPrinterTextOnly(hdc)) {
    print_settings->set_printer_language_type(
        mojom::PrinterLanguageType::kTextOnly);
    return;
  }
  if (IsPrinterXPS(hdc)) {
    print_settings->set_printer_language_type(mojom::PrinterLanguageType::kXps);
    return;
  }
  print_settings->set_printer_language_type(mojom::PrinterLanguageType::kNone);
}

}  // namespace printing
