/*
 * Copyright 2020 The Chromium Authors. All Rights Reserved.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "gtest-utils.h"

#include <gtest/gtest.h>
#include <string>

extern "C" int jpegtran(int argc, char *argv[]);

TEST(JPEGTranTest, ICC) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_rgb_islow.jpg");
  base::FilePath icc_path;
  GetTestFilePath(&icc_path, "test2.icc");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_rgb_islow2.jpg");

  std::string prog_name = "jpegtran";
  std::string arg1 = "-copy";
  std::string arg2 = "all";
  std::string arg3 = "-icc";
  std::string arg4 = icc_path.MaybeAsASCII();
  std::string arg5 = "-outfile";
  std::string arg6 = output_path.MaybeAsASCII();
  std::string arg7 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0]
                         };
  // Generate test image file.
  EXPECT_EQ(jpegtran(8, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "31d121e57b6c2934c890a7fc7763bcd4";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(JPEGTranTest, Crop) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_crop.jpg");

  std::string prog_name = "jpegtran";
  std::string arg1 = "-crop";
  std::string arg2 = "120x90+20+50";
  std::string arg3 = "-transpose";
  std::string arg4 = "-perfect";
  std::string arg5 = "-outfile";
  std::string arg6 = output_path.MaybeAsASCII();
  std::string arg7 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0]
                         };
  // Generate test image file.
  EXPECT_EQ(jpegtran(8, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "b4197f377e621c4e9b1d20471432610d";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

#ifdef C_ARITH_CODING_SUPPORTED
TEST(JPEGTranTest, ISlow420Ari) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testimgint.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420_islow_ari2.jpg");

  std::string prog_name = "jpegtran";
  std::string arg1 = "-arithmetic";
  std::string arg2 = "-outfile";
  std::string arg3 = output_path.MaybeAsASCII();
  std::string arg4 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0]
                         };
  // Generate test image file.
  EXPECT_EQ(jpegtran(5, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "e986fb0a637a8d833d96e8a6d6d84ea1";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(JPEGTranTest, ISlow420) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testimgari.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420_islow.jpg");

  std::string prog_name = "jpegtran";
  std::string arg1 = "-outfile";
  std::string arg2 = output_path.MaybeAsASCII();
  std::string arg3 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0]
                         };
  // Generate test image file.
  EXPECT_EQ(jpegtran(4, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "9a68f56bc76e466aa7e52f415d0f4a5f";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}
#endif
