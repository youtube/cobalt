// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

// To test Windows quoting behavior, we use a string that has some backslashes
// and quotes.
// Consider the command-line argument: q\"bs1\bs2\\bs3q\\\"
// Here it is with C-style escapes.
#define TRICKY_QUOTED L"q\\\"bs1\\bs2\\\\bs3q\\\\\\\""
// It should be parsed by Windows as: q"bs1\bs2\\bs3q\"
// Here that is with C-style escapes.
#define TRICKY L"q\"bs1\\bs2\\\\bs3q\\\""

TEST(CommandLineTest, CommandLineConstructor) {
#if defined(OS_WIN)
  CommandLine cl = CommandLine::FromString(
                     L"program --foo= -bAr  /Spaetzel=pierogi /Baz flim "
                     L"--other-switches=\"--dog=canine --cat=feline\" "
                     L"-spaetzle=Crepe   -=loosevalue  flan "
                     L"--input-translation=\"45\"--output-rotation "
                     L"--quotes=" TRICKY_QUOTED L" "
                     L"-- -- --not-a-switch "
                     L"\"in the time of submarines...\"");
  EXPECT_FALSE(cl.command_line_string().empty());
#elif defined(OS_POSIX)
  const char* argv[] = {"program", "--foo=", "-bar",
                        "-spaetzel=pierogi", "-baz", "flim",
                        "--other-switches=--dog=canine --cat=feline",
                        "-spaetzle=Crepe", "-=loosevalue", "flan",
                        "--input-translation=45--output-rotation",
                        "--", "--", "--not-a-switch",
                        "in the time of submarines..."};
  CommandLine cl(arraysize(argv), argv);
#endif
  EXPECT_FALSE(cl.HasSwitch("cruller"));
  EXPECT_FALSE(cl.HasSwitch("flim"));
  EXPECT_FALSE(cl.HasSwitch("program"));
  EXPECT_FALSE(cl.HasSwitch("dog"));
  EXPECT_FALSE(cl.HasSwitch("cat"));
  EXPECT_FALSE(cl.HasSwitch("output-rotation"));
  EXPECT_FALSE(cl.HasSwitch("not-a-switch"));
  EXPECT_FALSE(cl.HasSwitch("--"));

  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("program")).value(),
            cl.GetProgram().value());

  EXPECT_TRUE(cl.HasSwitch("foo"));
  EXPECT_TRUE(cl.HasSwitch("bar"));
  EXPECT_TRUE(cl.HasSwitch("baz"));
  EXPECT_TRUE(cl.HasSwitch("spaetzle"));
#if defined(OS_WIN)
  EXPECT_TRUE(cl.HasSwitch("SPAETZLE"));
#endif
  EXPECT_TRUE(cl.HasSwitch("other-switches"));
  EXPECT_TRUE(cl.HasSwitch("input-translation"));
#if defined(OS_WIN)
  EXPECT_TRUE(cl.HasSwitch("quotes"));
#endif

  EXPECT_EQ("Crepe", cl.GetSwitchValueASCII("spaetzle"));
  EXPECT_EQ("", cl.GetSwitchValueASCII("Foo"));
  EXPECT_EQ("", cl.GetSwitchValueASCII("bar"));
  EXPECT_EQ("", cl.GetSwitchValueASCII("cruller"));
  EXPECT_EQ("--dog=canine --cat=feline", cl.GetSwitchValueASCII(
      "other-switches"));
  EXPECT_EQ("45--output-rotation", cl.GetSwitchValueASCII("input-translation"));
#if defined(OS_WIN)
  EXPECT_EQ(TRICKY, cl.GetSwitchValueNative("quotes"));
#endif

  const std::vector<CommandLine::StringType>& args = cl.args();
  ASSERT_EQ(5U, args.size());

  std::vector<CommandLine::StringType>::const_iterator iter = args.begin();
  EXPECT_EQ(FILE_PATH_LITERAL("flim"), *iter);
  ++iter;
  EXPECT_EQ(FILE_PATH_LITERAL("flan"), *iter);
  ++iter;
  EXPECT_EQ(FILE_PATH_LITERAL("--"), *iter);
  ++iter;
  EXPECT_EQ(FILE_PATH_LITERAL("--not-a-switch"), *iter);
  ++iter;
  EXPECT_EQ(FILE_PATH_LITERAL("in the time of submarines..."), *iter);
  ++iter;
  EXPECT_TRUE(iter == args.end());
#if defined(OS_POSIX)
  const std::vector<std::string>& argvec = cl.argv();

  for (size_t i = 0; i < argvec.size(); i++) {
    EXPECT_EQ(0, argvec[i].compare(argv[i]));
  }
#endif
}

// Tests behavior with an empty input string.
TEST(CommandLineTest, EmptyString) {
#if defined(OS_WIN)
  CommandLine cl = CommandLine::FromString(L"");
  EXPECT_TRUE(cl.command_line_string().empty());
  EXPECT_TRUE(cl.GetProgram().empty());
#elif defined(OS_POSIX)
  CommandLine cl(0, NULL);
  EXPECT_EQ(0U, cl.argv().size());
#endif
  EXPECT_EQ(0U, cl.args().size());
}

// Test methods for appending switches to a command line.
TEST(CommandLineTest, AppendSwitches) {
  std::string switch1 = "switch1";
  std::string switch2 = "switch2";
  std::string value = "value";
  std::string switch3 = "switch3";
  std::string value3 = "a value with spaces";
  std::string switch4 = "switch4";
  std::string value4 = "\"a value with quotes\"";
  std::string switch5 = "quotes";
  std::string value5 = WideToUTF8(TRICKY);

  CommandLine cl(FilePath(FILE_PATH_LITERAL("Program")));

  cl.AppendSwitch(switch1);
  cl.AppendSwitchASCII(switch2, value);
  cl.AppendSwitchASCII(switch3, value3);
  cl.AppendSwitchASCII(switch4, value4);
  cl.AppendSwitchASCII(switch5, value5);

  EXPECT_TRUE(cl.HasSwitch(switch1));
  EXPECT_TRUE(cl.HasSwitch(switch2));
  EXPECT_EQ(value, cl.GetSwitchValueASCII(switch2));
  EXPECT_TRUE(cl.HasSwitch(switch3));
  EXPECT_EQ(value3, cl.GetSwitchValueASCII(switch3));
  EXPECT_TRUE(cl.HasSwitch(switch4));
  EXPECT_EQ(value4, cl.GetSwitchValueASCII(switch4));
  EXPECT_TRUE(cl.HasSwitch(switch5));
  EXPECT_EQ(value5, cl.GetSwitchValueASCII(switch5));

#if defined(OS_WIN)
  EXPECT_EQ(L"\"Program\" "
            L"--switch1 "
            L"--switch2=value "
            L"--switch3=\"a value with spaces\" "
            L"--switch4=\"\\\"a value with quotes\\\"\" "
            L"--quotes=\"" TRICKY_QUOTED L"\"",
            cl.command_line_string());
#endif
}

// Tests that when AppendArguments is called that the program is set correctly
// on the target CommandLine object and the switches from the source
// CommandLine are added to the target.
TEST(CommandLineTest, AppendArguments) {
  CommandLine cl1(FilePath(FILE_PATH_LITERAL("Program")));
  cl1.AppendSwitch("switch1");
  cl1.AppendSwitchASCII("switch2", "foo");

  CommandLine cl2(CommandLine::NO_PROGRAM);
  cl2.AppendArguments(cl1, true);
  EXPECT_EQ(cl1.GetProgram().value(), cl2.GetProgram().value());
  EXPECT_EQ(cl1.command_line_string(), cl2.command_line_string());

  CommandLine c1(FilePath(FILE_PATH_LITERAL("Program1")));
  c1.AppendSwitch("switch1");
  CommandLine c2(FilePath(FILE_PATH_LITERAL("Program2")));
  c2.AppendSwitch("switch2");

  c1.AppendArguments(c2, true);
  EXPECT_EQ(c1.GetProgram().value(), c2.GetProgram().value());
  EXPECT_TRUE(c1.HasSwitch("switch1"));
  EXPECT_TRUE(c1.HasSwitch("switch2"));
}

#if defined(OS_WIN)
// Make sure that the program part of a command line is always quoted.
// This only makes sense on Windows and the test is basically here to guard
// against regressions.
TEST(CommandLineTest, ProgramQuotes) {
  const FilePath kProgram(L"Program");

  // Check that quotes are not returned from GetProgram().
  CommandLine cl(kProgram);
  EXPECT_EQ(kProgram.value(), cl.GetProgram().value());

  // Verify that in the command line string, the program part is always quoted.
  CommandLine::StringType cmd(cl.command_line_string());
  CommandLine::StringType program(cl.GetProgram().value());
  EXPECT_EQ('"', cmd[0]);
  EXPECT_EQ(program, cmd.substr(1, program.length()));
  EXPECT_EQ('"', cmd[program.length() + 1]);
}
#endif
