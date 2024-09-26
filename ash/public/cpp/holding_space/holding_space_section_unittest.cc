// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_section.h"

#include <set>

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Helpers ---------------------------------------------------------------------

std::set<HoldingSpaceItem::Type> GetHoldingSpaceItemTypes() {
  std::set<HoldingSpaceItem::Type> types;
  for (size_t i = 0u;
       i <= static_cast<size_t>(HoldingSpaceItem::Type::kMaxValue); ++i) {
    types.emplace(static_cast<HoldingSpaceItem::Type>(i));
  }
  return types;
}

std::set<HoldingSpaceSectionId> GetHoldingSpaceSectionIds() {
  std::set<HoldingSpaceSectionId> section_ids;
  for (size_t i = static_cast<size_t>(HoldingSpaceSectionId::kMinValue);
       i <= static_cast<size_t>(HoldingSpaceSectionId::kMaxValue); ++i) {
    section_ids.emplace(static_cast<HoldingSpaceSectionId>(i));
  }
  return section_ids;
}

void ExpectSection(const HoldingSpaceSection* section,
                   HoldingSpaceSectionId expected_id) {
  switch (expected_id) {
    case HoldingSpaceSectionId::kDownloads:
      EXPECT_EQ(section->id, HoldingSpaceSectionId::kDownloads);
      EXPECT_THAT(section->supported_types,
                  testing::UnorderedElementsAre(
                      HoldingSpaceItem::Type::kArcDownload,
                      HoldingSpaceItem::Type::kCameraAppPhoto,
                      HoldingSpaceItem::Type::kCameraAppScanJpg,
                      HoldingSpaceItem::Type::kCameraAppScanPdf,
                      HoldingSpaceItem::Type::kCameraAppVideoGif,
                      HoldingSpaceItem::Type::kCameraAppVideoMp4,
                      HoldingSpaceItem::Type::kDiagnosticsLog,
                      HoldingSpaceItem::Type::kDownload,
                      HoldingSpaceItem::Type::kLacrosDownload,
                      HoldingSpaceItem::Type::kNearbyShare,
                      HoldingSpaceItem::Type::kPhoneHubCameraRoll,
                      HoldingSpaceItem::Type::kPrintedPdf,
                      HoldingSpaceItem::Type::kScan));
      EXPECT_EQ(section->max_item_count, 50u);
      EXPECT_EQ(section->max_visible_item_count, 4u);
      break;
    case HoldingSpaceSectionId::kPinnedFiles:
      EXPECT_EQ(section->id, HoldingSpaceSectionId::kPinnedFiles);
      EXPECT_THAT(
          section->supported_types,
          testing::UnorderedElementsAre(HoldingSpaceItem::Type::kPinnedFile));
      EXPECT_EQ(section->max_item_count, absl::nullopt);
      EXPECT_EQ(section->max_visible_item_count, absl::nullopt);
      break;
    case HoldingSpaceSectionId::kScreenCaptures:
      EXPECT_EQ(section->id, HoldingSpaceSectionId::kScreenCaptures);
      EXPECT_THAT(section->supported_types,
                  testing::UnorderedElementsAre(
                      HoldingSpaceItem::Type::kScreenRecording,
                      HoldingSpaceItem::Type::kScreenRecordingGif,
                      HoldingSpaceItem::Type::kScreenshot));
      EXPECT_EQ(section->max_item_count, 50u);
      EXPECT_EQ(section->max_visible_item_count, 3u);
      break;
    case HoldingSpaceSectionId::kSuggestions:
      EXPECT_EQ(section->id, HoldingSpaceSectionId::kSuggestions);
      EXPECT_THAT(section->supported_types,
                  testing::UnorderedElementsAre(
                      HoldingSpaceItem::Type::kLocalSuggestion,
                      HoldingSpaceItem::Type::kDriveSuggestion));
      EXPECT_EQ(section->max_item_count, absl::nullopt);
      EXPECT_EQ(section->max_visible_item_count, 4u);
      break;
  }
}

}  // namespace

// Tests -----------------------------------------------------------------------

using HoldingSpaceSectionTest = testing::Test;

// Verifies that every `HoldingSpaceSectionId` maps to an expected section.
TEST_F(HoldingSpaceSectionTest, GetHoldingSpaceSectionById) {
  for (const auto& id : GetHoldingSpaceSectionIds()) {
    SCOPED_TRACE(testing::Message() << "ID: " << static_cast<size_t>(id));
    ExpectSection(GetHoldingSpaceSection(id), id);
  }
}

// Verifies that every `HoldingSpaceItem::Type` maps to an expected section.
TEST_F(HoldingSpaceSectionTest, GetHoldingSpaceSectionByType) {
  for (const auto& type : GetHoldingSpaceItemTypes()) {
    SCOPED_TRACE(testing::Message() << "Type: " << static_cast<size_t>(type));
    absl::optional<HoldingSpaceSectionId> id;
    switch (type) {
      case HoldingSpaceItem::Type::kArcDownload:
      case HoldingSpaceItem::Type::kCameraAppPhoto:
      case HoldingSpaceItem::Type::kCameraAppScanJpg:
      case HoldingSpaceItem::Type::kCameraAppScanPdf:
      case HoldingSpaceItem::Type::kCameraAppVideoGif:
      case HoldingSpaceItem::Type::kCameraAppVideoMp4:
      case HoldingSpaceItem::Type::kDiagnosticsLog:
      case HoldingSpaceItem::Type::kDownload:
      case HoldingSpaceItem::Type::kLacrosDownload:
      case HoldingSpaceItem::Type::kNearbyShare:
      case HoldingSpaceItem::Type::kPhoneHubCameraRoll:
      case HoldingSpaceItem::Type::kPrintedPdf:
      case HoldingSpaceItem::Type::kScan:
        id = HoldingSpaceSectionId::kDownloads;
        break;
      case HoldingSpaceItem::Type::kDriveSuggestion:
      case HoldingSpaceItem::Type::kLocalSuggestion:
        id = HoldingSpaceSectionId::kSuggestions;
        break;
      case HoldingSpaceItem::Type::kPinnedFile:
        id = HoldingSpaceSectionId::kPinnedFiles;
        break;
      case HoldingSpaceItem::Type::kScreenRecording:
      case HoldingSpaceItem::Type::kScreenRecordingGif:
      case HoldingSpaceItem::Type::kScreenshot:
        id = HoldingSpaceSectionId::kScreenCaptures;
        break;
    }
    ExpectSection(GetHoldingSpaceSection(type), id.value());
  }
}

}  // namespace ash
