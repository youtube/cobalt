// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"

#include <algorithm>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#elif defined(OS_POSIX)
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#endif

CommandLine* CommandLine::current_process_commandline_ = NULL;

namespace {
typedef CommandLine::StringType::value_type CharType;

const CharType kSwitchTerminator[] = FILE_PATH_LITERAL("--");
const CharType kSwitchValueSeparator[] = FILE_PATH_LITERAL("=");
// Since we use a lazy match, make sure that longer versions (like "--") are
// listed before shorter versions (like "-") of similar prefixes.
#if defined(OS_WIN)
const CharType* const kSwitchPrefixes[] = {L"--", L"-", L"/"};
#elif defined(OS_POSIX)
// Unixes don't use slash as a switch.
const CharType* const kSwitchPrefixes[] = {"--", "-"};
#endif

#if defined(OS_WIN)
// Lowercase a string for case-insensitivity of switches.
// Is this desirable? It exists for backwards compatibility on Windows.
void Lowercase(std::string* arg) {
  transform(arg->begin(), arg->end(), arg->begin(), tolower);
}

// Quote a string if necessary, such that CommandLineToArgvW() will always
// process it as a single argument.
std::wstring WindowsStyleQuote(const std::wstring& arg) {
  // We follow the quoting rules of CommandLineToArgvW.
  // http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
  if (arg.find_first_of(L" \\\"") == std::wstring::npos) {
    // No quoting necessary.
    return arg;
  }

  std::wstring out;
  out.push_back(L'"');
  for (size_t i = 0; i < arg.size(); ++i) {
    if (arg[i] == '\\') {
      // Find the extent of this run of backslashes.
      size_t start = i, end = start + 1;
      for (; end < arg.size() && arg[end] == '\\'; ++end)
        /* empty */;
      size_t backslash_count = end - start;

      // Backslashes are escapes only if the run is followed by a double quote.
      // Since we also will end the string with a double quote, we escape for
      // either a double quote or the end of the string.
      if (end == arg.size() || arg[end] == '"') {
        // To quote, we need to output 2x as many backslashes.
        backslash_count *= 2;
      }
      for (size_t j = 0; j < backslash_count; ++j)
        out.push_back('\\');

      // Advance i to one before the end to balance i++ in loop.
      i = end - 1;
    } else if (arg[i] == '"') {
      out.push_back('\\');
      out.push_back('"');
    } else {
      out.push_back(arg[i]);
    }
  }
  out.push_back('"');

  return out;
}
#endif

// Returns true and fills in |switch_string| and |switch_value| if
// |parameter_string| represents a switch.
bool IsSwitch(const CommandLine::StringType& parameter_string,
              std::string* switch_string,
              CommandLine::StringType* switch_value) {
  switch_string->clear();
  switch_value->clear();

  for (size_t i = 0; i < arraysize(kSwitchPrefixes); ++i) {
    CommandLine::StringType prefix(kSwitchPrefixes[i]);
    if (parameter_string.find(prefix) != 0)
      continue;

    const size_t switch_start = prefix.length();
    const size_t equals_position = parameter_string.find(
        kSwitchValueSeparator, switch_start);
    CommandLine::StringType switch_native;
    if (equals_position == CommandLine::StringType::npos) {
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

}  // namespace

CommandLine::CommandLine(NoProgram no_program) {
#if defined(OS_POSIX)
  // Push an empty argument, because we always assume argv_[0] is a program.
  argv_.push_back("");
#endif
}

CommandLine::CommandLine(const FilePath& program) {
#if defined(OS_WIN)
  if (!program.empty()) {
    program_ = program.value();
    // TODO(evanm): proper quoting here.
    command_line_string_ = L'"' + program.value() + L'"';
  }
#elif defined(OS_POSIX)
  argv_.push_back(program.value());
#endif
}

#if defined(OS_POSIX)
CommandLine::CommandLine(int argc, const char* const* argv) {
  InitFromArgv(argc, argv);
}

CommandLine::CommandLine(const StringVector& argv) {
  InitFromArgv(argv);
}
#endif  // OS_POSIX

// static
void CommandLine::Init(int argc, const char* const* argv) {
  delete current_process_commandline_;
  current_process_commandline_ = new CommandLine;
#if defined(OS_WIN)
  current_process_commandline_->ParseFromString(::GetCommandLineW());
#elif defined(OS_POSIX)
  current_process_commandline_->InitFromArgv(argc, argv);
#endif
}

// static
void CommandLine::Reset() {
  DCHECK(current_process_commandline_);
  delete current_process_commandline_;
  current_process_commandline_ = NULL;
}

// static
CommandLine* CommandLine::ForCurrentProcess() {
  DCHECK(current_process_commandline_);
  return current_process_commandline_;
}

#if defined(OS_WIN)
// static
CommandLine CommandLine::FromString(const std::wstring& command_line) {
  CommandLine cmd;
  cmd.ParseFromString(command_line);
  return cmd;
}
#endif  // OS_WIN

#if defined(OS_POSIX)
void CommandLine::InitFromArgv(int argc, const char* const* argv) {
  for (int i = 0; i < argc; ++i)
    argv_.push_back(argv[i]);
  InitFromArgv(argv_);
}

void CommandLine::InitFromArgv(const StringVector& argv) {
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
    StringType switch_value;
    if (IsSwitch(arg, &switch_string, &switch_value)) {
      switches_[switch_string] = switch_value;
    } else {
      args_.push_back(arg);
    }
  }
}
#endif  // OS_POSIX

CommandLine::StringType CommandLine::command_line_string() const {
#if defined(OS_WIN)
  return command_line_string_;
#elif defined(OS_POSIX)
  return JoinString(argv_, ' ');
#endif
}

FilePath CommandLine::GetProgram() const {
#if defined(OS_WIN)
  return FilePath(program_);
#else
  DCHECK_GT(argv_.size(), 0U);
  return FilePath(argv_[0]);
#endif
}

bool CommandLine::HasSwitch(const std::string& switch_string) const {
  std::string lowercased_switch(switch_string);
#if defined(OS_WIN)
  Lowercase(&lowercased_switch);
#endif
  return switches_.find(lowercased_switch) != switches_.end();
}

std::string CommandLine::GetSwitchValueASCII(
    const std::string& switch_string) const {
  CommandLine::StringType value = GetSwitchValueNative(switch_string);
  if (!IsStringASCII(value)) {
    LOG(WARNING) << "Value of --" << switch_string << " must be ASCII.";
    return "";
  }
#if defined(OS_WIN)
  return WideToASCII(value);
#else
  return value;
#endif
}

FilePath CommandLine::GetSwitchValuePath(
    const std::string& switch_string) const {
  return FilePath(GetSwitchValueNative(switch_string));
}

CommandLine::StringType CommandLine::GetSwitchValueNative(
    const std::string& switch_string) const {
  std::string lowercased_switch(switch_string);
#if defined(OS_WIN)
  Lowercase(&lowercased_switch);
#endif

  SwitchMap::const_iterator result = switches_.find(lowercased_switch);

  if (result == switches_.end()) {
    return CommandLine::StringType();
  } else {
    return result->second;
  }
}

size_t CommandLine::GetSwitchCount() const {
  return switches_.size();
}

void CommandLine::AppendSwitch(const std::string& switch_string) {
#if defined(OS_WIN)
  command_line_string_.append(L" ");
  command_line_string_.append(kSwitchPrefixes[0] + ASCIIToWide(switch_string));
  switches_[switch_string] = L"";
#elif defined(OS_POSIX)
  argv_.push_back(kSwitchPrefixes[0] + switch_string);
  switches_[switch_string] = "";
#endif
}

void CommandLine::AppendSwitchPath(const std::string& switch_string,
                                   const FilePath& path) {
  AppendSwitchNative(switch_string, path.value());
}

void CommandLine::AppendSwitchNative(const std::string& switch_string,
                                     const CommandLine::StringType& value) {
#if defined(OS_WIN)
  StringType combined_switch_string =
      kSwitchPrefixes[0] + ASCIIToWide(switch_string);
  if (!value.empty())
    combined_switch_string += kSwitchValueSeparator + WindowsStyleQuote(value);

  command_line_string_.append(L" ");
  command_line_string_.append(combined_switch_string);

  switches_[switch_string] = value;
#elif defined(OS_POSIX)
  StringType combined_switch_string = kSwitchPrefixes[0] + switch_string;
  if (!value.empty())
    combined_switch_string += kSwitchValueSeparator + value;
  argv_.push_back(combined_switch_string);
  switches_[switch_string] = value;
#endif
}

void CommandLine::AppendSwitchASCII(const std::string& switch_string,
                                    const std::string& value_string) {
#if defined(OS_WIN)
  AppendSwitchNative(switch_string, ASCIIToWide(value_string));
#elif defined(OS_POSIX)
  AppendSwitchNative(switch_string, value_string);
#endif
}

void CommandLine::AppendSwitches(const CommandLine& other) {
  SwitchMap::const_iterator i;
  for (i = other.switches_.begin(); i != other.switches_.end(); ++i)
    AppendSwitchNative(i->first, i->second);
}

void CommandLine::CopySwitchesFrom(const CommandLine& source,
                                   const char* const switches[],
                                   size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (source.HasSwitch(switches[i])) {
      StringType value = source.GetSwitchValueNative(switches[i]);
      AppendSwitchNative(switches[i], value);
    }
  }
}

void CommandLine::AppendArg(const std::string& value) {
#if defined(OS_WIN)
  DCHECK(IsStringUTF8(value));
  AppendArgNative(UTF8ToWide(value));
#elif defined(OS_POSIX)
  AppendArgNative(value);
#endif
}

void CommandLine::AppendArgPath(const FilePath& path) {
  AppendArgNative(path.value());
}

void CommandLine::AppendArgNative(const CommandLine::StringType& value) {
#if defined(OS_WIN)
  command_line_string_.append(L" ");
  command_line_string_.append(WindowsStyleQuote(value));
  args_.push_back(value);
#elif defined(OS_POSIX)
  DCHECK(IsStringUTF8(value));
  argv_.push_back(value);
#endif
}

void CommandLine::AppendArgs(const CommandLine& other) {
  if(other.args_.size() <= 0)
    return;
#if defined(OS_WIN)
  command_line_string_.append(L" --");
#endif  // OS_WIN
  StringVector::const_iterator i;
  for (i = other.args_.begin(); i != other.args_.end(); ++i)
    AppendArgNative(*i);
}

void CommandLine::AppendArguments(const CommandLine& other,
                                  bool include_program) {
#if defined(OS_WIN)
  // Verify include_program is used correctly.
  DCHECK(!include_program || !other.GetProgram().empty());
  if (include_program)
    program_ = other.program_;

  if (!command_line_string_.empty())
    command_line_string_ += L' ';

  command_line_string_ += other.command_line_string_;
#elif defined(OS_POSIX)
  // Verify include_program is used correctly.
  // Logic could be shorter but this is clearer.
  DCHECK_EQ(include_program, !other.GetProgram().empty());

  if (include_program)
    argv_[0] = other.argv_[0];

  // Skip the first arg when copying since it's the program but push all
  // arguments to our arg vector.
  for (size_t i = 1; i < other.argv_.size(); ++i)
    argv_.push_back(other.argv_[i]);
#endif

  SwitchMap::const_iterator i;
  for (i = other.switches_.begin(); i != other.switches_.end(); ++i)
    switches_[i->first] = i->second;
}

void CommandLine::PrependWrapper(const CommandLine::StringType& wrapper) {
  // The wrapper may have embedded arguments (like "gdb --args"). In this case,
  // we don't pretend to do anything fancy, we just split on spaces.
  if (wrapper.empty())
    return;
  StringVector wrapper_and_args;
#if defined(OS_WIN)
  base::SplitString(wrapper, ' ', &wrapper_and_args);
  program_ = wrapper_and_args[0];
  command_line_string_ = wrapper + L" " + command_line_string_;
#elif defined(OS_POSIX)
  base::SplitString(wrapper, ' ', &wrapper_and_args);
  argv_.insert(argv_.begin(), wrapper_and_args.begin(), wrapper_and_args.end());
#endif
}

#if defined(OS_WIN)
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
#endif
