// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class works with command lines: building and parsing.
// Arguments with prefixes ('--', '-') are switches.
// Switches will precede all other arguments without switch prefixes.
// Switches can optionally have values, delimited by '=', e.g., "-switch=value".
// An argument of "--" will terminate switch parsing during initialization,
// interpreting subsequent tokens as non-switch arguments, regardless of prefix.

#ifndef STARBOARD_SHARED_STARBOARD_COMMAND_LINE_H_
#define STARBOARD_SHARED_STARBOARD_COMMAND_LINE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {

class CommandLine {
 public:
  typedef std::string StringType;

  typedef StringType::value_type CharType;
  typedef std::vector<StringType> StringVector;
  typedef std::map<std::string, StringType> SwitchMap;

  // Construct a new command line from an argument list.
  CommandLine(int argc, const CharType* const* argv);

  // Construct a new command line from a vector of strings.
  explicit CommandLine(const StringVector& argv);

  // Copy constructor
  CommandLine(const CommandLine& other)
      : argv_(other.argv_),
        switches_(other.switches_),
        begin_args_(other.begin_args_) {}

  // Move constructor
  CommandLine(CommandLine&& other) {
    swap(*this, other);
  }

  ~CommandLine();

  // Copy/move assignment. The "copy" of the copy-and-swap idiom is done by the
  // compiler to provide the by-value parameter, which comes from either the
  // copy or move constructor depending on whether or not other was an rvalue.
  CommandLine& operator=(CommandLine other) noexcept {
    swap(*this, other);
    return *this;
  }

  // Swaps two CommandLine values.
  friend void swap(CommandLine& a, CommandLine& b) {
    using std::swap;
    swap(a.argv_, b.argv_);
    swap(a.switches_, b.switches_);
    swap(a.begin_args_, b.begin_args_);
  }

  // Initialize from an argv vector.
  void InitFromArgv(int argc, const CharType* const* argv);

  // Returns the original command line string as a vector of strings.
  const StringVector& argv() const { return argv_; }

  // Returns true if this command line contains the given switch.
  // (Switch names are case-insensitive).
  bool HasSwitch(const std::string& switch_string) const;

  // Append a switch [with optional value] to the command line.
  // Note: Switches will precede arguments regardless of appending order.
  void AppendSwitch(const std::string& switch_string,
                    const StringType& value);

  // Returns the value associated with the given switch. If the switch has no
  // value or isn't present, this method returns the empty string.
  StringType GetSwitchValue(const std::string& switch_string) const;

  // Get a copy of all switches, along with their values.
  const SwitchMap& GetSwitches() const { return switches_; }

  // Get the remaining arguments to the command.
  StringVector GetArgs() const;

  // Append an argument to the command line. Note that the argument is quoted
  // properly such that it is interpreted as one argument to the target command.
  // AppendArg is primarily for ASCII; non-ASCII input is interpreted as UTF-8.
  // Note: Switches will precede arguments regardless of appending order.
  void AppendArg(const std::string& value);

  // Append the switches and arguments from another command line to this one.
  // If |include_program| is true, include |other|'s program as well.
  void AppendArguments(const CommandLine& other, bool include_program);

 private:
  void InitFromArgv(const StringVector& argv);

  // Disallow default constructor; a program name must be explicitly specified.
  CommandLine();
  // Allow the copy constructor. A common pattern is to copy of the current
  // process's command line and then add some flags to it.

  // The argv array: { program, [(--|-|/)switch[=value]]*, [--], [argument]* }
  StringVector argv_;

  // Parsed-out switch keys and values.
  SwitchMap switches_;

  // The index after the program and switches, any arguments start here.
  size_t begin_args_;
};

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_COMMAND_LINE_H_
