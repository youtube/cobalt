// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/printing_api_utils.h"

#include "base/test/values_test_util.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/printer_configuration.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace idl = api::printing;

namespace {

constexpr char kId[] = "id";
constexpr char kName[] = "name";
constexpr char kDescription[] = "description";
constexpr char kUri[] = "ipp://192.168.1.5";
constexpr int kRank = 2;

constexpr int kCopies = 5;
constexpr int kHorizontalDpi = 300;
constexpr int kVerticalDpi = 400;
constexpr int kMediaSizeWidth = 210000;
constexpr int kMediaSizeHeight = 297000;
constexpr char kMediaSizeVendorId[] = "iso_a4_210x297mm";

constexpr char kCjt[] = R"(
    {
      "version": "1.0",
      "print": {
        "color": {
          "type": "STANDARD_MONOCHROME"
        },
        "duplex": {
          "type": "NO_DUPLEX"
        },
        "page_orientation": {
          "type": "LANDSCAPE"
        },
        "copies": {
          "copies": 5
        },
        "dpi": {
          "horizontal_dpi": 300,
          "vertical_dpi": 400
        },
        "media_size": {
          "width_microns": 210000,
          "height_microns": 297000,
          "vendor_id": "iso_a4_210x297mm"
        },
        "collate": {
          "collate": false
        }
      }
    })";

constexpr char kIncompleteCjt[] = R"(
    {
      "version": "1.0",
      "print": {
        "color": {
          "type": "STANDARD_MONOCHROME"
        },
        "duplex": {
          "type": "NO_DUPLEX"
        },
        "copies": {
          "copies": 5
        },
        "dpi": {
          "horizontal_dpi": 300,
          "vertical_dpi": 400
        }
      }
    })";

std::unique_ptr<printing::PrintSettings> ConstructPrintSettings() {
  auto settings = std::make_unique<printing::PrintSettings>();
  settings->set_color(printing::mojom::ColorModel::kColor);
  settings->set_duplex_mode(printing::mojom::DuplexMode::kLongEdge);
  settings->SetOrientation(/*landscape=*/true);
  settings->set_copies(kCopies);
  settings->set_dpi_xy(kHorizontalDpi, kVerticalDpi);
  printing::PrintSettings::RequestedMedia requested_media;
  requested_media.size_microns = gfx::Size(kMediaSizeWidth, kMediaSizeHeight);
  requested_media.vendor_id = kMediaSizeVendorId;
  settings->set_requested_media(requested_media);
  settings->set_collate(true);
  return settings;
}

printing::PrinterSemanticCapsAndDefaults ConstructPrinterCapabilities() {
  printing::PrinterSemanticCapsAndDefaults capabilities;
  capabilities.color_model = printing::mojom::ColorModel::kColor;
  capabilities.duplex_modes.push_back(printing::mojom::DuplexMode::kLongEdge);
  capabilities.copies_max = kCopies;
  capabilities.dpis.push_back(gfx::Size(kHorizontalDpi, kVerticalDpi));
  printing::PrinterSemanticCapsAndDefaults::Paper paper;
  paper.vendor_id = kMediaSizeVendorId;
  paper.size_um = gfx::Size(kMediaSizeWidth, kMediaSizeHeight);
  capabilities.papers.push_back(paper);
  capabilities.collate_capable = true;
  return capabilities;
}

}  // namespace

TEST(PrintingApiUtilsTest, GetDefaultPrinterRules) {
  std::string default_printer_rules_str =
      R"({"kind": "local", "idPattern": "id.*", "namePattern": "name.*"})";
  absl::optional<DefaultPrinterRules> default_printer_rules =
      GetDefaultPrinterRules(default_printer_rules_str);
  ASSERT_TRUE(default_printer_rules.has_value());
  EXPECT_EQ("local", default_printer_rules->kind);
  EXPECT_EQ("id.*", default_printer_rules->id_pattern);
  EXPECT_EQ("name.*", default_printer_rules->name_pattern);
}

TEST(PrintingApiUtilsTest, GetDefaultPrinterRules_EmptyPref) {
  std::string default_printer_rules_str;
  absl::optional<DefaultPrinterRules> default_printer_rules =
      GetDefaultPrinterRules(default_printer_rules_str);
  EXPECT_FALSE(default_printer_rules.has_value());
}

TEST(PrintingApiUtilsTest, PrinterToIdl) {
  crosapi::mojom::LocalDestinationInfo printer(kId, kName, kDescription, true,
                                               kUri);

  absl::optional<DefaultPrinterRules> default_printer_rules =
      DefaultPrinterRules();
  default_printer_rules->kind = "local";
  default_printer_rules->name_pattern = "n.*e";
  base::flat_map<std::string, int> recently_used_ranks = {{kId, kRank},
                                                          {"ok", 1}};
  idl::Printer idl_printer =
      PrinterToIdl(printer, default_printer_rules, recently_used_ranks);

  EXPECT_EQ(kId, idl_printer.id);
  EXPECT_EQ(kName, idl_printer.name);
  EXPECT_EQ(kDescription, idl_printer.description);
  EXPECT_EQ(kUri, idl_printer.uri);
  EXPECT_EQ(idl::PRINTER_SOURCE_POLICY, idl_printer.source);
  EXPECT_EQ(true, idl_printer.is_default);
  ASSERT_TRUE(idl_printer.recently_used_rank);
  EXPECT_EQ(kRank, *idl_printer.recently_used_rank);
}

TEST(PrintingApiUtilsTest, ParsePrintTicket) {
  base::Value::Dict cjt_ticket = base::test::ParseJsonDict(kCjt);
  std::unique_ptr<printing::PrintSettings> settings =
      ParsePrintTicket(std::move(cjt_ticket));

  ASSERT_TRUE(settings);
  EXPECT_EQ(printing::mojom::ColorModel::kGray, settings->color());
  EXPECT_EQ(printing::mojom::DuplexMode::kSimplex, settings->duplex_mode());
  EXPECT_TRUE(settings->landscape());
  EXPECT_EQ(5, settings->copies());
  EXPECT_EQ(gfx::Size(kHorizontalDpi, kVerticalDpi), settings->dpi_size());
  EXPECT_EQ(gfx::Size(kMediaSizeWidth, kMediaSizeHeight),
            settings->requested_media().size_microns);
  EXPECT_EQ(kMediaSizeVendorId, settings->requested_media().vendor_id);
  EXPECT_FALSE(settings->collate());
}

TEST(PrintingApiUtilsTest, ParsePrintTicket_IncompleteCjt) {
  base::Value::Dict incomplete_cjt_ticket =
      base::test::ParseJsonDict(kIncompleteCjt);
  EXPECT_FALSE(ParsePrintTicket(std::move(incomplete_cjt_ticket)));
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilitiesCompatibility) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  EXPECT_TRUE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilitiesCompatibility_Color) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  capabilities.color_model = printing::mojom::ColorModel::kUnknownColorModel;
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilitiesCompatibility_Duplex) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  capabilities.duplex_modes = {printing::mojom::DuplexMode::kSimplex};
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilitiesCompatibility_Copies) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  capabilities.copies_max = 1;
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilitiesCompatibility_Dpi) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  capabilities.dpis = {gfx::Size(100, 100)};
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest,
     CheckSettingsAndCapabilitiesCompatibility_MediaSize) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  capabilities.papers.clear();
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

TEST(PrintingApiUtilsTest, CheckSettingsAndCapabilitiesCompatibility_Collate) {
  std::unique_ptr<printing::PrintSettings> settings = ConstructPrintSettings();
  printing::PrinterSemanticCapsAndDefaults capabilities =
      ConstructPrinterCapabilities();
  capabilities.collate_capable = false;
  EXPECT_FALSE(
      CheckSettingsAndCapabilitiesCompatibility(*settings, capabilities));
}

}  // namespace extensions
