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

#include <pthread.h>
#include <sys/stat.h>

#include <vector>

#include "cobalt/version.h"
#include "starboard/common/command_line.h"
#include "starboard/common/log.h"
#include "starboard/common/paths.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/elf_loader/elf_loader.h"
#include "starboard/elf_loader/elf_loader_constants.h"
#include "starboard/elf_loader/evergreen_info.h"
#include "starboard/elf_loader/sabi_string.h"
#include "starboard/event.h"
#include "starboard/extension/loader_app_metrics.h"
#include "starboard/loader_app/app_key.h"
#include "starboard/loader_app/loader_app_switches.h"
#include "starboard/loader_app/memory_tracker_thread.h"
#include "starboard/loader_app/record_loader_app_status.h"
#include "starboard/loader_app/reset_evergreen_update.h"
#include "starboard/loader_app/slot_management.h"
#include "starboard/loader_app/system_get_extension_shim.h"

#include "starboard/common/check_op.h"
#include "starboard/crashpad_wrapper/annotations.h"
#include "starboard/crashpad_wrapper/wrapper.h"

namespace {

// Relative path to the Cobalt's system image content path.
const char kSystemImageContentPath[] = "app/cobalt/content";

// Relative path to the Cobalt's system image library.
const char kSystemImageLibraryPath[] = "app/cobalt/lib/libcobalt.so";

// Relative path to the compressed Cobalt's system image library.
const char kSystemImageCompressedLibraryPath[] = "app/cobalt/lib/libcobalt.lz4";

// Cobalt default URL.
const char kCobaltDefaultUrl[] = "https://www.youtube.com/tv";

// Portable ELF loader.
elf_loader::ElfLoader g_elf_loader;

// Pointer to the |SbEventHandle| function in the
// Cobalt binary.
void (*g_sb_event_func)(const SbEvent*) = NULL;

class CobaltLibraryLoader : public loader_app::LibraryLoader {
 public:
  virtual bool Load(const std::string& library_path,
                    const std::string& content_path,
                    bool use_compression,
                    bool use_memory_mapped_file) {
    return g_elf_loader.Load(library_path, content_path, false,
                             &loader_app::SbSystemGetExtensionShim,
                             use_compression, use_memory_mapped_file);
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

void LoadLibraryAndInitialize(const std::string& alternative_content_path,
                              bool use_memory_mapped_file) {
  std::string content_dir;
  if (!GetContentDir(&content_dir)) {
    SB_LOG(ERROR) << "Failed to get the content dir";
    return;
  }
  SB_LOG(INFO) << "getting content_dir" << content_dir;
  std::string content_path;
  if (alternative_content_path.empty()) {
    content_path = content_dir;
    content_path += kSbFileSepString;
    content_path += kSystemImageContentPath;
    SB_LOG(INFO) << "getting content_path" << content_path
                 << "alternative_content_path" << alternative_content_path;
  } else {
    content_path = alternative_content_path.c_str();
    SB_LOG(INFO) << "getting content_path" << content_path;
  }
  std::string library_path = content_dir;
  library_path += kSbFileSepString;

  std::string compressed_library_path(library_path);
  compressed_library_path += kSystemImageCompressedLibraryPath;

  std::string uncompressed_library_path(library_path);
  uncompressed_library_path += kSystemImageLibraryPath;

  bool use_compression;
  struct stat info;
  if (stat(compressed_library_path.c_str(), &info) == 0) {
    library_path = compressed_library_path;
    use_compression = true;
  } else if (stat(uncompressed_library_path.c_str(), &info) == 0) {
    library_path = uncompressed_library_path;
    use_compression = false;
  } else {
    SB_LOG(ERROR) << "No library found at compressed "
                  << compressed_library_path << " or uncompressed "
                  << uncompressed_library_path << " path";
    return;
  }

  if (use_compression && use_memory_mapped_file) {
    SB_LOG(ERROR) << "Using both compression and mmap files is not supported";
    return;
  }

  if (!g_elf_loader.Load(library_path, content_path, false, nullptr,
                         use_compression, use_memory_mapped_file)) {
    SB_NOTREACHED() << "Failed to load library at '"
                    << g_elf_loader.GetLibraryPath() << "'.";
    return;
  }

  SB_LOG(INFO) << "Successfully loaded '" << g_elf_loader.GetLibraryPath()
               << "'.";

  EvergreenInfo evergreen_info;
  GetEvergreenInfo(&evergreen_info);
  if (!crashpad::AddEvergreenInfoToCrashpad(evergreen_info)) {
    SB_LOG(ERROR) << "Could not send Cobalt library information into Crashpad.";
  } else {
    SB_LOG(INFO) << "Loaded Cobalt library information into Crashpad.";
  }

  auto get_evergreen_sabi_string_func = reinterpret_cast<const char* (*)()>(
      g_elf_loader.LookupSymbol("GetEvergreenSabiString"));

  if (!CheckSabi(get_evergreen_sabi_string_func)) {
    SB_LOG(ERROR) << "CheckSabi failed";
    return;
  }

  auto get_user_agent_func = reinterpret_cast<const char* (*)()>(
      g_elf_loader.LookupSymbol("GetCobaltUserAgentString"));
  if (!get_user_agent_func) {
    SB_LOG(ERROR) << "Failed to get user agent string";
  } else {
    std::vector<char> buffer(USER_AGENT_STRING_MAX_SIZE);
    starboard::strlcpy(buffer.data(), get_user_agent_func(),
                       USER_AGENT_STRING_MAX_SIZE);
    if (crashpad::InsertCrashpadAnnotation(
            crashpad::kCrashpadUserAgentStringKey, buffer.data())) {
      SB_DLOG(INFO) << "Added user agent string to Crashpad.";
    } else {
      SB_DLOG(INFO) << "Failed to add user agent string to Crashpad.";
    }
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

void InstallCrashpadHandler(const std::string& evergreen_content_path) {
  std::string ca_certificates_path =
      evergreen_content_path.empty()
          ? starboard::GetCACertificatesPath()
          : starboard::GetCACertificatesPath(evergreen_content_path);
  if (ca_certificates_path.empty()) {
    SB_LOG(ERROR) << "Failed to get CA certificates path";
  }

  crashpad::InstallCrashpadHandler(ca_certificates_path);
}

}  // namespace

void SbEventHandle(const SbEvent* event) {
  static pthread_mutex_t mutex PTHREAD_MUTEX_INITIALIZER;

  SB_CHECK_EQ(pthread_mutex_lock(&mutex), 0);

  if (!g_sb_event_func && (event->type == kSbEventTypeStart ||
                           event->type == kSbEventTypePreload)) {
    const SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
    const starboard::CommandLine command_line(
        data->argument_count, const_cast<const char**>(data->argument_values));

    if (command_line.HasSwitch(loader_app::kResetEvergreenUpdate)) {
      SB_LOG(INFO) << "Resetting the Evergreen Update";
      loader_app::ResetEvergreenUpdate();
      SbSystemRequestStop(0);
      SB_CHECK_EQ(pthread_mutex_unlock(&mutex), 0);
      return;
    }

    if (command_line.HasSwitch(loader_app::kLoaderAppVersion)) {
      std::string versiong_msg = "Loader app version: ";
      versiong_msg += COBALT_VERSION;
      versiong_msg += "\n";
      SbLogRaw(versiong_msg.c_str());
    }

    bool is_evergreen_lite = command_line.HasSwitch(loader_app::kEvergreenLite);
    SB_LOG(INFO) << "is_evergreen_lite=" << is_evergreen_lite;

    if (command_line.HasSwitch(loader_app::kShowSABI)) {
      std::string sabi = "SABI=";
      sabi += SB_SABI_JSON_ID;
      SbLogRaw(sabi.c_str());
    }

    std::string alternative_content =
        command_line.GetSwitchValue(loader_app::kContent);
    SB_LOG(INFO) << "alternative_content=" << alternative_content;

    bool use_compressed_updates =
        !is_evergreen_lite &&
        !command_line.HasSwitch(loader_app::kUseUncompressedUpdates);

    bool use_memory_mapped_file =
        command_line.HasSwitch(loader_app::kLoaderUseMemoryMappedFile);
    SB_LOG(INFO) << "use_memory_mapped_file=" << use_memory_mapped_file;

    if (use_compressed_updates && use_memory_mapped_file) {
      SB_LOG(ERROR) << "Compression and memory mapping are incompatible."
                    << " Compressed updates should not be installed because"
                    << " the loader app is configured to use memory mapping"
                    << " and would not be able to load them.";
      return;
    }

    if (command_line.HasSwitch(loader_app::kLoaderTrackMemory)) {
      std::string period =
          command_line.GetSwitchValue(loader_app::kLoaderTrackMemory);
      if (period.empty()) {
        static loader_app::MemoryTrackerThread memory_tracker_thread;
        memory_tracker_thread.Start();
      } else {
        static loader_app::MemoryTrackerThread memory_tracker_thread(
            atoi(period.c_str()));
        memory_tracker_thread.Start();
      }
    }

    InstallCrashpadHandler(
        command_line.GetSwitchValue(elf_loader::kEvergreenContent));

    if (is_evergreen_lite) {
      loader_app::RecordSlotSelectionStatus(SlotSelectionStatus::kEGLite);
      LoadLibraryAndInitialize(alternative_content, use_memory_mapped_file);
    } else {
      std::string url = command_line.GetSwitchValue(loader_app::kURL);
      if (url.empty()) {
        url = kCobaltDefaultUrl;
      }
      std::string app_key = loader_app::GetAppKey(url);
      SB_CHECK(!app_key.empty());

      g_sb_event_func = reinterpret_cast<void (*)(const SbEvent*)>(
          loader_app::LoadSlotManagedLibrary(app_key, alternative_content,
                                             &g_cobalt_library_loader,
                                             use_memory_mapped_file));

      if (g_sb_event_func == NULL) {
        SB_LOG(ERROR) << "Failed to initialize Installation Manager. Loading "
                         "system image instead.";
        loader_app::RecordSlotSelectionStatus(
            SlotSelectionStatus::kLoadSysImgFailedToInitInstallationManager);
        LoadLibraryAndInitialize(alternative_content, use_memory_mapped_file);
      }
    }
    // If g_sb_event_func is NULL at this point, the app has no choice but to
    // crash.
    SB_CHECK(g_sb_event_func);
  }

  if (g_sb_event_func != NULL) {
    g_sb_event_func(event);
  }

  SB_CHECK_EQ(pthread_mutex_unlock(&mutex), 0);
}

int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
