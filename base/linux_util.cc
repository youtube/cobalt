// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/linux_util.h"

#include <stdlib.h>

#include <vector>

#include "base/command_line.h"
#include "base/lock.h"
#include "base/process_util.h"
#include "base/singleton.h"
#include "base/string_util.h"

namespace {

class EnvironmentVariableGetterImpl
    : public base::EnvironmentVariableGetter {
 public:
  virtual bool Getenv(const char* variable_name, std::string* result) {
    const char* env_value = ::getenv(variable_name);
    if (env_value) {
      // Note that the variable may be defined but empty.
      *result = env_value;
      return true;
    }
    // Some commonly used variable names are uppercase while others
    // are lowercase, which is inconsistent. Let's try to be helpful
    // and look for a variable name with the reverse case.
    char first_char = variable_name[0];
    std::string alternate_case_var;
    if (first_char >= 'a' && first_char <= 'z')
      alternate_case_var = StringToUpperASCII(std::string(variable_name));
    else if (first_char >= 'A' && first_char <= 'Z')
      alternate_case_var = StringToLowerASCII(std::string(variable_name));
    else
      return false;
    env_value = ::getenv(alternate_case_var.c_str());
    if (env_value) {
      *result = env_value;
      return true;
    }
    return false;
  }
};

// Not needed for OS_CHROMEOS.
#if defined(OS_LINUX)
enum LinuxDistroState {
  STATE_DID_NOT_CHECK  = 0,
  STATE_CHECK_STARTED  = 1,
  STATE_CHECK_FINISHED = 2,
};

// Helper class for GetLinuxDistro().
class LinuxDistroHelper {
 public:
  // Retrieves the Singleton.
  static LinuxDistroHelper* Get() {
    return Singleton<LinuxDistroHelper>::get();
  }

  // The simple state machine goes from:
  // STATE_DID_NOT_CHECK -> STATE_CHECK_STARTED -> STATE_CHECK_FINISHED.
  LinuxDistroHelper() : state_(STATE_DID_NOT_CHECK) {}
  ~LinuxDistroHelper() {}

  // Retrieve the current state, if we're in STATE_DID_NOT_CHECK,
  // we automatically move to STATE_CHECK_STARTED so nobody else will
  // do the check.
  LinuxDistroState State() {
    AutoLock scoped_lock(lock_);
    if (STATE_DID_NOT_CHECK == state_) {
      state_ = STATE_CHECK_STARTED;
      return STATE_DID_NOT_CHECK;
    }
    return state_;
  }

  // Indicate the check finished, move to STATE_CHECK_FINISHED.
  void CheckFinished() {
    AutoLock scoped_lock(lock_);
    DCHECK(state_ == STATE_CHECK_STARTED);
    state_ = STATE_CHECK_FINISHED;
  }

 private:
  Lock lock_;
  LinuxDistroState state_;
};
#endif  // if defined(OS_LINUX)

}  // anonymous namespace

namespace base {

uint8_t* BGRAToRGBA(const uint8_t* pixels, int width, int height, int stride) {
  if (stride == 0)
    stride = width * 4;

  uint8_t* new_pixels = static_cast<uint8_t*>(malloc(height * stride));

  // We have to copy the pixels and swap from BGRA to RGBA.
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      int idx = i * stride + j * 4;
      new_pixels[idx] = pixels[idx + 2];
      new_pixels[idx + 1] = pixels[idx + 1];
      new_pixels[idx + 2] = pixels[idx];
      new_pixels[idx + 3] = pixels[idx + 3];
    }
  }

  return new_pixels;
}

// We use this static string to hold the Linux distro info. If we
// crash, the crash handler code will send this in the crash dump.
std::string linux_distro =
#if defined(OS_CHROMEOS)
    "CrOS";
#else  // if defined(OS_LINUX)
    "Unknown";
#endif

std::string GetLinuxDistro() {
#if defined(OS_CHROMEOS)
  return linux_distro;
#else  // if defined(OS_LINUX)
  LinuxDistroHelper* distro_state_singleton = LinuxDistroHelper::Get();
  LinuxDistroState state = distro_state_singleton->State();
  if (STATE_DID_NOT_CHECK == state) {
    // We do this check only once per process. If it fails, there's
    // little reason to believe it will work if we attempt to run
    // lsb_release again.
    std::vector<std::string> argv;
    argv.push_back("lsb_release");
    argv.push_back("-d");
    std::string output;
    base::GetAppOutput(CommandLine(argv), &output);
    if (output.length() > 0) {
      // lsb_release -d should return: Description:<tab>Distro Info
      static const std::string field = "Description:\t";
      if (output.compare(0, field.length(), field) == 0) {
        linux_distro = output.substr(field.length());
        TrimWhitespaceASCII(linux_distro, TRIM_ALL, &linux_distro);
      }
    }
    distro_state_singleton->CheckFinished();
    return linux_distro;
  } else if (STATE_CHECK_STARTED == state) {
    // If the distro check above is in progress in some other thread, we're
    // not going to wait for the results.
    return "Unknown";
  } else {
    // In STATE_CHECK_FINISHED, no more writing to |linux_distro|.
    return linux_distro;
  }
#endif
}

// static
EnvironmentVariableGetter* EnvironmentVariableGetter::Create() {
  return new EnvironmentVariableGetterImpl();
}

DesktopEnvironment GetDesktopEnvironment(EnvironmentVariableGetter* env) {
  std::string desktop_session;
  if (env->Getenv("DESKTOP_SESSION", &desktop_session)) {
    if (desktop_session == "gnome")
      return DESKTOP_ENVIRONMENT_GNOME;
    else if (desktop_session == "kde4")
      return DESKTOP_ENVIRONMENT_KDE4;
    else if (desktop_session == "kde")
      return DESKTOP_ENVIRONMENT_KDE3;
  }

  // Fall back on some older environment variables.
  // Useful particularly in the DESKTOP_SESSION=default case.
  std::string dummy;
  if (env->Getenv("GNOME_DESKTOP_SESSION_ID", &dummy)) {
    return DESKTOP_ENVIRONMENT_GNOME;
  } else if (env->Getenv("KDE_FULL_SESSION", &dummy)) {
    if (env->Getenv("KDE_SESSION_VERSION", &dummy))
      return DESKTOP_ENVIRONMENT_KDE4;
    return DESKTOP_ENVIRONMENT_KDE3;
  }

  return DESKTOP_ENVIRONMENT_OTHER;
}

const char* GetDesktopEnvironmentName(DesktopEnvironment env) {
  switch (env) {
    case DESKTOP_ENVIRONMENT_OTHER:
      return NULL;
    case DESKTOP_ENVIRONMENT_GNOME:
      return "GNOME";
    case DESKTOP_ENVIRONMENT_KDE3:
      return "KDE3";
    case DESKTOP_ENVIRONMENT_KDE4:
      return "KDE4";
  }
  return NULL;
}

const char* GetDesktopEnvironmentName(EnvironmentVariableGetter* env) {
  return GetDesktopEnvironmentName(GetDesktopEnvironment(env));
}

}  // namespace base
