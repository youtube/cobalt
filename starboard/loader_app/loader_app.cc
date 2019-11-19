// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/elf_loader/elf_loader.h"
#include "starboard/event.h"
#include "starboard/loader_app/installation_manager.h"
#include "starboard/string.h"

// TODO: Try to merge with the implementation in starboard/elf_loader/sandbox.cc

namespace {
// The max number of installations slots.
const int kMaxNumInstallations = 3;

// Relative path for the Cobalt so file.
const char kCobaltLibraryPath[] = "lib";

// Filename for the Cobalt binary.
const char kCobaltLibraryName[] = "libcobalt.so";

// Relative path for the content directory of
// the Cobalt installation.
const char kCobaltContentPath[] = "content";

// Portable ELF loader.
starboard::elf_loader::ElfLoader g_elf_loader;

// Pointer to the |SbEventHandle| function in the
// Cobalt binary.
void (*g_sb_event_func)(const SbEvent*) = NULL;

void LoadLibraryAndInitialize() {
  // Initialize the Installation Manager.
  SB_CHECK(ImInitialize(kMaxNumInstallations) == IM_SUCCESS)
      << "Abort. Failed to initialize Installation Manager";

  // Roll forward if needed.
  if (ImRollForwardIfNeeded() == IM_ERROR) {
    SB_LOG(WARNING) << "Failed to roll forward";
  }

  // Loop by priority.
  int current_installation = ImGetCurrentInstallationIndex();
  SB_LOG(INFO) << "current_installation=" << current_installation;
  while (current_installation != IM_ERROR) {
    // if not successful and num_tries_left > 0 decrement and try to
    // load the library.
    if (ImGetInstallationStatus(current_installation) !=
        IM_INSTALLATION_STATUS_SUCCESS) {
      int num_tries_left = ImGetInstallationNumTriesLeft(current_installation);
      if (num_tries_left == IM_ERROR || num_tries_left <= 0 ||
          ImDecrementInstallationNumTries(current_installation) == IM_ERROR) {
        // If no more tries are left or if we have hard failure,
        // discard the image and auto rollback, but only if
        // the current image is not the system image.
        if (current_installation != 0) {
          current_installation = ImRevertToSuccessfulInstallation();
        }
      }
    }

    SB_LOG(INFO) << "Try to load the Cobalt binary";

    //  Try to load the image. Failures here discard the image.
    char installation_path[SB_FILE_MAX_PATH];
    if (ImGetInstallationPath(current_installation, installation_path,
                              SB_FILE_MAX_PATH) == IM_ERROR) {
      SB_LOG(ERROR) << "Failed to find library file";

      // Hard failure. Discard the image and auto rollback, but only if
      // the current image is not the system image.
      if (current_installation != 0) {
        current_installation = ImRevertToSuccessfulInstallation();
        continue;
      } else {
        // The system image at index 0 failed.
        break;
      }
    }

    SB_DLOG(INFO) << "installation_path=" << installation_path;

    // installation_n/lib/libcobalt.so
    char lib_path[SB_FILE_MAX_PATH];
    SbStringFormatF(lib_path, SB_FILE_MAX_PATH, "%s%s%s%s%s", installation_path,
                    SB_FILE_SEP_STRING, kCobaltLibraryPath, SB_FILE_SEP_STRING,
                    kCobaltLibraryName);
    SB_LOG(INFO) << "lib_path=" << lib_path;

    // installation_n/content
    char content_path[SB_FILE_MAX_PATH];
    SbStringFormatF(content_path, SB_FILE_MAX_PATH, "%s%s%s", installation_path,
                    SB_FILE_SEP_STRING, kCobaltContentPath);
    SB_LOG(INFO) << "content_path=" << content_path;

    if (!g_elf_loader.Load(lib_path, content_path, false)) {
      SB_LOG(WARNING) << "Failed to load Cobalt!";

      // Hard failure. Discard the image and auto rollback, but only if
      // the current image is not the system image.
      if (current_installation != 0) {
        current_installation = ImRevertToSuccessfulInstallation();
        continue;
      } else {
        // The system image at index 0 failed.
        break;
      }
    }

    SB_DLOG(INFO) << "Successfully loaded Cobalt!\n";
    void* p = g_elf_loader.LookupSymbol("SbEventHandle");
    if (p != NULL) {
      SB_DLOG(INFO) << "Symbol Lookup succeeded address=0x" << std::hex << p;
      g_sb_event_func = (void (*)(const SbEvent*))p;
      break;
    } else {
      SB_LOG(ERROR) << "Symbol Lookup failed\n";

      // Hard failure. Discard the image and auto rollback, but only if
      // the current image is not the system image.
      if (current_installation != 0) {
        current_installation = ImRevertToSuccessfulInstallation();
        continue;
      } else {
        // The system image at index 0 failed.
        break;
      }
    }
  }
}
}  // namespace

void SbEventHandle(const SbEvent* event) {
  if (!g_sb_event_func) {
    LoadLibraryAndInitialize();
  }
  SB_CHECK(g_sb_event_func);
  g_sb_event_func(event);
}
