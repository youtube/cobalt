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

#include <vector>

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/elf_loader/elf_loader.h"
#include "starboard/event.h"
#include "starboard/loader_app/installation_manager.h"
#include "starboard/loader_app/loader_app_switches.h"
#include "starboard/loader_app/system_get_extension_shim.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/command_line.h"
#include "starboard/string.h"
#include "starboard/thread_types.h"

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

// Relative path to the Cobalt's system image content path.
const char kSystemImageContentPath[] = "app/cobalt/content";

// Relative path to the Cobalt's system image library.
const char kSystemImageLibraryPath[] = "app/cobalt/lib/libcobalt.so";

// Cobalt default URL.
const char kCobaltDefaultUrl[] = "https://www.youtube.com/tv";

// Portable ELF loader.
starboard::elf_loader::ElfLoader g_elf_loader;

// Pointer to the |SbEventHandle| function in the
// Cobalt binary.
void (*g_sb_event_func)(const SbEvent*) = NULL;

bool GetContentDir(std::string* content) {
  std::vector<char> content_dir(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathContentDirectory, content_dir.data(),
                       kSbFileMaxPath)) {
    return false;
  }
  *content = content_dir.data();
  return true;
}

void LoadLibraryAndInitialize(const std::string& alternative_content_path) {
  std::string content_dir;
  if (!GetContentDir(&content_dir)) {
    SB_LOG(ERROR) << "Failed to get the content dir";
    return;
  }
  std::string content_path;
  if (alternative_content_path.empty()) {
    content_path = content_dir;
    content_path += kSbFileSepString;
    content_path += kSystemImageContentPath;
  } else {
    content_path = alternative_content_path.c_str();
  }
  std::string library_path = content_dir;
  library_path += kSbFileSepString;
  library_path += kSystemImageLibraryPath;

  if (!g_elf_loader.Load(library_path, content_path, false)) {
    SB_NOTREACHED() << "Failed to load library at '"
                    << g_elf_loader.GetLibraryPath() << "'.";
    return;
  }

  SB_LOG(INFO) << "Successfully loaded '" << g_elf_loader.GetLibraryPath()
               << "'.";

  g_sb_event_func = reinterpret_cast<void (*)(const SbEvent*)>(
      g_elf_loader.LookupSymbol("SbEventHandle"));

  if (!g_sb_event_func) {
    SB_LOG(ERROR) << "Failed to find SbEventHandle.";
    return;
  }

  SB_LOG(INFO) << "Found SbEventHandle at address: "
               << reinterpret_cast<void*>(g_sb_event_func);
}

void LoadUpdatableLibraryAndInitialize(
    const std::string& app_key,
    const std::string& alternative_content_path) {
  // Initialize the Installation Manager.
  // TODO: Pass real app_key to initialize.
  SB_CHECK(ImInitialize(kMaxNumInstallations, "cobalt") == IM_SUCCESS)
      << "Abort. Failed to initialize Installation Manager";

  // Roll forward if needed.
  if (ImRollForwardIfNeeded() == IM_ERROR) {
    SB_LOG(WARNING) << "Failed to roll forward";
  }

  // Loop by priority.
  int current_installation = ImGetCurrentInstallationIndex();
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
    SB_LOG(INFO) << "current_installation=" << current_installation;

    //  Try to load the image. Failures here discard the image.
    std::vector<char> installation_path(kSbFileMaxPath);
    if (ImGetInstallationPath(current_installation, installation_path.data(),
                              kSbFileMaxPath) == IM_ERROR) {
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

    SB_DLOG(INFO) << "installation_path=" << installation_path.data();

    // installation_n/lib/libcobalt.so
    std::vector<char> lib_path(kSbFileMaxPath);
    SbStringFormatF(lib_path.data(), kSbFileMaxPath, "%s%s%s%s%s",
                    installation_path.data(), kSbFileSepString,
                    kCobaltLibraryPath, kSbFileSepString, kCobaltLibraryName);
    SB_LOG(INFO) << "lib_path=" << lib_path.data();

    std::string content;
    if (alternative_content_path.empty()) {
      // installation_n/content
      std::vector<char> content_path(kSbFileMaxPath);
      SbStringFormatF(content_path.data(), kSbFileMaxPath, "%s%s%s",
                      installation_path.data(), kSbFileSepString,
                      kCobaltContentPath);
      content = content_path.data();
    } else {
      content = alternative_content_path.c_str();
    }

    SB_LOG(INFO) << "content=" << content;

    if (!g_elf_loader.Load(lib_path.data(), content, false,
                           &starboard::loader_app::SbSystemGetExtensionShim)) {
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
      SB_DLOG(INFO) << "Symbol Lookup succeeded address: " << p;
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
  static SbMutex mutex = SB_MUTEX_INITIALIZER;

  SB_CHECK(SbMutexAcquire(&mutex) == kSbMutexAcquired);

  if (!g_sb_event_func && event->type == kSbEventTypeStart) {
    const SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
    const starboard::shared::starboard::CommandLine command_line(
        data->argument_count, const_cast<const char**>(data->argument_values));

    bool disable_updates =
        command_line.HasSwitch(starboard::loader_app::kDisableUpdates);
    SB_LOG(INFO) << "disable_updates=" << disable_updates;

    std::string alternative_content =
        command_line.GetSwitchValue(starboard::loader_app::kContent);
    SB_LOG(INFO) << "alternative_content=" << alternative_content;

    if (disable_updates) {
      LoadLibraryAndInitialize(alternative_content);
    } else {
      std::string url =
          command_line.GetSwitchValue(starboard::loader_app::kURL);
      if (url.empty()) {
        url = "https://www.youtube.com/tv";
      }
      // TODO: Call GetAppkey(url, &app_key) to get proper application key;
      std::string app_key = "cobalt";
      LoadUpdatableLibraryAndInitialize(app_key, alternative_content);
    }
    SB_CHECK(g_sb_event_func);
  }

  if (g_sb_event_func != NULL) {
    g_sb_event_func(event);
  }

  SB_CHECK(SbMutexRelease(&mutex) == true);
}
