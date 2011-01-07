// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This command-line program dumps the contents of a set of cache files, either
// to stdout or to another set of cache files.

#include <stdio.h>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/win/scoped_handle.h"
#include "net/disk_cache/disk_format.h"

enum Errors {
  GENERIC = -1,
  ALL_GOOD = 0,
  INVALID_ARGUMENT = 1,
  FILE_ACCESS_ERROR,
  UNKNOWN_VERSION,
  TOOL_NOT_FOUND,
};

int GetMajorVersion(const std::wstring& input_path);
int DumpContents(const std::wstring& input_path);
int DumpHeaders(const std::wstring& input_path);
int RunSlave(const std::wstring& input_path, const std::wstring& pipe_number);
int CopyCache(const std::wstring& output_path, HANDLE pipe, bool copy_to_text);
HANDLE CreateServer(std::wstring* pipe_number);

const char kUpgradeHelp[] =
    "\nIn order to use the upgrade function, a version of this tool that\n"
    "understands the file format of the files to upgrade is needed. For\n"
    "instance, to upgrade files saved with file format 3.4 to version 5.2,\n"
    "a version of this program that was compiled with version 3.4 has to be\n"
    "located beside this executable, and named dump_cache_3.exe, and this\n"
    "executable should be compiled with version 5.2 being the current one.";

// Folders to read and write cache files.
const char kInputPath[] = "input";
const char kOutputPath[] = "output";

// Dumps the file headers to stdout.
const char kDumpHeaders[] = "dump-headers";

// Dumps all entries to stdout.
const char kDumpContents[] = "dump-contents";

// Convert the cache to files.
const char kDumpToFiles[] = "dump-to-files";

// Upgrade an old version to the current one.
const char kUpgrade[] = "upgrade";

// Internal use:
const char kSlave[] = "slave";
const char kPipe[] = "pipe";

int Help() {
  printf("warning: input files are modified by this tool\n");
  printf("dump_cache --input=path1 [--output=path2]\n");
  printf("--dump-headers: display file headers\n");
  printf("--dump-contents: display all entries\n");
  printf("--upgrade: copy contents to the output path\n");
  printf("--dump-to-files: write the contents of the cache to files\n");
  return INVALID_ARGUMENT;
}

// Starts a new process, to generate the files.
int LaunchSlave(const CommandLine& command_line,
                const std::wstring& pipe_number, int version) {
  // TODO(port): remove this string-munging hackery.
  std::wstring hacked_command_line = command_line.command_line_string();
  const std::wstring old_exe(L"dump_cache");
  size_t to_remove = hacked_command_line.find(old_exe);
  hacked_command_line.erase(to_remove, old_exe.size());

  bool do_upgrade = command_line.HasSwitch(kUpgrade);
  bool do_convert_to_text = command_line.HasSwitch(kDumpToFiles);

  std::wstring new_program;
  if (do_upgrade)
    new_program = base::StringPrintf(L"%ls%d", L"dump_cache_", version);
  else
    new_program = base::StringPrintf(L"dump_cache");

  hacked_command_line.insert(to_remove, new_program);

  CommandLine new_command_line = CommandLine::FromString(hacked_command_line);

  if (do_upgrade || do_convert_to_text)
    new_command_line.AppendSwitch(kSlave);

  // TODO(evanm): remove needless usage of wstring from here and elsewhere.
  new_command_line.AppendSwitchASCII(kPipe, WideToASCII(pipe_number));
  if (!base::LaunchApp(new_command_line, false, false, NULL)) {
    printf("Unable to launch the needed version of this tool: %ls\n",
           new_program.c_str());
    printf(kUpgradeHelp);
    return TOOL_NOT_FOUND;
  }
  return ALL_GOOD;
}

// -----------------------------------------------------------------------

int main(int argc, const char* argv[]) {
  // Setup an AtExitManager so Singleton objects will be destroyed.
  base::AtExitManager at_exit_manager;

  CommandLine::Init(argc, argv);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::wstring input_path = command_line.GetSwitchValueNative(kInputPath);
  if (input_path.empty())
    return Help();

  bool upgrade = false;
  bool slave_required = false;
  bool copy_to_text = false;
  // TODO(evanm): port to FilePath.
  std::wstring output_path = command_line.GetSwitchValueNative(kOutputPath);
  // Make sure that output directory ends with a slash.
  if (output_path.size() >= 1 && output_path[output_path.size() - 1] != '\\')
    output_path.push_back('\\');

  if (command_line.HasSwitch(kUpgrade))
    upgrade = true;
  if (command_line.HasSwitch(kDumpToFiles))
    copy_to_text = true;

  if (upgrade || copy_to_text) {
    if (output_path.empty())
      return Help();
    slave_required = true;
  }

  int version = GetMajorVersion(input_path);
  if (!version)
    return FILE_ACCESS_ERROR;

  if (version != disk_cache::kCurrentVersion >> 16) {
    if (command_line.HasSwitch(kSlave)) {
      printf("Unknown version\n");
      return UNKNOWN_VERSION;
    }
    slave_required = true;
  }

  std::wstring pipe_number = command_line.GetSwitchValueNative(kPipe);
  if (command_line.HasSwitch(kSlave) && slave_required)
    return RunSlave(input_path, pipe_number);

  base::win::ScopedHandle server;
  if (slave_required) {
    server.Set(CreateServer(&pipe_number));
    if (!server.IsValid()) {
      printf("Unable to create the server pipe\n");
      return -1;
    }

    int ret = LaunchSlave(command_line, pipe_number, version);
    if (ret)
      return ret;
  }

  if (upgrade || copy_to_text)
    return CopyCache(output_path, server, copy_to_text);

  if (slave_required) {
    // Wait until the slave starts dumping data before we quit. Lazy "fix" for a
    // console quirk.
    Sleep(500);
    return ALL_GOOD;
  }

  if (command_line.HasSwitch(kDumpContents))
    return DumpContents(input_path);
  if (command_line.HasSwitch(kDumpHeaders))
    return DumpHeaders(input_path);
  return Help();
}
