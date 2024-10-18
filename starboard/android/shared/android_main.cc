// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <atomic>

#include "game-activity/GameActivity.h"
#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/log_internal.h"
#include "starboard/common/file.h"
#include "starboard/common/semaphore.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/event.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/command_line.h"
#include "starboard/thread.h"
#if SB_IS(EVERGREEN_COMPATIBLE)
#include "starboard/crashpad_wrapper/wrapper.h"  // nogncheck
#endif

namespace starboard {
namespace android {
namespace shared {

std::atomic_bool g_block_swapbuffers{false};

namespace {

using ::starboard::shared::starboard::CommandLine;
typedef ::starboard::android::shared::ApplicationAndroid::AndroidCommand
    AndroidCommand;

pthread_t g_starboard_thread = 0;
Semaphore* g_app_created_semaphore = nullptr;

// Safeguard to avoid sending AndroidCommands either when there is no instance
// of the Starboard application, or after the run loop has exited and the
// ALooper receiving the commands is no longer being polled.
std::atomic_bool g_app_running{false};

std::vector<std::string> GetArgs() {
  std::vector<std::string> args;
  // Fake program name as args[0]
  args.push_back(strdup("android_main"));

  JniEnvExt* env = JniEnvExt::Get();

  ScopedLocalJavaRef<jobjectArray> args_array(
      env->CallStarboardObjectMethodOrAbort("getArgs",
                                            "()[Ljava/lang/String;"));
  jint argc = !args_array ? 0 : env->GetArrayLength(args_array.Get());

  for (jint i = 0; i < argc; i++) {
    ScopedLocalJavaRef<jstring> element(
        env->GetObjectArrayElementOrAbort(args_array.Get(), i));
    args.push_back(env->GetStringStandardUTFOrAbort(element.Get()));
  }

  return args;
}

std::string GetStartDeepLink() {
  JniEnvExt* env = JniEnvExt::Get();
  std::string start_url;

  ScopedLocalJavaRef<jstring> j_url(env->CallStarboardObjectMethodOrAbort(
      "getStartDeepLink", "()Ljava/lang/String;"));
  if (j_url) {
    start_url = env->GetStringStandardUTFOrAbort(j_url.Get());
  }
  SB_LOG(INFO) << "GetStartDeepLink: " << start_url;
  return start_url;
}

#if SB_IS(EVERGREEN_COMPATIBLE)
bool CopyDirContents(const std::string& src_dir_path,
                     const std::string& dst_dir_path) {
  DIR* src_dir = opendir(src_dir_path.c_str());
  if (!src_dir) {
    SB_LOG(WARNING) << "Failed to open dir=" << src_dir_path;
    return false;
  }

  std::vector<char> filename_buffer(kSbFileMaxName);

  while (true) {
    if (filename_buffer.size() < kSbFileMaxName) {
      break;
    }

    if (!src_dir || !filename_buffer.data()) {
      break;
    }

    struct dirent dirent_buffer;
    struct dirent* dirent;
    int result = readdir_r(src_dir, &dirent_buffer, &dirent);
    if (result || !dirent) {
      break;
    }

    starboard::strlcpy(filename_buffer.data(), dirent->d_name,
                       filename_buffer.size());

    std::string filename(filename_buffer.begin(), filename_buffer.end());
    std::string path_to_src_file = src_dir_path + kSbFileSepString + filename;
    int src_file = open(path_to_src_file.c_str(), O_RDONLY, S_IRUSR | S_IWUSR);
    if (!IsValid(src_file)) {
      SB_LOG(WARNING) << "Failed to open file=" << path_to_src_file;
      return false;
    }

    struct stat info;
    if (fstat(src_file, &info)) {
      SB_LOG(WARNING) << "Failed to get info for file=" << path_to_src_file;
      close(src_file);
      return false;
    }

    int file_size = static_cast<int>(info.st_size);

    // Read in bytes from src file
    char file_contents_buffer[file_size];
    int read = ReadAll(src_file, file_contents_buffer, file_size);
    if (read == -1) {
      SB_LOG(WARNING) << "ReadAll failed for file=" << path_to_src_file;
      return false;
    }
    const std::string file_contents =
        std::string(file_contents_buffer, file_size);
    close(src_file);

    // Write bytes out to dst file
    std::string path_to_dst_file = dst_dir_path;
    path_to_dst_file.append(kSbFileSepString);
    path_to_dst_file.append(filename);
    int dst_file = open(path_to_dst_file.c_str(), O_CREAT | O_TRUNC | O_WRONLY,
                        S_IRUSR | S_IWUSR);
    if (!IsValid(dst_file)) {
      SB_LOG(WARNING) << "Failed to open file=" << path_to_dst_file;
      return false;
    }
    int wrote = WriteAll(dst_file, file_contents.c_str(), file_size);
    RecordFileWriteStat(wrote);
    if (wrote == -1) {
      SB_LOG(WARNING) << "WriteAll failed for file=" << path_to_dst_file;
      return false;
    }
    close(dst_file);
  }

  closedir(src_dir);
  return true;
}

// Extracts CA certificates from the APK to the file system and returns the path
// to the directory containing the extracted certifictes, or an empty string on
// error.
std::string ExtractCertificatesToFileSystem() {
  std::vector<char> apk_path_buffer(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathContentDirectory, apk_path_buffer.data(),
                       apk_path_buffer.size())) {
    SB_LOG(WARNING) << "Failed to get path to content dir in APK";
    return "";
  }

  std::string apk_path(apk_path_buffer.data());
  apk_path.append(std::string(kSbFileSepString) + "app" + kSbFileSepString +
                  "cobalt" + kSbFileSepString + "content" + kSbFileSepString +
                  "ssl" + kSbFileSepString + "certs");
  struct stat info;
  if (stat(apk_path.c_str(), &info) != 0) {
    SB_LOG(WARNING) << "CA certificates directory not found in APK";
    return "";
  }

  std::vector<char> file_system_path_buffer(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory,
                       file_system_path_buffer.data(),
                       file_system_path_buffer.size())) {
    SB_LOG(WARNING) << "Failed to get path to cache dir on file system";
    return "";
  }

  std::string file_system_path(file_system_path_buffer.data());
  file_system_path.append(std::string(kSbFileSepString) + "certs");
  struct stat fileinfo;
  if (mkdir(file_system_path.c_str(), 0700) &&
      !(stat(file_system_path.c_str(), &fileinfo) == 0 &&
        S_ISDIR(fileinfo.st_mode))) {
    SB_LOG(WARNING) << "Failed to create new dir for CA certificates";
    return "";
  }

  if (!CopyDirContents(apk_path, file_system_path)) {
    SB_LOG(WARNING) << "Failed to copy CA certificates to the file system";
    return "";
  }

  return file_system_path;
}

void InstallCrashpadHandler(const CommandLine& command_line) {
  std::string extracted_ca_certificates_path =
      ExtractCertificatesToFileSystem();
  if (extracted_ca_certificates_path.empty()) {
    SB_LOG(WARNING) << "Failed to extract CA certificates to file system, not "
                    << "installing Crashpad handler";
    return;
  }

  third_party::crashpad::wrapper::InstallCrashpadHandler(
      extracted_ca_certificates_path);
}
#endif  // SB_IS(EVERGREEN_COMPATIBLE)

void* ThreadEntryPoint(void* context) {
  pthread_setname_np(pthread_self(), "StarboardMain");
  g_app_created_semaphore = static_cast<Semaphore*>(context);

  int unused_value = -1;
  int error_level = SbRunStarboardMain(unused_value, nullptr, nullptr);

  // Our launcher.py looks for this to know when the app (test) is done.
  SB_LOG(INFO) << "***Application Stopped*** " << error_level;

  // Inform StarboardBridge that the run loop has exited so it can cleanup and
  // kill the process.
  JniEnvExt* env = JniEnvExt::Get();
  env->CallStarboardVoidMethodOrAbort("afterStopped", "()V");

  return NULL;
}

void OnStart(GameActivity* activity) {
  if (g_app_running.load()) {
    ApplicationAndroid::Get()->SendAndroidCommand(AndroidCommand::kStart);
  }
}

void OnResume(GameActivity* activity) {
  if (g_app_running.load()) {
    ApplicationAndroid::Get()->SendAndroidCommand(AndroidCommand::kResume);
  }
}

void OnPause(GameActivity* activity) {
  if (g_app_running.load()) {
    ApplicationAndroid::Get()->SendAndroidCommand(AndroidCommand::kPause);
  }
}

void OnStop(GameActivity* activity) {
  if (g_app_running.load()) {
    ApplicationAndroid::Get()->SendAndroidCommand(AndroidCommand::kStop);
  }
}

bool OnTouchEvent(GameActivity* activity,
                  const GameActivityMotionEvent* event) {
  if (g_app_running.load()) {
    return ApplicationAndroid::Get()->SendAndroidMotionEvent(event);
  }
  return false;
}

bool OnKey(GameActivity* activity, const GameActivityKeyEvent* event) {
  if (g_app_running.load()) {
    return ApplicationAndroid::Get()->SendAndroidKeyEvent(event);
  }
  return false;
}

void OnWindowFocusChanged(GameActivity* activity, bool focused) {
  if (g_app_running.load()) {
    ApplicationAndroid::Get()->SendAndroidCommand(
        focused ? AndroidCommand::kWindowFocusGained
                : AndroidCommand::kWindowFocusLost);
  }
}

void OnNativeWindowCreated(GameActivity* activity, ANativeWindow* window) {
  g_block_swapbuffers.store(false);
  if (g_app_running.load()) {
    ApplicationAndroid::Get()->SendAndroidCommand(
        AndroidCommand::kNativeWindowCreated, window);
  }
}

void OnNativeWindowDestroyed(GameActivity* activity, ANativeWindow* window) {
  g_block_swapbuffers.store(true);
  if (g_app_running.load()) {
    ApplicationAndroid::Get()->SendAndroidCommand(
        AndroidCommand::kNativeWindowDestroyed);
  }
}

extern "C" SB_EXPORT_PLATFORM void GameActivity_onCreate(
    GameActivity* activity,
    void* savedState,
    size_t savedStateSize) {
  // Start the Starboard thread the first time an Activity is created.
  if (g_starboard_thread == 0) {
    Semaphore semaphore;

    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);

    pthread_create(&g_starboard_thread, &attributes, &ThreadEntryPoint,
                   &semaphore);

    pthread_attr_destroy(&attributes);

    // Wait for the ApplicationAndroid to be created.
    semaphore.Take();
  }

  activity->callbacks->onStart = OnStart;
  activity->callbacks->onResume = OnResume;
  activity->callbacks->onPause = OnPause;
  activity->callbacks->onStop = OnStop;
  activity->callbacks->onTouchEvent = OnTouchEvent;
  activity->callbacks->onKeyDown = OnKey;
  activity->callbacks->onKeyUp = OnKey;
  activity->callbacks->onWindowFocusChanged = OnWindowFocusChanged;
  activity->callbacks->onNativeWindowCreated = OnNativeWindowCreated;
  activity->callbacks->onNativeWindowDestroyed = OnNativeWindowDestroyed;

  activity->instance = ApplicationAndroid::Get();
}

extern "C" SB_EXPORT_PLATFORM jboolean
Java_dev_cobalt_coat_StarboardBridge_nativeIsReleaseBuild() {
#if defined(COBALT_BUILD_TYPE_GOLD)
  return true;
#else
  return false;
#endif
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_StarboardBridge_nativeInitialize(
    JniEnvExt* env,
    jobject starboard_bridge) {
  JniEnvExt::Initialize(env, starboard_bridge);
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_VolumeStateReceiver_nativeVolumeChanged(JNIEnv* env,
                                                             jobject jcaller,
                                                             jint volumeDelta) {
  if (g_app_running.load()) {
    SbKey key =
        volumeDelta > 0 ? SbKey::kSbKeyVolumeUp : SbKey::kSbKeyVolumeDown;
    ApplicationAndroid::Get()->SendKeyboardInject(key);
  }
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_VolumeStateReceiver_nativeMuteChanged(JNIEnv* env,
                                                           jobject jcaller) {
  if (g_app_running.load()) {
    ApplicationAndroid::Get()->SendKeyboardInject(SbKey::kSbKeyVolumeMute);
  }
}

}  // namespace

extern "C" int SbRunStarboardMain(int argc,
                                  char** argv,
                                  SbEventHandleCallback callback) {
  ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
  ApplicationAndroid app(looper, callback);

  CommandLine command_line(GetArgs());
  LogInit(command_line);

#if SB_IS(EVERGREEN_COMPATIBLE)
  InstallCrashpadHandler(command_line);
#endif  // SB_IS(EVERGREEN_COMPATIBLE)

  // Mark the app running before signaling app created so there's no race to
  // allow sending the first AndroidCommand after onCreate() returns.
  g_app_running.store(true);

  // Signal GameActivity_onCreate() that it may proceed.
  g_app_created_semaphore->Put();

  // Enter the Starboard run loop until stopped.
  int error_level =
      app.Run(std::move(command_line), GetStartDeepLink().c_str());

  // Mark the app not running before informing StarboardBridge that the app is
  // stopped so that we won't send any more AndroidCommands as a result of
  // shutting down the Activity.
  g_app_running.store(false);

  return error_level;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
