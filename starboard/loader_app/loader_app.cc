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
#include "starboard/elf_loader/evergreen_info.h"
#include "starboard/event.h"
#include "starboard/file.h"
#include "starboard/loader_app/app_key.h"
#include "starboard/loader_app/loader_app_switches.h"
#include "starboard/loader_app/slot_management.h"
#include "starboard/loader_app/system_get_extension_shim.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/command_line.h"
#include "starboard/string.h"
#include "starboard/thread_types.h"
#include "third_party/crashpad/wrapper/wrapper.h"

namespace {

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

class CobaltLibraryLoader : public starboard::loader_app::LibraryLoader {
 public:
  virtual bool Load(const std::string& library_path,
                    const std::string& content_path) {
    return g_elf_loader.Load(library_path, content_path, false,
                             &starboard::loader_app::SbSystemGetExtensionShim);
  }
  virtual void* Resolve(const std::string& symbol) {
    return g_elf_loader.LookupSymbol(symbol.c_str());
  }
};

CobaltLibraryLoader g_cobalt_library_loader;

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

  EvergreenInfo evergreen_info;
  GetEvergreenInfo(&evergreen_info);
  if (!third_party::crashpad::wrapper::AddEvergreenInfoToCrashpad(
          evergreen_info)) {
    SB_LOG(ERROR) << "Could not send Cobalt library information into Crashapd.";
  } else {
    SB_LOG(INFO) << "Loaded Cobalt library information into Crashpad.";
  }

  g_sb_event_func = reinterpret_cast<void (*)(const SbEvent*)>(
      g_elf_loader.LookupSymbol("SbEventHandle"));

  if (!g_sb_event_func) {
    SB_LOG(ERROR) << "Failed to find SbEventHandle.";
    return;
  }

  SB_LOG(INFO) << "Found SbEventHandle at address: "
               << reinterpret_cast<void*>(g_sb_event_func);
}

}  // namespace

void SbEventHandle(const SbEvent* event) {
  static SbMutex mutex = SB_MUTEX_INITIALIZER;

  SB_CHECK(SbMutexAcquire(&mutex) == kSbMutexAcquired);

  if (!g_sb_event_func && (event->type == kSbEventTypeStart ||
                           event->type == kSbEventTypePreload)) {
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
      std::string app_key = starboard::loader_app::GetAppKey(url);
      SB_CHECK(!app_key.empty());

      g_sb_event_func = reinterpret_cast<void (*)(const SbEvent*)>(
          starboard::loader_app::LoadSlotManagedLibrary(
              app_key, alternative_content, &g_cobalt_library_loader));
    }
    SB_CHECK(g_sb_event_func);
  }

  if (g_sb_event_func != NULL) {
    g_sb_event_func(event);
  }

  SB_CHECK(SbMutexRelease(&mutex) == true);
}
