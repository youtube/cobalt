// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_MAC)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif
#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "ui/gl/gl_surface.h"

extern "C" {
#if defined(GLES2_CONFORM_SUPPORT_ONLY)
#include "gpu/gles2_conform_support/gtf/gtf_stubs.h"
#else
#include "third_party/gles2_conform/GTF_ES/glsl/GTF/Source/GTFMain.h"  // nogncheck
#endif
}

int main(int argc, char *argv[]) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();

#if BUILDFLAG(IS_MAC)
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  std::unique_ptr<const char* []> argsArray(new const char*[args.size() + 1]);
  argsArray[0] = argv[0];

#if BUILDFLAG(IS_WIN)
  std::vector<std::string> argsNonWide(args.size());
  for (size_t index = 0; index < args.size(); ++index) {
    argsNonWide[index] = base::WideToASCII(args[index]);
    argsArray[index+1] = argsNonWide[index].c_str();
  }
#else
  for (size_t index = 0; index < args.size(); ++index) {
    argsArray[index+1] = args[index].c_str();
  }
#endif

  GTFMain(static_cast<int>(args.size()+1),
    const_cast<char**>(argsArray.get()));

  return 0;
}

