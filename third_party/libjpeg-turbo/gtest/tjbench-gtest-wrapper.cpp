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

extern "C" int tjbench(int argc, char *argv[]);

// Test image files and their expected MD5 sums.
const static std::vector<std::pair<const std::string,
                                   const std::string>> IMAGE_MD5_BASELINE = {
  { "testout_tile_GRAY_Q95_8x8.ppm", "89d3ca21213d9d864b50b4e4e7de4ca6" },
  { "testout_tile_420_Q95_8x8.ppm", "847fceab15c5b7b911cb986cf0f71de3" },
  { "testout_tile_422_Q95_8x8.ppm", "d83dacd9fc73b0a6f10c09acad64eb1e" },
  { "testout_tile_444_Q95_8x8.ppm", "7964e41e67cfb8d0a587c0aa4798f9c3" },
  { "testout_tile_GRAY_Q95_16x16.ppm", "89d3ca21213d9d864b50b4e4e7de4ca6" },
  { "testout_tile_420_Q95_16x16.ppm", "ca45552a93687e078f7137cc4126a7b0" },
  { "testout_tile_422_Q95_16x16.ppm", "35077fb610d72dd743b1eb0cbcfe10fb" },
  { "testout_tile_444_Q95_16x16.ppm", "7964e41e67cfb8d0a587c0aa4798f9c3" },
  { "testout_tile_GRAY_Q95_32x32.ppm", "89d3ca21213d9d864b50b4e4e7de4ca6" },
  { "testout_tile_420_Q95_32x32.ppm", "d8676f1d6b68df358353bba9844f4a00" },
  { "testout_tile_422_Q95_32x32.ppm", "e6902ed8a449ecc0f0d6f2bf945f65f7" },
  { "testout_tile_444_Q95_32x32.ppm", "7964e41e67cfb8d0a587c0aa4798f9c3" },
  { "testout_tile_GRAY_Q95_64x64.ppm", "89d3ca21213d9d864b50b4e4e7de4ca6" },
  { "testout_tile_420_Q95_64x64.ppm", "4e4c1a3d7ea4bace4f868bcbe83b7050" },
  { "testout_tile_422_Q95_64x64.ppm", "2b4502a8f316cedbde1da7bce3d2231e" },
  { "testout_tile_444_Q95_64x64.ppm", "7964e41e67cfb8d0a587c0aa4798f9c3" },
  { "testout_tile_GRAY_Q95_128x128.ppm", "89d3ca21213d9d864b50b4e4e7de4ca6" },
  { "testout_tile_420_Q95_128x128.ppm", "f24c3429c52265832beab9df72a0ceae" },
  { "testout_tile_422_Q95_128x128.ppm", "f0b5617d578f5e13c8eee215d64d4877" },
  { "testout_tile_444_Q95_128x128.ppm", "7964e41e67cfb8d0a587c0aa4798f9c3" }
};

class TJBenchTest : public
  ::testing::TestWithParam<std::pair<const std::string, const std::string>> {

 protected:

  static void SetUpTestSuite() {
    base::FilePath resource_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &resource_path));
    resource_path = resource_path.AppendASCII("third_party");
    resource_path = resource_path.AppendASCII("libjpeg_turbo");
    resource_path = resource_path.AppendASCII("testimages");
    resource_path = resource_path.AppendASCII("testorig.ppm");
    ASSERT_TRUE(base::PathExists(resource_path));

    base::FilePath target_path(GetTargetDirectory());
    target_path = target_path.AppendASCII("testout_tile.ppm");

    ASSERT_TRUE(base::CopyFile(resource_path, target_path));

    std::string prog_name = "tjbench";
    std::string arg1 = target_path.MaybeAsASCII();
    std::string arg2 = "95";
    std::string arg3 = "-rgb";
    std::string arg4 = "-quiet";
    std::string arg5 = "-tile";
    std::string arg6 = "-benchtime";
    std::string arg7 = "0.01";
    std::string arg8 = "-warmup";
    std::string arg9 = "0";
    char *command_line[] = { &prog_name[0],
                             &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                             &arg6[0], &arg7[0], &arg8[0], &arg9[0],
                           };
    // Generate test image tiles.
    EXPECT_EQ(tjbench(10, command_line), 0);
  }

};

TEST_P(TJBenchTest, TestTileBaseline) {
  // Construct path for test image file.
  base::FilePath test_image_path(GetTargetDirectory());
  test_image_path = test_image_path.AppendASCII(std::get<0>(GetParam()));
  // Read test image as string and compute MD5 sum.
  std::string test_image_data;
  ASSERT_TRUE(base::ReadFileToString(test_image_path, &test_image_data));
  const std::string md5 = base::MD5String(test_image_data);
  // Compare expected MD5 sum against that of test image.
  EXPECT_EQ(std::get<1>(GetParam()), md5);
}

INSTANTIATE_TEST_SUITE_P(TestTileBaseline,
                         TJBenchTest,
                         ::testing::ValuesIn(IMAGE_MD5_BASELINE));

// Test image files and their expected MD5 sums.
const static std::vector<std::pair<const std::string,
                                   const std::string>> IMAGE_MD5_MERGED = {
  { "testout_tilem_420_Q95_8x8.ppm", "bc25320e1f4c31ce2e610e43e9fd173c" },
  { "testout_tilem_422_Q95_8x8.ppm", "828941d7f41cd6283abd6beffb7fd51d" },
  { "testout_tilem_420_Q95_16x16.ppm", "75ffdf14602258c5c189522af57fa605" },
  { "testout_tilem_422_Q95_16x16.ppm", "e877ae1324c4a280b95376f7f018172f" },
  { "testout_tilem_420_Q95_32x32.ppm", "75ffdf14602258c5c189522af57fa605" },
  { "testout_tilem_422_Q95_32x32.ppm", "e877ae1324c4a280b95376f7f018172f" },
  { "testout_tilem_420_Q95_64x64.ppm", "75ffdf14602258c5c189522af57fa605" },
  { "testout_tilem_422_Q95_64x64.ppm", "e877ae1324c4a280b95376f7f018172f" },
  { "testout_tilem_420_Q95_128x128.ppm", "75ffdf14602258c5c189522af57fa605" },
  { "testout_tilem_422_Q95_128x128.ppm", "e877ae1324c4a280b95376f7f018172f" }
};

class TJBenchTestMerged : public
  ::testing::TestWithParam<std::pair<const std::string, const std::string>> {

 protected:

  static void SetUpTestSuite() {
    base::FilePath resource_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &resource_path));
    resource_path = resource_path.AppendASCII("third_party");
    resource_path = resource_path.AppendASCII("libjpeg_turbo");
    resource_path = resource_path.AppendASCII("testimages");
    resource_path = resource_path.AppendASCII("testorig.ppm");
    ASSERT_TRUE(base::PathExists(resource_path));

    base::FilePath target_path(GetTargetDirectory());
    target_path = target_path.AppendASCII("testout_tilem.ppm");

    ASSERT_TRUE(base::CopyFile(resource_path, target_path));

    std::string prog_name = "tjbench";
    std::string arg1 = target_path.MaybeAsASCII();
    std::string arg2 = "95";
    std::string arg3 = "-rgb";
    std::string arg4 = "-fastupsample";
    std::string arg5 = "-quiet";
    std::string arg6 = "-tile";
    std::string arg7 = "-benchtime";
    std::string arg8 = "0.01";
    std::string arg9 = "-warmup";
    std::string arg10 = "0";
    char *command_line[] = { &prog_name[0],
                             &arg1[0], &arg2[0], &arg3[0], &arg4[0], &arg5[0],
                             &arg6[0], &arg7[0], &arg8[0], &arg9[0], &arg10[0]
                           };
    // Generate test image output tiles.
    EXPECT_EQ(tjbench(11, command_line), 0);
  }

};

TEST_P(TJBenchTestMerged, TestTileMerged) {
  // Construct path for test image file.
  base::FilePath test_image_path(GetTargetDirectory());
  test_image_path = test_image_path.AppendASCII(std::get<0>(GetParam()));
  // Read test image as string and compute MD5 sum.
  std::string test_image_data;
  ASSERT_TRUE(base::ReadFileToString(test_image_path, &test_image_data));
  const std::string md5 = base::MD5String(test_image_data);
  // Compare expected MD5 sum against that of test image.
  EXPECT_EQ(std::get<1>(GetParam()), md5);
}

INSTANTIATE_TEST_SUITE_P(TestTileMerged,
                         TJBenchTestMerged,
                         ::testing::ValuesIn(IMAGE_MD5_MERGED));
