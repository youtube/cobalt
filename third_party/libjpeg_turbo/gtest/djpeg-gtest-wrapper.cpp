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

extern "C" int djpeg(int argc, char *argv[]);

const static std::vector<std::tuple<const std::string,
                                    const std::string,
                                    const std::string>> SCALE_IMAGE_MD5 = {
  std::make_tuple("2/1", "testout_420m_islow_2_1.ppm",
                  "9f9de8c0612f8d06869b960b05abf9c9"),
  std::make_tuple("15/8", "testout_420m_islow_15_8.ppm",
                  "b6875bc070720b899566cc06459b63b7"),
  std::make_tuple("13/8", "testout_420m_islow_13_8.ppm",
                  "bc3452573c8152f6ae552939ee19f82f"),
  std::make_tuple("11/8", "testout_420m_islow_11_8.ppm",
                  "d8cc73c0aaacd4556569b59437ba00a5"),
  std::make_tuple("9/8", "testout_420m_islow_9_8.ppm",
                  "d25e61bc7eac0002f5b393aa223747b6"),
  std::make_tuple("7/8", "testout_420m_islow_7_8.ppm",
                  "ddb564b7c74a09494016d6cd7502a946"),
  std::make_tuple("3/4", "testout_420m_islow_3_4.ppm",
                  "8ed8e68808c3fbc4ea764fc9d2968646"),
  std::make_tuple("5/8", "testout_420m_islow_5_8.ppm",
                  "a3363274999da2366a024efae6d16c9b"),
  std::make_tuple("1/2", "testout_420m_islow_1_2.ppm",
                  "e692a315cea26b988c8e8b29a5dbcd81"),
  std::make_tuple("3/8", "testout_420m_islow_3_8.ppm",
                  "79eca9175652ced755155c90e785a996"),
  std::make_tuple("1/4", "testout_420m_islow_1_4.ppm",
                  "79cd778f8bf1a117690052cacdd54eca"),
  std::make_tuple("1/8", "testout_420m_islow_1_8.ppm",
                  "391b3d4aca640c8567d6f8745eb2142f")
};

class DJPEGTestScalingDCT : public
  ::testing::TestWithParam<std::tuple<const std::string,
                                      const std::string,
                                      const std::string>> {};

TEST_P(DJPEGTestScalingDCT, Test) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII(std::get<1>(GetParam()));

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-scale";
  std::string arg4 = std::get<0>(GetParam());
  std::string arg5 = "-nosmooth";
  std::string arg6 = "-ppm";
  std::string arg7 = "-outfile";
  std::string arg8 = output_path.MaybeAsASCII();
  std::string arg9 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0], &arg9[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(10, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  EXPECT_TRUE(CompareFileAndMD5(output_path, std::get<2>(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(TestScalingDCT,
                         DJPEGTestScalingDCT,
                         ::testing::ValuesIn(SCALE_IMAGE_MD5));

TEST(DJPEGTest, ISlow420256) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420_islow_256.bmp");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-colors";
  std::string arg4 = "256";
  std::string arg5 = "-bmp";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "4980185e3776e89bd931736e1cddeee6";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, ISlow420565) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420_islow_565.bmp");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-rgb565";
  std::string arg4 = "-dither";
  std::string arg5 = "none";
  std::string arg6 = "-bmp";
  std::string arg7 = "-outfile";
  std::string arg8 = output_path.MaybeAsASCII();
  std::string arg9 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0], &arg9[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(10, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "bf9d13e16c4923b92e1faa604d7922cb";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, ISlow420565D) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420_islow_565D.bmp");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-rgb565";
  std::string arg4 = "-bmp";
  std::string arg5 = "-outfile";
  std::string arg6 = output_path.MaybeAsASCII();
  std::string arg7 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(8, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "6bde71526acc44bcff76f696df8638d2";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, ISlow420M565) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420m_islow_565.bmp");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-nosmooth";
  std::string arg4 = "-rgb565";
  std::string arg5 = "-dither";
  std::string arg6 = "none";
  std::string arg7 = "-bmp";
  std::string arg8 = "-outfile";
  std::string arg9 = output_path.MaybeAsASCII();
  std::string arg10 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0], &arg9[0], &arg10[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(11, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "8dc0185245353cfa32ad97027342216f";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, ISlow420M565D) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420m_islow_565D.bmp");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-nosmooth";
  std::string arg4 = "-rgb565";
  std::string arg5 = "-bmp";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "ce034037d212bc403330df6f915c161b";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, ISlow420Skip1531) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testorig.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420_islow_skip15_31.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-skip";
  std::string arg4 = "15,31";
  std::string arg5 = "-ppm";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "c4c65c1e43d7275cd50328a61e6534f0";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

#ifdef C_ARITH_CODING_SUPPORTED
TEST(DJPEGTest, ISlow420AriSkip16139) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testimgari.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII(
                              "testout_420_islow_ari_skip16_139.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-skip";
  std::string arg4 = "16,139";
  std::string arg5 = "-ppm";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "087c6b123db16ac00cb88c5b590bb74a";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, ISlow420AriCrop53x5344) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testimgari.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII(
                              "testout_420_islow_ari_crop53x53_4_4.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-crop";
  std::string arg4 = "53x53+4+4";
  std::string arg5 = "-ppm";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "886c6775af22370257122f8b16207e6d";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, IFast420MAri) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testimgari.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420m_ifast_ari.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-fast";
  std::string arg2 = "-ppm";
  std::string arg3 = "-outfile";
  std::string arg4 = output_path.MaybeAsASCII();
  std::string arg5 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(6, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "72b59a99bcf1de24c5b27d151bde2437";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, ISlow444AriCrop37x3700) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_444_islow_ari.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII(
                              "testout_444_islow_ari_crop37x37_0_0.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-crop";
  std::string arg4 = "37x37+0+0";
  std::string arg5 = "-ppm";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "cb57b32bd6d03e35432362f7bf184b6d";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}
#endif

TEST(DJPEGTest, RGBISlow) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_rgb_islow.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_rgb_islow.ppm");
  base::FilePath output_icc_path(GetTargetDirectory());
  output_icc_path = output_icc_path.AppendASCII("testout_rgb_islow.icc");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-ppm";
  std::string arg4 = "-icc";
  std::string arg5 = output_icc_path.MaybeAsASCII();
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "00a257f5393fef8821f2b88ac7421291";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
  const std::string ICC_EXPECTED_MD5 = "b06a39d730129122e85c1363ed1bbc9e";
  EXPECT_TRUE(CompareFileAndMD5(output_icc_path, ICC_EXPECTED_MD5));
}

TEST(DJPEGTest, RGBISlow565) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_rgb_islow.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_rgb_islow_565.bmp");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-rgb565";
  std::string arg4 = "-dither";
  std::string arg5 = "none";
  std::string arg6 = "-bmp";
  std::string arg7 = "-outfile";
  std::string arg8 = output_path.MaybeAsASCII();
  std::string arg9 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0], &arg9[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(10, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "f07d2e75073e4bb10f6c6f4d36e2e3be";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, RGBISlow565D) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_rgb_islow.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_rgb_islow_565D.bmp");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-rgb565";
  std::string arg4 = "-bmp";
  std::string arg5 = "-outfile";
  std::string arg6 = output_path.MaybeAsASCII();
  std::string arg7 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(8, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "4cfa0928ef3e6bb626d7728c924cfda4";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, IFast422) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_422_ifast_opt.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_422_ifast.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "fast";
  std::string arg3 = "-outfile";
  std::string arg4 = output_path.MaybeAsASCII();
  std::string arg5 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(6, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "35bd6b3f833bad23de82acea847129fa";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, IFast422M) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_422_ifast_opt.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_422m_ifast.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "fast";
  std::string arg3 = "-nosmooth";
  std::string arg4 = "-outfile";
  std::string arg5 = output_path.MaybeAsASCII();
  std::string arg6 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(7, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "8dbc65323d62cca7c91ba02dd1cfa81d";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, IFast422M565) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_422_ifast_opt.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_422m_ifast_565.bmp");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-nosmooth";
  std::string arg4 = "-rgb565";
  std::string arg5 = "-dither";
  std::string arg6 = "none";
  std::string arg7 = "-bmp";
  std::string arg8 = "-outfile";
  std::string arg9 = output_path.MaybeAsASCII();
  std::string arg10 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0], &arg9[0], &arg10[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(11, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "3294bd4d9a1f2b3d08ea6020d0db7065";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, IFast422M565D) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_422_ifast_opt.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_422m_ifast_565D.bmp");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-nosmooth";
  std::string arg4 = "-rgb565";
  std::string arg5 = "-bmp";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "da98c9c7b6039511be4a79a878a9abc1";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, IFastProg420Q100) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_420_q100_ifast_prog.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420_q100_ifast.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "fast";
  std::string arg3 = "-outfile";
  std::string arg4 = output_path.MaybeAsASCII();
  std::string arg5 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(6, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "5a732542015c278ff43635e473a8a294";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, IFastProg420MQ100) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_420_q100_ifast_prog.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_420m_q100_ifast.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "fast";
  std::string arg3 = "-nosmooth";
  std::string arg4 = "-outfile";
  std::string arg5 = output_path.MaybeAsASCII();
  std::string arg6 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(7, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "ff692ee9323a3b424894862557c092f1";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, GrayISlow) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_gray_islow.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_gray_islow.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-outfile";
  std::string arg4 = output_path.MaybeAsASCII();
  std::string arg5 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(6, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "8d3596c56eace32f205deccc229aa5ed";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, GrayISlowRGB) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_gray_islow.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_gray_islow_rgb.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-rgb";
  std::string arg4 = "-outfile";
  std::string arg5 = output_path.MaybeAsASCII();
  std::string arg6 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(7, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "116424ac07b79e5e801f00508eab48ec";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, GrayISlow565) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_gray_islow.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_gray_islow_565.bmp");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-rgb565";
  std::string arg4 = "-dither";
  std::string arg5 = "none";
  std::string arg6 = "-bmp";
  std::string arg7 = "-outfile";
  std::string arg8 = output_path.MaybeAsASCII();
  std::string arg9 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0], &arg9[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(10, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "12f78118e56a2f48b966f792fedf23cc";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, GrayISlow565D) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_gray_islow.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_gray_islow_565D.bmp");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-rgb565";
  std::string arg4 = "-bmp";
  std::string arg5 = "-outfile";
  std::string arg6 = output_path.MaybeAsASCII();
  std::string arg7 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(8, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "bdbbd616441a24354c98553df5dc82db";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, FloatProg3x2) {

  base::FilePath input_image_path;
#if defined(WITH_SIMD) && (defined(__i386__) || defined(__x86_64__))
  GetTestFilePath(&input_image_path, "testout_3x2_float_prog_sse.jpg");
#else
  GetTestFilePath(&input_image_path, "testout_3x2_float_prog.jpg");
#endif
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_3x2_float.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "float";
  std::string arg3 = "-outfile";
  std::string arg4 = output_path.MaybeAsASCII();
  std::string arg5 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(6, command_line), 0);

  // Compare expected MD5 sum against that of test image.
#if defined(WITH_SIMD) && (defined(__i386__) || defined(__x86_64__))
  const std::string EXPECTED_MD5 = "1a75f36e5904d6fc3a85a43da9ad89bb";
#else
  const std::string EXPECTED_MD5 = "f6bfab038438ed8f5522fbd33595dcdc";
#endif
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, IFastProg3x2) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_3x2_ifast_prog.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_3x2_ifast.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "fast";
  std::string arg3 = "-outfile";
  std::string arg4 = output_path.MaybeAsASCII();
  std::string arg5 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(6, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "fd283664b3b49127984af0a7f118fccd";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, ISlowProgCrop62x627171) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_420_islow_prog.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII(
                              "testout_420_islow_prog_crop62x62_71_71.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-crop";
  std::string arg4 = "62x62+71+71";
  std::string arg5 = "-ppm";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "26eb36ccc7d1f0cb80cdabb0ac8b5d99";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, ISlow444Skip16) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_444_islow.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII("testout_444_islow_skip1_6.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-skip";
  std::string arg4 = "1,6";
  std::string arg5 = "-ppm";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "5606f86874cf26b8fcee1117a0a436a6";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}

TEST(DJPEGTest, ISlowProg444Crop98x981313) {

  base::FilePath input_image_path;
  GetTestFilePath(&input_image_path, "testout_444_islow_prog.jpg");
  base::FilePath output_path(GetTargetDirectory());
  output_path = output_path.AppendASCII(
                              "testout_444_islow_prog_crop98x98_13_13.ppm");

  std::string prog_name = "djpeg";
  std::string arg1 = "-dct";
  std::string arg2 = "int";
  std::string arg3 = "-crop";
  std::string arg4 = "98x98+13+13";
  std::string arg5 = "-ppm";
  std::string arg6 = "-outfile";
  std::string arg7 = output_path.MaybeAsASCII();
  std::string arg8 = input_image_path.MaybeAsASCII();

  char *command_line[] = { &prog_name[0],
                           &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                           &arg6[0], &arg7[0], &arg8[0]
                         };
  // Generate test image file.
  EXPECT_EQ(djpeg(9, command_line), 0);

  // Compare expected MD5 sum against that of test image.
  const std::string EXPECTED_MD5 = "db87dc7ce26bcdc7a6b56239ce2b9d6c";
  EXPECT_TRUE(CompareFileAndMD5(output_path, EXPECTED_MD5));
}
