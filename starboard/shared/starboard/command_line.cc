// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard/shared/starboard/command_line.h"

#include <algorithm>
#include <ostream>

#include "starboard/configuration.h"
#include "starboard/log.h"

namespace starboard {
namespace shared {
namespace starboard {

namespace {
// From base/string_util.cc
const char kWhitespaceASCII[] = {
  0x09,    // <control-0009> to <control-000D>
  0x0A,
  0x0B,
  0x0C,
  0x0D,
  0x20,    // Space
  0
};

const CommandLine::CharType kSwitchTerminator[] = "--";
const CommandLine::CharType kSwitchValueSeparator[] = "=";
// Since we use a lazy match, make sure that longer versions (like "--") are
// listed before shorter versions (like "-") of similar prefixes.
// Unixes don't use slash as a switch.
const CommandLine::CharType* const kSwitchPrefixes[] = {"--", "-"};

size_t GetSwitchPrefixLength(const CommandLine::StringType& string) {
  for (size_t i = 0; i < SB_ARRAY_SIZE_INT(kSwitchPrefixes); ++i) {
    CommandLine::StringType prefix(kSwitchPrefixes[i]);
    if (string.compare(0, prefix.length(), prefix) == 0)
      return prefix.length();
  }
  return 0;
}

// Fills in |switch_string| and |switch_value| if |string| is a switch.
// This will preserve the input switch prefix in the output |switch_string|.
bool IsSwitch(const CommandLine::StringType& string,
              CommandLine::StringType* switch_string,
              CommandLine::StringType* switch_value) {
  switch_string->clear();
  switch_value->clear();
  size_t prefix_length = GetSwitchPrefixLength(string);
  if (prefix_length == 0 || prefix_length == string.length())
    return false;

  const size_t equals_position = string.find(kSwitchValueSeparator);
  *switch_string = string.substr(0, equals_position);
  if (equals_position != CommandLine::StringType::npos)
    *switch_value = string.substr(equals_position + 1);
  return true;
}

// Append switches and arguments, keeping switches before arguments.
void AppendSwitchesAndArguments(CommandLine* command_line,
                                const CommandLine::StringVector& argv) {
  bool parse_switches = true;
  for (size_t i = 1; i < argv.size(); ++i) {
    CommandLine::StringType arg = argv[i];

    // Begin inlined TrimWhitespace TRIM_ALL from base/string_util.cc
    CommandLine::StringType::size_type first_good_char =
        arg.find_first_not_of(kWhitespaceASCII);
    CommandLine::StringType::size_type last_good_char =
        arg.find_last_not_of(kWhitespaceASCII);

    if (arg.empty() ||
      (first_good_char == CommandLine::StringType::npos) ||
        (last_good_char == CommandLine::StringType::npos)) {
      arg.clear();
    } else {
      // Trim the whitespace.
      arg = arg.substr(first_good_char, last_good_char - first_good_char + 1);
    }
    // End inlined TrimWhitespace TRIM_ALL from base/string_util.cc

    CommandLine::StringType switch_string;
    CommandLine::StringType switch_value;
    parse_switches &= (arg != kSwitchTerminator);
    if (parse_switches && IsSwitch(arg, &switch_string, &switch_value)) {
      command_line->AppendSwitch(switch_string, switch_value);
    } else {
      command_line->AppendArg(arg);
    }
  }
}

// Lowercase switches for backwards compatiblity *on Windows*.
std::string LowerASCIIOnWindows(const std::string& string) {
  return string;
}

}  // namespace

CommandLine::CommandLine(int argc, const CommandLine::CharType* const* argv)
    : argv_(1),
      begin_args_(1) {
  InitFromArgv(argc, argv);
}

CommandLine::CommandLine(const StringVector& argv)
    : argv_(1),
      begin_args_(1) {
  InitFromArgv(argv);
}

CommandLine::~CommandLine() {
}

void CommandLine::InitFromArgv(int argc,
                               const CommandLine::CharType* const* argv) {
  StringVector new_argv;
  for (int i = 0; i < argc; ++i)
    new_argv.push_back(argv[i]);
  InitFromArgv(new_argv);
}

void CommandLine::InitFromArgv(const StringVector& argv) {
  argv_ = StringVector(1);
  begin_args_ = 1;
  AppendSwitchesAndArguments(this, argv);
}

bool CommandLine::HasSwitch(const std::string& switch_string) const {
  return switches_.find(LowerASCIIOnWindows(switch_string)) != switches_.end();
}

CommandLine::StringType CommandLine::GetSwitchValue(
    const std::string& switch_string) const {
  SwitchMap::const_iterator result = switches_.end();
  result = switches_.find(LowerASCIIOnWindows(switch_string));
  return result == switches_.end() ? StringType() : result->second;
}

void CommandLine::AppendSwitch(const std::string& switch_string,
                               const CommandLine::StringType& value) {
  std::string switch_key(LowerASCIIOnWindows(switch_string));
  StringType combined_switch_string(switch_string);
  size_t prefix_length = GetSwitchPrefixLength(combined_switch_string);
  switches_[switch_key.substr(prefix_length)] = value;
  // Preserve existing switch prefixes in |argv_|; only append one if necessary.
  if (prefix_length == 0)
    combined_switch_string = kSwitchPrefixes[0] + combined_switch_string;
  if (!value.empty())
    combined_switch_string += kSwitchValueSeparator + value;
  // Append the switch and update the switches/arguments divider |begin_args_|.
  argv_.insert(argv_.begin() + begin_args_++, combined_switch_string);
}

CommandLine::StringVector CommandLine::GetArgs() const {
  // Gather all arguments after the last switch (may include kSwitchTerminator).
  StringVector args(argv_.begin() + begin_args_, argv_.end());
  // Erase only the first kSwitchTerminator (maybe "--" is a legitimate page?)
  StringVector::iterator switch_terminator =
      std::find(args.begin(), args.end(), kSwitchTerminator);
  if (switch_terminator != args.end())
    args.erase(switch_terminator);
  return args;
}

void CommandLine::AppendArg(const CommandLine::StringType& value) {
  argv_.push_back(value);
}

void CommandLine::AppendArguments(const CommandLine& other,
                                  bool include_program) {
  SB_UNREFERENCED_PARAMETER(include_program);
  AppendSwitchesAndArguments(this, other.argv());
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard
