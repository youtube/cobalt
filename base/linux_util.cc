// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/linux_util.h"

#include <dirent.h>
#include <errno.h>
#include <glib.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vector>

#include "base/command_line.h"
#include "base/lock.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/third_party/xdg_user_dirs/xdg_user_dir_lookup.h"

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

// expected prefix of the target of the /proc/self/fd/%d link for a socket
static const char kSocketLinkPrefix[] = "socket:[";

// Parse a symlink in /proc/pid/fd/$x and return the inode number of the
// socket.
//   inode_out: (output) set to the inode number on success
//   path: e.g. /proc/1234/fd/5 (must be a UNIX domain socket descriptor)
//   log: if true, log messages about failure details
bool ProcPathGetInode(ino_t* inode_out, const char* path, bool log = false) {
  DCHECK(inode_out);
  DCHECK(path);

  char buf[256];
  const ssize_t n = readlink(path, buf, sizeof(buf) - 1);
  if (n == -1) {
    if (log) {
      LOG(WARNING) << "Failed to read the inode number for a socket from /proc"
                      "(" << errno << ")";
    }
    return false;
  }
  buf[n] = 0;

  if (memcmp(kSocketLinkPrefix, buf, sizeof(kSocketLinkPrefix) - 1)) {
    if (log) {
      LOG(WARNING) << "The descriptor passed from the crashing process wasn't a"
                      " UNIX domain socket.";
    }
    return false;
  }

  char *endptr;
  const unsigned long long int inode_ul =
      strtoull(buf + sizeof(kSocketLinkPrefix) - 1, &endptr, 10);
  if (*endptr != ']')
    return false;

  if (inode_ul == ULLONG_MAX) {
    if (log) {
      LOG(WARNING) << "Failed to parse a socket's inode number: the number was "
                      "too large. Please report this bug: " << buf;
    }
    return false;
  }

  *inode_out = inode_ul;
  return true;
}

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

FilePath GetHomeDir(EnvironmentVariableGetter* env) {
  std::string home_dir;
  if (env->Getenv("HOME", &home_dir) && !home_dir.empty())
    return FilePath(home_dir);

  home_dir = g_get_home_dir();
  if (!home_dir.empty())
    return FilePath(home_dir);

  FilePath rv;
  if (PathService::Get(base::DIR_TEMP, &rv))
    return rv;

  // Last resort.
  return FilePath("/tmp");
}

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

FilePath GetXDGDirectory(EnvironmentVariableGetter* env,
                         const char* env_name, const char* fallback_dir) {
  std::string env_value;
  if (env->Getenv(env_name, &env_value) && !env_value.empty())
    return FilePath(env_value);
  return GetHomeDir(env).Append(fallback_dir);
}

FilePath GetXDGUserDirectory(EnvironmentVariableGetter* env,
                             const char* dir_name, const char* fallback_dir) {
  char* xdg_dir = xdg_user_dir_lookup(dir_name);
  if (xdg_dir) {
    FilePath rv(xdg_dir);
    free(xdg_dir);
    return rv;
  }
  return GetHomeDir(env).Append(fallback_dir);
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
    else if (desktop_session == "kde") {
      // This may mean KDE4 on newer systems, so we have to check.
      std::string dummy;
      if (env->Getenv("KDE_SESSION_VERSION", &dummy))
        return DESKTOP_ENVIRONMENT_KDE4;
      return DESKTOP_ENVIRONMENT_KDE3;
    }
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

bool FileDescriptorGetInode(ino_t* inode_out, int fd) {
  DCHECK(inode_out);

  struct stat buf;
  if (fstat(fd, &buf) < 0)
    return false;

  if (!S_ISSOCK(buf.st_mode))
    return false;

  *inode_out = buf.st_ino;
  return true;
}

bool FindProcessHoldingSocket(pid_t* pid_out, ino_t socket_inode) {
  DCHECK(pid_out);
  bool already_found = false;

  DIR* proc = opendir("/proc");
  if (!proc) {
    LOG(WARNING) << "Cannot open /proc";
    return false;
  }

  std::vector<pid_t> pids;

  struct dirent* dent;
  while ((dent = readdir(proc))) {
    char *endptr;
    const unsigned long int pid_ul = strtoul(dent->d_name, &endptr, 10);
    if (pid_ul == ULONG_MAX || *endptr)
      continue;
    pids.push_back(pid_ul);
  }
  closedir(proc);

  for (std::vector<pid_t>::const_iterator
       i = pids.begin(); i != pids.end(); ++i) {
    const pid_t current_pid = *i;
    char buf[256];
    snprintf(buf, sizeof(buf), "/proc/%d/fd", current_pid);
    DIR* fd = opendir(buf);
    if (!fd)
      continue;

    while ((dent = readdir(fd))) {
      if (snprintf(buf, sizeof(buf), "/proc/%d/fd/%s", current_pid,
                   dent->d_name) >= static_cast<int>(sizeof(buf))) {
        continue;
      }

      ino_t fd_inode;
      if (ProcPathGetInode(&fd_inode, buf)) {
        if (fd_inode == socket_inode) {
          if (already_found) {
            closedir(fd);
            return false;
          }

          already_found = true;
          *pid_out = current_pid;
          break;
        }
      }
    }

    closedir(fd);
  }

  return already_found;
}

}  // namespace base
