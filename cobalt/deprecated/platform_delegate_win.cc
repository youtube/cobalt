/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "cobalt/deprecated/platform_delegate.h"

#include <direct.h>
#include <ObjBase.h>
#include <stdlib.h>
#include <windows.h>

#include <string>

#include "lbshell/src/lb_globals.h"
#include "lbshell/src/platform/xb1/posix_emulation/pthread.h"
#include "lbshell/src/platform/xb1/posix_emulation/pthread_internal.h"

// init_seg specifies the static object initialization order, which is compiler
// -> lib -> user. In this case we use a static PThreadInitializer object to
// initialize pthread support inside the POSIX emulation layer. init_seg(lib)
// ensures that __pthread_init() is called before any other initialization in
// our code.
// If this doesn't work, try init_seg(".CRT$XCB"). This will explicitly specify
// the code section, therefore putting the initialization at front most, before
// those of init_seg(compiler).
// For more information see init_seg on MSDN.
#pragma warning(disable : 4073)
#pragma init_seg(lib)
class PThreadInitializer {
 public:
  PThreadInitializer() {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    __pthread_init();
  }
};

namespace cobalt {
namespace deprecated {
namespace {

// This will initialize pthread support inside the POSIX emulation layer.
PThreadInitializer pthread_initializer;

}  // namespace

static bool exit_game_requested = false;

// static
void PlatformDelegate::PlatformInit() {
  global_values_t* global_values = GetGlobalsPtr();
  char path_buffer[MAX_PATH];

  // Get the directory of the executable.
  HMODULE hModule = GetModuleHandle(NULL);
  GetModuleFileNameA(hModule, path_buffer, (sizeof(path_buffer)));
  std::string exe_path = path_buffer;
  unsigned last_bs = exe_path.rfind('\\');
  std::string exe_dir = exe_path.substr(0, last_bs);

  // Set game_content_path.
  std::string game_content_path = exe_dir + "\\content\\data";
  global_values->game_content_path = strdup(game_content_path.c_str());

  // Set dir_source_root.
  std::string dir_source_root = exe_dir + "\\content\\dir_source_root";
  global_values->dir_source_root = strdup(dir_source_root.c_str());

  // Set screenshot_output_path.
#if defined(__LB_SHELL__ENABLE_SCREENSHOT__)
  std::string screenshot_output_path = exe_dir + "\\content\\logs";
  global_values->screenshot_output_path =
      strdup(screenshot_output_path.c_str());
  _mkdir(screenshot_output_path.c_str());
#endif

  // Set logging_output_path.
#if defined(__LB_SHELL__FORCE_LOGGING__) || \
    defined(__LB_SHELL__ENABLE_CONSOLE__)
  std::string logging_output_path = exe_dir + "\\content\\logs";
  global_values->logging_output_path = strdup(logging_output_path.c_str());
  _mkdir(logging_output_path.c_str());
#endif

  // Set tmp_path.
  GetTempPathA(MAX_PATH, path_buffer);
  global_values->tmp_path = strdup(path_buffer);

#if !defined(__LB_SHELL_NO_CHROME__)
  // setup graphics
  // LBGraphics::SetupGraphics();
#endif
}

std::string PlatformDelegate::GetSystemLanguage() {
  return "en-US";
}

// static
void PlatformDelegate::PlatformUpdateDuringStartup() {
}

// static
void PlatformDelegate::PlatformTeardown() {
#if !defined(__LB_SHELL_NO_CHROME__)
  // stop graphics
  // LBGraphics::TeardownGraphics();
#endif

  global_values_t* global_values = GetGlobalsPtr();
  free(const_cast<char*>(global_values->dir_source_root));
  free(const_cast<char*>(global_values->game_content_path));
  free(const_cast<char*>(global_values->screenshot_output_path));
  free(const_cast<char*>(global_values->logging_output_path));
  free(const_cast<char*>(global_values->tmp_path));
  global_values->dir_source_root = NULL;
  global_values->game_content_path = NULL;
  global_values->screenshot_output_path = NULL;
  global_values->logging_output_path = NULL;
  global_values->tmp_path = NULL;
}

// static
bool PlatformDelegate::ExitGameRequested() {
  return exit_game_requested;
}

// static
const char *PlatformDelegate::GameTitleId() {
  return "";
}

// static
void PlatformDelegate::PlatformMediaInit() {
#if !defined(__LB_SHELL_NO_CHROME__)
  // media::ShellAudioStreamer::Initialize();
#endif
}

// static
void PlatformDelegate::PlatformMediaTeardown() {
#if !defined(__LB_SHELL_NO_CHROME__)
  // media::ShellAudioStreamer::Terminate();
#endif
}

// static
void PlatformDelegate::CheckParentalControl() {
}

// static
bool PlatformDelegate::ProtectOutput() {
  // Output protection not supported.
  return false;
}

}  // namespace deprecated
}  // namespace cobalt
