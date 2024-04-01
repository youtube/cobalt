// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/x11_switches.h"

namespace switches {

// Which X11 display to connect to. Emulates the GTK+ "--display=" command line
// argument.
const char kX11Display[] = "display";

// Disables MIT-SHM extension.
const char kNoXshm[] = "no-xshm";

}  // namespace switches
