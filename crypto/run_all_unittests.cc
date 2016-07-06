// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/main_hook.h"
#include "base/test/test_suite.h"
#include "crypto/nss_util.h"

#if !defined(OS_STARBOARD)
int main(int argc, char** argv) {
  MainHook hook(main, argc, argv);

#if defined(USE_NSS)
  // This is most likely not needed, but it basically replaces a similar call
  // that was performed on test_support_base.
  // TODO(rvargas) Bug 79359: remove this.
  crypto::EnsureNSSInit();
#endif  // defined(USE_NSS)

  return base::TestSuite(argc, argv).Run();
}
#else
#include "starboard/event.h"
#include "starboard/system.h"

void SbEventHandle(const SbEvent* event) {
  if (event->type == kSbEventTypeStart) {
    SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
    MainHook hook(NULL, data->argument_count, data->argument_values);
    SbSystemRequestStop(
        base::TestSuite(data->argument_count, data->argument_values).Run());
  }
}
#endif
