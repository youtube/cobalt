// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/main_hook.h"
#include "base/test/test_suite.h"
#include "sql/test_vfs.h"

#if !defined(OS_STARBOARD)
int main(int argc, char** argv) {
  MainHook hook(main, argc, argv);
  sql::RegisterTestVfs();
  int result = base::TestSuite(argc, argv).Run();
  sql::UnregisterTestVfs();
  return result;
}
#else
#include "starboard/event.h"
#include "starboard/system.h"

void SbEventHandle(const SbEvent* event) {
  if (event->type == kSbEventTypeStart) {
    SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
    MainHook hook(NULL, data->argument_count, data->argument_values);
    sql::RegisterTestVfs();
    int error_level =
        base::TestSuite(data->argument_count, data->argument_values).Run();
    sql::UnregisterTestVfs();
    SbSystemRequestStop(error_level);
  }
}
#endif
