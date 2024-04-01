// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "media/midi/midi_switches.h"

namespace midi {
namespace features {

#if defined(OS_WIN)
const base::Feature kMidiManagerWinrt{"MidiManagerWinrt",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace features
}  // namespace midi
