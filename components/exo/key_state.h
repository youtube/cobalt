// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_KEY_STATE_H_
#define COMPONENTS_EXO_KEY_STATE_H_

namespace ui {
enum class DomCode;
}

namespace exo {

// Represents the current pressed key state.
struct KeyState {
  ui::DomCode code;
  bool consumed_by_ime;
};

inline bool operator==(const KeyState& lhs, const KeyState& rhs) {
  return lhs.code == rhs.code && lhs.consumed_by_ime == rhs.consumed_by_ime;
}

}  // namespace exo

#endif  // COMPONENTS_EXO_KEY_STATE_H_
