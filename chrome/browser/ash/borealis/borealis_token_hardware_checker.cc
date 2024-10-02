// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_token_hardware_checker.h"

#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_features_util.h"

namespace borealis {

namespace {

constexpr uint64_t kGibi = 1024ull * 1024 * 1024;

// Regex used for CPU checks on intel processors, this means "any 11th
// generation or greater i5/i7 processor".
constexpr char kBorealisCapableIntelCpuRegex[] = "[1-9][1-9].. Gen.*i[357]-";

}  // namespace

using AllowStatus = BorealisFeatures::AllowStatus;

// static
AllowStatus BorealisTokenHardwareChecker::BuildAndCheck(Data data) {
  return BorealisTokenHardwareChecker(std::move(data)).Check();
}

BorealisTokenHardwareChecker::BorealisTokenHardwareChecker(Data data)
    : TokenHardwareChecker(std::move(data)) {}

BorealisTokenHardwareChecker::~BorealisTokenHardwareChecker() = default;

AllowStatus BorealisTokenHardwareChecker::Check() const {
  // Get the status from the board's perspective, based on some combination of
  // tokens and hardware/model checks.
  AllowStatus per_board_status = BoardSpecificChecks();

  // Early exit if we're allowed.
  if (per_board_status == AllowStatus::kAllowed) {
    return AllowStatus::kAllowed;
  }

  if (HasNamedToken("super", "i9n6HT3+3Bo:C1p^_qk!\\",
                    "X1391g+2yiuBQrceA3gRGrT7+DQcaYGR/GkmFscyOfQ=")) {
    return AllowStatus::kAllowed;
  }

  return per_board_status;
}

// Helper method that performs different checks based on the user's board.
AllowStatus BorealisTokenHardwareChecker::BoardSpecificChecks() const {
  if (BoardIn({"hatch-borealis", "puff-borealis", "zork-borealis",
               "volteer-borealis", "aurora-borealis"})) {
    if (HasNamedToken("dogfood", "MXlY+SFZ!2,P_k^02]hK",
                      "FbxB2mxNa/uqskX4X+NqHhAE6ebHeWC0u+Y+UlGEB/4=")) {
      return AllowStatus::kAllowed;
    }
    return AllowStatus::kIncorrectToken;
  } else if (IsBoard("volteer")) {
    bool valid_model =
        ModelIn({"delbin", "voxel", "volta", "lindar", "elemi", "volet",
                 "drobit", "lillipup", "delbing", "eldrid", "chronicler"});
    if (HasSufficientHardware(kBorealisCapableIntelCpuRegex) && valid_model) {
      return AllowStatus::kAllowed;
    } else if (HasNamedToken("volteer", "w/8GMLXyB.EOkFaP/-AA",
                             "waiTIRjxZCFjFIRkuUVlnAbiDOMBSzyp3iSJl5x3YwA=")) {
      return AllowStatus::kAllowed;
    }
    return valid_model ? AllowStatus::kHardwareChecksFailed
                       : AllowStatus::kUnsupportedModel;
  } else if (BoardIn({"brya", "adlrvp", "brask"})) {
    if (HasSufficientHardware(kBorealisCapableIntelCpuRegex)) {
      return AllowStatus::kAllowed;
    } else if (HasNamedToken("brya", "tPl24iMxXNR,w$h6,g",
                             "LWULWUcemqmo6Xvdu2LalOYOyo/V4/CkljTmAneXF+U=")) {
      return AllowStatus::kAllowed;
    }
    return AllowStatus::kHardwareChecksFailed;
  } else if (BoardIn({"guybrush", "majolica"})) {
    if (HasSufficientHardware("Ryzen [357]")) {
      return AllowStatus::kAllowed;
    } else if (HasNamedToken("guybrush-majolica", "^_GkTVWDP.FQo5KclS",
                             "ftqv2wT3qeJKajioXqd+VrEW34CciMsigH3MGfMiMsU=")) {
      return AllowStatus::kAllowed;
    }
    return AllowStatus::kHardwareChecksFailed;
  } else if (IsBoard("draco")) {
    return AllowStatus::kAllowed;
  } else if (IsBoard("nissa")) {
    // TODO(b/274537000): unblock for non-developer users.
    if (HasNamedToken("nissa", "nissa/!wcers4vuP7+2a/X$C8",
                      "24/U3nXWbTno/VJwp17HI+UDzWd77iXj5oDgavIZhoI=")) {
      return AllowStatus::kAllowed;
    }
  } else if (IsBoard("skyrim")) {
    // TODO(b/274537000): unblock for non-developer users.
    if (HasNamedToken("skyrim", "skyrim/!2-DxWY_cL/nXF1U+oV",
                      "esBGhWX18eOMlNrqOS5oEcFfyy0MbNJ5VWz+92iVOwk=")) {
      return AllowStatus::kAllowed;
    }
  }
  return AllowStatus::kUnsupportedModel;
}

bool BorealisTokenHardwareChecker::HasSufficientHardware(
    const std::string& cpu_regex) const {
  return HasMemory(7 * kGibi) && CpuRegexMatches(cpu_regex);
}

bool BorealisTokenHardwareChecker::HasNamedToken(const char* name,
                                                 const char* salt,
                                                 const char* expected) const {
  if (TokenHashMatches(salt, expected)) {
    LOG(WARNING) << "Bypassing hardware checks with \"" << name << "\" token";
    return true;
  }
  return false;
}

}  // namespace borealis
