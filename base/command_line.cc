// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#elif defined(OS_POSIX)
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#endif
#if defined(OS_LINUX)
#include <sys/prctl.h>
#endif

#include <algorithm>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"

#if defined(OS_LINUX)
// Linux/glibc doesn't natively have setproctitle().
#include "base/setproctitle_linux.h"
#endif

CommandLine* CommandLine::current_process_commandline_ = NULL;

// Since we use a lazy match, make sure that longer versions (like L"--")
// are listed before shorter versions (like L"-") of similar prefixes.
#if defined(OS_WIN)
const wchar_t* const kSwitchPrefixes[] = {L"--", L"-", L"/"};
const wchar_t kSwitchTerminator[] = L"--";
const wchar_t kSwitchValueSeparator[] = L"=";
#elif defined(OS_POSIX)
// Unixes don't use slash as a switch.
const char* const kSwitchPrefixes[] = {"--", "-"};
const char kSwitchTerminator[] = "--";
const char kSwitchValueSeparator[] = "=";
#endif

#if defined(OS_WIN)
// Lowercase a string.  This is used to lowercase switch names.
// Is this what we really want?  It seems crazy to me.  I've left it in
// for backwards compatibility on Windows.
static void Lowercase(std::string* parameter) {
  transform(parameter->begin(), parameter->end(), parameter->begin(),
            tolower);
}
#endif

CommandLine::~CommandLine() {
}

#if defined(OS_WIN)
CommandLine::CommandLine(ArgumentsOnly args_only) {
}

void CommandLine::ParseFromString(const std::wstring& command_line) {
  TrimWhitespace(command_line, TRIM_ALL, &command_line_string_);

  if (command_line_string_.empty())
    return;

  int num_args = 0;
  wchar_t** args = NULL;

  args = CommandLineToArgvW(command_line_string_.c_str(), &num_args);

  // Populate program_ with the trimmed version of the first arg.
  TrimWhitespace(args[0], TRIM_ALL, &program_);

  bool parse_switches = true;
  for (int i = 1; i < num_args; ++i) {
    std::wstring arg;
    TrimWhitespace(args[i], TRIM_ALL, &arg);

    if (!parse_switches) {
      args_.push_back(arg);
      continue;
    }

    if (arg == kSwitchTerminator) {
      parse_switches = false;
      continue;
    }

    std::string switch_string;
    std::wstring switch_value;
    if (IsSwitch(arg, &switch_string, &switch_value)) {
      switches_[switch_string] = switch_value;
    } else {
      args_.push_back(arg);
    }
  }

  if (args)
    LocalFree(args);
}

CommandLine::CommandLine(const FilePath& program) {
  if (!program.empty()) {
    program_ = program.value();
    command_line_string_ = L'"' + program.value() + L'"';
  }
}

#elif defined(OS_POSIX)
CommandLine::CommandLine(ArgumentsOnly args_only) {
  // Push an empty argument, because we always assume argv_[0] is a program.
  argv_.push_back("");
}

void CommandLine::InitFromArgv(int argc, const char* const* argv) {
  for (int i = 0; i < argc; ++i)
    argv_.push_back(argv[i]);
  InitFromArgv(argv_);
}

void CommandLine::InitFromArgv(const std::vector<std::string>& argv) {
  argv_ = argv;
  bool parse_switches = true;
  for (size_t i = 1; i < argv_.size(); ++i) {
    const std::string& arg = argv_[i];

    if (!parse_switches) {
      args_.push_back(arg);
      continue;
    }

    if (arg == kSwitchTerminator) {
      parse_switches = false;
      continue;
    }

    std::string switch_string;
    std::string switch_value;
    if (IsSwitch(arg, &switch_string, &switch_value)) {
      switches_[switch_string] = switch_value;
    } else {
      args_.push_back(arg);
    }
  }
}

CommandLine::CommandLine(const FilePath& program) {
  argv_.push_back(program.value());
}

#endif

// static
bool CommandLine::IsSwitch(const StringType& parameter_string,
                           std::string* switch_string,
                           StringType* switch_value) {
  switch_string->clear();
  switch_value->clear();

  for (size_t i = 0; i < arraysize(kSwitchPrefixes); ++i) {
    StringType prefix(kSwitchPrefixes[i]);
    if (parameter_string.find(prefix) != 0)
      continue;

    const size_t switch_start = prefix.length();
    const size_t equals_position = parameter_string.find(
        kSwitchValueSeparator, switch_start);
    StringType switch_native;
    if (equals_position == StringType::npos) {
      switch_native = parameter_string.substr(switch_start);
    } else {
      switch_native = parameter_string.substr(
          switch_start, equals_position - switch_start);
      *switch_value = parameter_string.substr(equals_position + 1);
    }
#if defined(OS_WIN)
    *switch_string = WideToASCII(switch_native);
    Lowercase(switch_string);
#else
    *switch_string = switch_native;
#endif

    return true;
  }

  return false;
}

// static
void CommandLine::Init(int argc, const char* const* argv) {
  delete current_process_commandline_;
  current_process_commandline_ = new CommandLine;
#if defined(OS_WIN)
  current_process_commandline_->ParseFromString(::GetCommandLineW());
#elif defined(OS_POSIX)
  current_process_commandline_->InitFromArgv(argc, argv);
#endif

#if defined(OS_LINUX)
  if (argv)
    setproctitle_init(const_cast<char**>(argv));
#endif
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
// static
void CommandLine::SetProcTitle() {
  // Build a single string which consists of all the arguments separated
  // by spaces. We can't actually keep them separate due to the way the
  // setproctitle() function works.
  std::string title;
  bool have_argv0 = false;
#if defined(OS_LINUX)
  // In Linux we sometimes exec ourselves from /proc/self/exe, but this makes us
  // show up as "exe" in process listings. Read the symlink /proc/self/exe and
  // use the path it points at for our process title. Note that this is only for
  // display purposes and has no TOCTTOU security implications.
  char buffer[PATH_MAX];
  // Note: readlink() does not append a null byte to terminate the string.
  ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer));
  DCHECK(length <= static_cast<ssize_t>(sizeof(buffer)));
  if (length > 0) {
    have_argv0 = true;
    title.assign(buffer, length);
    // If the binary has since been deleted, Linux appends " (deleted)" to the
    // symlink target. Remove it, since this is not really part of our name.
    const std::string kDeletedSuffix = " (deleted)";
    if (EndsWith(title, kDeletedSuffix, true))
      title.resize(title.size() - kDeletedSuffix.size());
#if defined(PR_SET_NAME)
    // If PR_SET_NAME is available at compile time, we try using it. We ignore
    // any errors if the kernel does not support it at runtime though. When
    // available, this lets us set the short process name that shows when the
    // full command line is not being displayed in most process listings.
    prctl(PR_SET_NAME, FilePath(title).BaseName().value().c_str());
#endif
  }
#endif
  for (size_t i = 1; i < current_process_commandline_->argv_.size(); ++i) {
    if (!title.empty())
      title += " ";
    title += current_process_commandline_->argv_[i];
  }
  // Disable prepending argv[0] with '-' if we prepended it ourselves above.
  setproctitle(have_argv0 ? "-%s" : "%s", title.c_str());
}
#endif

void CommandLine::Reset() {
  DCHECK(current_process_commandline_ != NULL);
  delete current_process_commandline_;
  current_process_commandline_ = NULL;
}

bool CommandLine::HasSwitch(const std::string& switch_string) const {
  std::string lowercased_switch(switch_string);
#if defined(OS_WIN)
  Lowercase(&lowercased_switch);
#endif
  return switches_.find(lowercased_switch) != switches_.end();
}

std::wstring CommandLine::GetSwitchValue(
    const std::string& switch_string) const {
  std::string lowercased_switch(switch_string);
#if defined(OS_WIN)
  Lowercase(&lowercased_switch);
#endif

  std::map<std::string, StringType>::const_iterator result =
      switches_.find(lowercased_switch);

  if (result == switches_.end()) {
    return L"";
  } else {
#if defined(OS_WIN)
    return result->second;
#else
    return base::SysNativeMBToWide(result->second);
#endif
  }
}

#if defined(OS_WIN)
std::wstring CommandLine::program() const {
  return program_;
}
#else
std::wstring CommandLine::program() const {
  DCHECK_GT(argv_.size(), 0U);
  return base::SysNativeMBToWide(argv_[0]);
}
#endif

#if defined(OS_POSIX)
std::string CommandLine::command_line_string() const {
  return JoinString(argv_, ' ');
}
#endif

// static
std::wstring CommandLine::PrefixedSwitchString(
    const std::string& switch_string) {
#if defined(OS_WIN)
  return kSwitchPrefixes[0] + ASCIIToWide(switch_string);
#else
  return ASCIIToWide(kSwitchPrefixes[0] + switch_string);
#endif
}

// static
std::wstring CommandLine::PrefixedSwitchStringWithValue(
    const std::string& switch_string, const std::wstring& value_string) {
  if (value_string.empty()) {
    return PrefixedSwitchString(switch_string);
  }

  return PrefixedSwitchString(switch_string +
#if defined(OS_WIN)
                              WideToASCII(kSwitchValueSeparator)
#else
                              kSwitchValueSeparator
#endif
                              ) + value_string;
}

#if defined(OS_WIN)
void CommandLine::AppendSwitch(const std::string& switch_string) {
  std::wstring prefixed_switch_string = PrefixedSwitchString(switch_string);
  command_line_string_.append(L" ");
  command_line_string_.append(prefixed_switch_string);
  switches_[switch_string] = L"";
}

void CommandLine::AppendSwitchWithValue(const std::string& switch_string,
                                        const std::wstring& value_string) {
  std::wstring value_string_edit;

  // NOTE(jhughes): If the value contains a quotation mark at one
  //                end but not both, you may get unusable output.
  if (!value_string.empty() &&
      (value_string.find(L" ") != std::wstring::npos) &&
      (value_string[0] != L'"') &&
      (value_string[value_string.length() - 1] != L'"')) {
    // need to provide quotes
    value_string_edit = StringPrintf(L"\"%ls\"", value_string.c_str());
  } else {
    value_string_edit = value_string;
  }

  std::wstring combined_switch_string =
      PrefixedSwitchStringWithValue(switch_string, value_string_edit);

  command_line_string_.append(L" ");
  command_line_string_.append(combined_switch_string);

  switches_[switch_string] = value_string;
}

void CommandLine::AppendLooseValue(const std::wstring& value) {
  // TODO(evan): quoting?
  command_line_string_.append(L" ");
  command_line_string_.append(value);
  args_.push_back(value);
}

void CommandLine::AppendArguments(const CommandLine& other,
                                  bool include_program) {
  // Verify include_program is used correctly.
  // Logic could be shorter but this is clearer.
  DCHECK_EQ(include_program, !other.GetProgram().empty());
  command_line_string_ += L" " + other.command_line_string_;
  std::map<std::string, StringType>::const_iterator i;
  for (i = other.switches_.begin(); i != other.switches_.end(); ++i)
    switches_[i->first] = i->second;
}

void CommandLine::PrependWrapper(const std::wstring& wrapper) {
  // The wrapper may have embedded arguments (like "gdb --args"). In this case,
  // we don't pretend to do anything fancy, we just split on spaces.
  std::vector<std::wstring> wrapper_and_args;
  SplitString(wrapper, ' ', &wrapper_and_args);
  program_ = wrapper_and_args[0];
  command_line_string_ = wrapper + L" " + command_line_string_;
}

#elif defined(OS_POSIX)
void CommandLine::AppendSwitch(const std::string& switch_string) {
  argv_.push_back(kSwitchPrefixes[0] + switch_string);
  switches_[switch_string] = "";
}

void CommandLine::AppendSwitchWithValue(const std::string& switch_string,
                                        const std::wstring& value_string) {
  std::string mb_value = base::SysWideToNativeMB(value_string);

  argv_.push_back(kSwitchPrefixes[0] + switch_string +
                  kSwitchValueSeparator + mb_value);
  switches_[switch_string] = mb_value;
}

void CommandLine::AppendLooseValue(const std::wstring& value) {
  argv_.push_back(base::SysWideToNativeMB(value));
}

void CommandLine::AppendArguments(const CommandLine& other,
                                  bool include_program) {
  // Verify include_program is used correctly.
  // Logic could be shorter but this is clearer.
  DCHECK_EQ(include_program, !other.GetProgram().empty());
  size_t first_arg = include_program ? 0 : 1;
  for (size_t i = first_arg; i < other.argv_.size(); ++i)
    argv_.push_back(other.argv_[i]);
  std::map<std::string, StringType>::const_iterator i;
  for (i = other.switches_.begin(); i != other.switches_.end(); ++i)
    switches_[i->first] = i->second;
}

void CommandLine::PrependWrapper(const std::wstring& wrapper_wide) {
  // The wrapper may have embedded arguments (like "gdb --args"). In this case,
  // we don't pretend to do anything fancy, we just split on spaces.
  const std::string wrapper = base::SysWideToNativeMB(wrapper_wide);
  std::vector<std::string> wrapper_and_args;
  SplitString(wrapper, ' ', &wrapper_and_args);
  argv_.insert(argv_.begin(), wrapper_and_args.begin(), wrapper_and_args.end());
}

#endif

// private
CommandLine::CommandLine() {
}
