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

extern "C" int cjpeg(int argc, char *argv[]);

TEST(CJPEGTest, RGBISlow) {

  base::FilePath icc_path;
  GetTestFilePath(&icc_path, "test1.icc");
  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.ppm");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_rgb_islow.jpg");

  std::string prog_name = "cjpeg";
  std::string arg1 = "-rgb";
  std::string arg2 = "-dct";
  std::string arg3 = "int";
  std::string arg4 = "-icc";
  std::string arg5 = icc_path.MaybeAsASCII();
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0],
                         };
  // Generate test image file.
  EXPECT_EQ(cjpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "1d44a406f61da743b5fd31c0a9abdca3";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(CJPEGTest, IFastOpt422) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.ppm");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_422_ifast_opt.jpg");

  std::string prog_name = "cjpeg";
  std::string arg1 = "-sample";
  std::string arg2 = "2x1";
  std::string arg3 = "-dct";
  std::string arg4 = "fast";
  std::string arg5 = "-opt";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0],
                         };
  // Generate test image file.
  EXPECT_EQ(cjpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "2540287b79d913f91665e660303ab2c8";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(CJPEGTest, IFastProg420Q100) {

  base::FilePath scans_path;
  GetTestFilePath(&scans_path, "test.scan");
  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.ppm");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420_q100_ifast_prog.jpg");

  std::string prog_name = "cjpeg";
  std::string arg1 = "-sample";
  std::string arg2 = "2x2";
  std::string arg3 = "-quality";
  std::string arg4 = "100";
  std::string arg5 = "-dct";
  std::string arg6 = "fast";
  std::string arg7 = "-scans";
  std::string arg8 = scans_path.MaybeAsASCII();
  std::string arg9 = "-outfile";
  std::string arg10 = output_path.MaybeAsASCII();
  std::string arg11 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0], &arg9[0], &arg10[0],
                           &arg11[0]
                         };
  // Generate test image file.
  EXPECT_EQ(cjpeg(12, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "0ba15f9dab81a703505f835f9dbbac6d";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(CJPEGTest, GrayISlow) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.ppm");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_gray_islow.jpg");

  std::string prog_name = "cjpeg";
  std::string arg1 = "-gray";
  std::string arg2 = "-dct";
  std::string arg3 = "int";
  std::string arg4 = "-outfile";
  std::string arg5 = output_path.MaybeAsASCII();
  std::string arg6 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0],
                         };
  // Generate test image file.
  EXPECT_EQ(cjpeg(7, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "72b51f894b8f4a10b3ee3066770aa38d";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(CJPEGTest, IFastOpt420S) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.ppm");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420s_ifast_opt.jpg");

  std::string prog_name = "cjpeg";
  std::string arg1 = "-sample";
  std::string arg2 = "2x2";
  std::string arg3 = "-smooth";
  std::string arg4 = "1";
  std::string arg5 = "-dct";
  std::string arg6 = "int";
  std::string arg7 = "-opt";
  std::string arg8 = "-outfile";
  std::string arg9 = output_path.MaybeAsASCII();
  std::string arg10 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0], &arg9[0], &arg10[0]
                         };
  // Generate test image file.
  EXPECT_EQ(cjpeg(11, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "388708217ac46273ca33086b22827ed8";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(CJPEGTest, FloatProg3x2) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.ppm");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_3x2_float_prog.jpg");

  std::string prog_name = "cjpeg";
  std::string arg1 = "-sample";
  std::string arg2 = "3x2";
  std::string arg3 = "-dct";
  std::string arg4 = "float";
  std::string arg5 = "-prog";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(cjpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
#if defined(WITH_SIMD) && (defined(__i386__) || defined(__x86_64__))
  const std::string EXPECTED_MD5 = "343e3f8caf8af5986ebaf0bdc13b5c71";
#else
  const std::string EXPECTED_MD5 = "9bca803d2042bd1eb03819e2bf92b3e5";
#endif
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(CJPEGTest, IFastProg3x2) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.ppm");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_3x2_ifast_prog.jpg");

  std::string prog_name = "cjpeg";
  std::string arg1 = "-sample";
  std::string arg2 = "3x2";
  std::string arg3 = "-dct";
  std::string arg4 = "fast";
  std::string arg5 = "-prog";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(cjpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "1ee5d2c1a77f2da495f993c8c7cceca5";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

#ifdef C_ARITH_CODING_SUPPORTED
TEST(CJPEGTest, ISlowAri420) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.ppm");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420_islow_ari.jpg");

  std::string prog_name = "cjpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-arithmetic";
  std::string arg4 = "-outfile";
  std::string arg5 = output_path.MaybeAsASCII();
  std::string arg6 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0]
                         };
  // Generate test image file.
  EXPECT_EQ(cjpeg(7, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "e986fb0a637a8d833d96e8a6d6d84ea1";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(CJPEGTest, ISlowProgAri444) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.ppm");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_444_islow_progari.jpg");

  std::string prog_name = "cjpeg";
  std::string arg1 = "-sample";
  std::string arg2 = "1x1";
  std::string arg3 = "-dct";
  std::string arg4 = "int";
  std::string arg5 = "-prog";
  std::string arg6 = "-arithmetic";
  std::string arg7 = "-outfile";
  std::string arg8 = output_path.MaybeAsASCII();
  std::string arg9 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0], &arg9[0]
                         };
  // Generate test image file.
  EXPECT_EQ(cjpeg(10, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "0a8f1c8f66e113c3cf635df0a475a617";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(CJPEGTest, ISlowAri444) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.ppm");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_444_islow_ari.jpg");

  std::string prog_name = "cjpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-arithmetic";
  std::string arg4 = "-sample";
  std::string arg5 = "1x1";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(cjpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "dd1b827fc504feb0259894e7132585b4";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}
#endif

TEST(CJPEGTest, ISlowProg420) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.ppm");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420_islow_prog.jpg");

  std::string prog_name = "cjpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-prog";
  std::string arg4 = "-outfile";
  std::string arg5 = output_path.MaybeAsASCII();
  std::string arg6 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0]
                         };
  // Generate test image file.
  EXPECT_EQ(cjpeg(7, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "1c4afddc05c0a43489ee54438a482d92";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(CJPEGTest, ISlow444) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.ppm");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_444_islow.jpg");

  std::string prog_name = "cjpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-sample";
  std::string arg4 = "1x1";
  std::string arg5 = "-outfile";
  std::string arg6 = output_path.MaybeAsASCII();
  std::string arg7 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0]
                         };
  // Generate test image file.
  EXPECT_EQ(cjpeg(8, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "62a8665a2e08e90c6fffa3a94b894ce2";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(CJPEGTest, ISlowProg444) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.ppm");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_444_islow_prog.jpg");

  std::string prog_name = "cjpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-prog";
  std::string arg4 = "-sample";
  std::string arg5 = "1x1";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(cjpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "666ef192fff72570e332db7610e1a7d1";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}
