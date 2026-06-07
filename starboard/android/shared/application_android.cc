// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/application_android.h"

#include <android/looper.h>
#include <android/native_activity.h>
#include <jni.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#include "base/android/jni_string.h"
#include "cobalt/android/jni_headers/CobaltSystemConfigChangeReceiver_jni.h"
#include "cobalt/android/jni_headers/HTMLMediaElementExtension_jni.h"
#include "partition_alloc/partition_stats.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "starboard/android/shared/file_internal.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/android/shared/window_internal.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/event.h"
#include "starboard/extension/accessibility.h"
#include "starboard/key.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using jni_zero::AttachCurrentThread;
using jni_zero::JavaParamRef;
using jni_zero::ScopedJavaGlobalRef;
using jni_zero::ScopedJavaLocalRef;

// TODO(cobalt, b/378708359): Remove this dummy init.
void stubSbEventHandle(const SbEvent* event) {
  SB_LOG(ERROR) << "Starboard event DISCARDED:" << event->type;
}

static std::atomic<bool> g_stop_thread{false};
static std::thread g_logging_thread;
static int64_t GetSelfPssKb() {
  std::ifstream file("/proc/self/smaps_rollup");
  if (!file.is_open()) {
    return 0;
  }
  std::string line;
  while (std::getline(file, line)) {
    if (line.compare(0, 4, "Pss:") == 0) {
      size_t start = line.find_first_of("0123456789");
      if (start != std::string::npos) {
        size_t end = line.find_first_not_of("0123456789", start);
        std::string pss_str = line.substr(start, end - start);
        return std::strtoll(pss_str.c_str(), nullptr, 10);
      }
    }
  }
  return 0;
}
struct AndroidMemoryInfo {
  int dalvik_pss_kb = 0;
  int native_pss_kb = 0;
  int other_pss_kb = 0;
  int graphics_pss_kb = 0;
  int code_pss_kb = 0;
  int dalvik_other_pss_kb = 0;
};

static AndroidMemoryInfo GetAndroidMemoryInfo() {
  AndroidMemoryInfo info;
  JNIEnv* env = AttachCurrentThread();
  if (!env) {
    return info;
  }

  // Static caching of classes, methods, and fields to minimize JNI overhead
  static ScopedJavaGlobalRef<jclass>* debug_class = nullptr;
  static ScopedJavaGlobalRef<jclass>* mem_info_class = nullptr;
  static jmethodID mem_info_init = nullptr;
  static jmethodID get_mem_info_method = nullptr;
  static jfieldID dalvik_field = nullptr;
  static jfieldID native_field = nullptr;
  static jfieldID other_field = nullptr;
  static jmethodID get_memory_stat_method = nullptr;
  static jmethodID get_other_private_dirty_method = nullptr;

  if (debug_class == nullptr) {
    jclass local_debug = env->FindClass("android/os/Debug");
    debug_class = new ScopedJavaGlobalRef<jclass>(env, local_debug);

    jclass local_mem_info = env->FindClass("android/os/Debug$MemoryInfo");
    mem_info_class = new ScopedJavaGlobalRef<jclass>(env, local_mem_info);

    mem_info_init = env->GetMethodID(mem_info_class->obj(), "<init>", "()V");
    get_mem_info_method =
        env->GetStaticMethodID(debug_class->obj(), "getMemoryInfo",
                               "(Landroid/os/Debug$MemoryInfo;)V");

    dalvik_field = env->GetFieldID(mem_info_class->obj(), "dalvikPss", "I");
    native_field = env->GetFieldID(mem_info_class->obj(), "nativePss", "I");
    other_field = env->GetFieldID(mem_info_class->obj(), "otherPss", "I");

    get_memory_stat_method =
        env->GetMethodID(mem_info_class->obj(), "getMemoryStat",
                         "(Ljava/lang/String;)Ljava/lang/String;");
    get_other_private_dirty_method =
        env->GetMethodID(mem_info_class->obj(), "getOtherPrivateDirty", "(I)I");
  }

  if (mem_info_class->is_null() || !mem_info_init) {
    return info;
  }

  ScopedJavaLocalRef<jobject> mem_info_obj(
      env, env->NewObject(mem_info_class->obj(), mem_info_init));
  if (mem_info_obj.is_null()) {
    return info;
  }

  env->CallStaticVoidMethod(debug_class->obj(), get_mem_info_method,
                            mem_info_obj.obj());

  if (dalvik_field) {
    info.dalvik_pss_kb = env->GetIntField(mem_info_obj.obj(), dalvik_field);
  }
  if (native_field) {
    info.native_pss_kb = env->GetIntField(mem_info_obj.obj(), native_field);
  }
  if (other_field) {
    info.other_pss_kb = env->GetIntField(mem_info_obj.obj(), other_field);
  }

  if (get_memory_stat_method) {
    auto get_stat = [&](const char* stat_name) -> int {
      ScopedJavaLocalRef<jstring> j_stat_name =
          base::android::ConvertUTF8ToJavaString(env, stat_name);
      ScopedJavaLocalRef<jstring> j_stat_val(
          env,
          static_cast<jstring>(env->CallObjectMethod(
              mem_info_obj.obj(), get_memory_stat_method, j_stat_name.obj())));
      if (j_stat_val.is_null()) {
        return 0;
      }
      std::string stat_val =
          base::android::ConvertJavaStringToUTF8(env, j_stat_val.obj());
      return std::atoi(stat_val.c_str());
    };

    info.graphics_pss_kb = get_stat("summary.graphics");
    info.code_pss_kb = get_stat("summary.code");
  }

  if (get_other_private_dirty_method) {
    info.dalvik_other_pss_kb =
        env->CallIntMethod(mem_info_obj.obj(), get_other_private_dirty_method,
                           0) +  // Dalvik Other
        env->CallIntMethod(mem_info_obj.obj(), get_other_private_dirty_method,
                           3) +  // Ashmem
        env->CallIntMethod(mem_info_obj.obj(), get_other_private_dirty_method,
                           13);  // ART mmap
  }

  return info;
}

extern int g_allocated_mib;
extern "C" int g_skia_cache_mib;

static void LogPartitionAllocStats() {
  auto Format = [](int64_t number) {
    return starboard::FormatWithDigitSeparators(number);
  };
  auto* root = allocator_shim::internal::PartitionAllocMalloc::Allocator();
  if (root) {
    partition_alloc::SimplePartitionStatsDumper dumper;
    root->DumpStats("main", /*is_light_dump=*/true, &dumper);
    const auto& stats = dumper.stats();
    double frag_ratio = 0.0;
    if (stats.total_committed_bytes > 0) {
      double committed = static_cast<double>(stats.total_committed_bytes);
      double allocated = static_cast<double>(stats.total_allocated_bytes);
      if (committed > allocated) {
        frag_ratio = (committed - allocated) / committed;
      }
    }
    AndroidMemoryInfo pss_info = GetAndroidMemoryInfo();
    SB_LOG(INFO) << "PA_STATS: mmapped=" << Format(stats.total_mmapped_bytes)
                 << ", committed=" << Format(stats.total_committed_bytes)
                 << ", allocated=" << Format(stats.total_allocated_bytes)
                 << ", frag_ratio=" << frag_ratio
                 << ", decoder_buffer(mib)=" << g_allocated_mib
                 << ", skia_cache(mib)=" << g_skia_cache_mib;
    SB_LOG(INFO) << "PSS_STATS (KiB): total=" << Format(GetSelfPssKb())
                 << ", native=" << Format(pss_info.native_pss_kb)
                 << ", dalvik=" << Format(pss_info.dalvik_pss_kb)
                 << ", graphics=" << Format(pss_info.graphics_pss_kb)
                 << ", code=" << Format(pss_info.code_pss_kb)
                 << ", other=" << Format(pss_info.other_pss_kb)
                 << ", dalvik_other=" << Format(pss_info.dalvik_other_pss_kb);
  }
}
static void MemoryLoggingThreadLoop() {
  while (!g_stop_thread.load(std::memory_order_relaxed)) {
    LogPartitionAllocStats();
    for (int i = 0; i < 50; ++i) {
      if (g_stop_thread.load(std::memory_order_relaxed)) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}

ApplicationAndroid::ApplicationAndroid(
    std::unique_ptr<CommandLine> command_line,
    ScopedJavaGlobalRef<jobject> asset_manager,
    const std::string& files_dir,
    const std::string& cache_dir,
    const std::string& native_library_dir)
    : Application(stubSbEventHandle) {
  starboard::StarboardBridge::GetInstance()->SetStartupMilestone(6);
  SetCommandLine(std::move(command_line));
  // Initialize Time Zone early so that local time works correctly.
  // Called once here to help SbTimeZoneGet*Name()
  tzset();

  // Initialize Android asset access.
  SbFileAndroidInitialize(asset_manager, files_dir, cache_dir,
                          native_library_dir);

  // This effectively initializes the singleton and caches all RRO settings, if
  // they haven't yet be cached by other users of the RuntimeResourceOverlay
  // class.
  RuntimeResourceOverlay::GetInstance();

  JNIEnv* jni_env = AttachCurrentThread();
  app_start_timestamp_ = starboard_bridge_->GetAppStartTimestamp(jni_env);

  starboard_bridge_->ApplicationStarted(jni_env);
  // Start the periodic memory logging loop (every 5 seconds)
  g_stop_thread.store(false, std::memory_order_relaxed);
  g_logging_thread = std::thread(&MemoryLoggingThreadLoop);
}

ApplicationAndroid::~ApplicationAndroid() {
  // Stop and join the logging thread
  g_stop_thread.store(true, std::memory_order_relaxed);
  if (g_logging_thread.joinable()) {
    g_logging_thread.join();
  }
  JNIEnv* env = AttachCurrentThread();
  starboard_bridge_->ApplicationStopping(env);
}

void JNI_CobaltSystemConfigChangeReceiver_DateTimeConfigurationChanged(
    JNIEnv* env) {
  // TODO(cobalt, b/378705729): Make sure tzset() is called on the right thread.
  // Set the timezone to allow SbTimeZoneGetName() to return updated timezone.
  tzset();
}

ScopedJavaLocalRef<jstring> JNI_HTMLMediaElementExtension_CanPlayType(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_mime_type,
    const JavaParamRef<jstring>& j_key_system) {
  std::string mime_type, key_system;
  if (j_mime_type) {
    mime_type = ConvertJavaStringToUTF8(env, j_mime_type);
  }
  if (j_key_system) {
    key_system = ConvertJavaStringToUTF8(env, j_key_system);
  }
  SbMediaSupportType support_type =
      SbMediaCanPlayMimeAndKeySystem(mime_type.c_str(), key_system.c_str());
  const char* ret;
  switch (support_type) {
    case kSbMediaSupportTypeNotSupported:
      ret = "";
      break;
    case kSbMediaSupportTypeMaybe:
      ret = "maybe";
      break;
    case kSbMediaSupportTypeProbably:
      ret = "probably";
      break;
  }
  SB_LOG(INFO) << __func__ << " (" << mime_type << ", " << key_system
               << ") --> " << ret;
  return ConvertUTF8ToJavaString(env, ret);
}

Application::Event* ApplicationAndroid::GetNextEvent() {
  SB_LOG(FATAL) << __func__
                << " should not be called since Android doesn't utilize "
                   "Starboard's event handling";
  return nullptr;
}

void ApplicationAndroid::Inject(Application::Event* event) {
  SB_LOG(FATAL) << __func__
                << " should not be called since Android doesn't utilize "
                   "Starboard's event handling";
}

void ApplicationAndroid::InjectTimedEvent(
    Application::TimedEvent* timed_event) {
  SB_LOG(FATAL) << __func__
                << " should not be called since Android doesn't utilize "
                   "Starboard's event handling";
}

void ApplicationAndroid::CancelTimedEvent(SbEventId event_id) {
  SB_LOG(FATAL) << __func__
                << " should not be called since Android doesn't utilize "
                   "Starboard's event handling";
}

Application::TimedEvent* ApplicationAndroid::GetNextDueTimedEvent() {
  SB_LOG(FATAL) << __func__
                << " should not be called since Android doesn't utilize "
                   "Starboard's event handling";
  return nullptr;
}

int64_t ApplicationAndroid::GetNextTimedEventTargetTime() {
  SB_LOG(FATAL) << __func__
                << " should not be called since Android doesn't utilize "
                   "Starboard's event handling";
  return std::numeric_limits<int64_t>::max();
}

}  // namespace starboard
