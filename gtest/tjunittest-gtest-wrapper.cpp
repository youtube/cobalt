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

#include <gtest/gtest.h>

extern "C" int testBmp(int yuv, int noyuvpad, int autoalloc);
extern "C" int testThreeByte444(int yuv, int noyuvpad, int autoalloc);
extern "C" int testFourByte444(int yuv, int noyuvpad, int autoalloc);
extern "C" int testThreeByte422(int yuv, int noyuvpad, int autoalloc);
extern "C" int testFourByte422(int yuv, int noyuvpad, int autoalloc);
extern "C" int testThreeByte420(int yuv, int noyuvpad, int autoalloc);
extern "C" int testFourByte420(int yuv, int noyuvpad, int autoalloc);
extern "C" int testThreeByte440(int yuv, int noyuvpad, int autoalloc);
extern "C" int testFourByte440(int yuv, int noyuvpad, int autoalloc);
extern "C" int testThreeByte411(int yuv, int noyuvpad, int autoalloc);
extern "C" int testFourByte411(int yuv, int noyuvpad, int autoalloc);
extern "C" int testOnlyGray(int yuv, int noyuvpad, int autoalloc);
extern "C" int testThreeByteGray(int yuv, int noyuvpad, int autoalloc);
extern "C" int testFourByteGray(int yuv, int noyuvpad, int autoalloc);
extern "C" int testBufSize(int yuv, int noyuvpad, int autoalloc);
extern "C" int testYUVOnlyRGB444(int noyuvpad, int autoalloc);
extern "C" int testYUVOnlyRGB422(int noyuvpad, int autoalloc);
extern "C" int testYUVOnlyRGB420(int noyuvpad, int autoalloc);
extern "C" int testYUVOnlyRGB440(int noyuvpad, int autoalloc);
extern "C" int testYUVOnlyRGB411(int noyuvpad, int autoalloc);
extern "C" int testYUVOnlyRGBGray(int noyuvpad, int autoalloc);
extern "C" int testYUVOnlyGrayGray(int noyuvpad, int autoalloc);

const int YUV = 1;
const int NO_YUV = 0;
const int NO_YUV_PAD = 1;
const int YUV_PAD = 0;
const int AUTO_ALLOC = 1;
const int NO_AUTO_ALLOC = 0;

class TJUnitTest : public
  ::testing::TestWithParam<std::tuple<int, int, int>> {};

TEST_P(TJUnitTest, BMP) {
  EXPECT_EQ(testBmp(std::get<0>(GetParam()),
                    std::get<1>(GetParam()),
                    std::get<2>(GetParam())), 0);
}

TEST_P(TJUnitTest, ThreeByte444) {
  EXPECT_EQ(testThreeByte444(std::get<0>(GetParam()),
                             std::get<1>(GetParam()),
                             std::get<2>(GetParam())), 0);
}

TEST_P(TJUnitTest, FourByte444) {
  EXPECT_EQ(testFourByte444(std::get<0>(GetParam()),
                            std::get<1>(GetParam()),
                            std::get<2>(GetParam())), 0);
}

TEST_P(TJUnitTest, ThreeByte422) {
  EXPECT_EQ(testThreeByte422(std::get<0>(GetParam()),
                             std::get<1>(GetParam()),
                             std::get<2>(GetParam())), 0);
}

TEST_P(TJUnitTest, FourByte422) {
  EXPECT_EQ(testFourByte422(std::get<0>(GetParam()),
                            std::get<1>(GetParam()),
                            std::get<2>(GetParam())), 0);
}

TEST_P(TJUnitTest, ThreeByte420) {
  EXPECT_EQ(testThreeByte420(std::get<0>(GetParam()),
                             std::get<1>(GetParam()),
                             std::get<2>(GetParam())), 0);
}

TEST_P(TJUnitTest, FourByte420) {
  EXPECT_EQ(testFourByte420(std::get<0>(GetParam()),
                            std::get<1>(GetParam()),
                            std::get<2>(GetParam())), 0);
}

TEST_P(TJUnitTest, ThreeByte440) {
  EXPECT_EQ(testThreeByte440(std::get<0>(GetParam()),
                             std::get<1>(GetParam()),
                             std::get<2>(GetParam())), 0);
}

TEST_P(TJUnitTest, FourByte440) {
  EXPECT_EQ(testFourByte440(std::get<0>(GetParam()),
                            std::get<1>(GetParam()),
                            std::get<2>(GetParam())), 0);
}

TEST_P(TJUnitTest, ThreeByte411) {
  EXPECT_EQ(testThreeByte411(std::get<0>(GetParam()),
                             std::get<1>(GetParam()),
                             std::get<2>(GetParam())), 0);
}

TEST_P(TJUnitTest, FourByte411) {
  EXPECT_EQ(testFourByte411(std::get<0>(GetParam()),
                            std::get<1>(GetParam()),
                            std::get<2>(GetParam())), 0);
}

TEST_P(TJUnitTest, OnlyGray) {
  EXPECT_EQ(testOnlyGray(std::get<0>(GetParam()),
                         std::get<1>(GetParam()),
                         std::get<2>(GetParam())), 0);
}

TEST_P(TJUnitTest, ThreeByteGray) {
  EXPECT_EQ(testThreeByteGray(std::get<0>(GetParam()),
                              std::get<1>(GetParam()),
                              std::get<2>(GetParam())), 0);
}

TEST_P(TJUnitTest, FourByteGray) {
  EXPECT_EQ(testFourByteGray(std::get<0>(GetParam()),
                             std::get<1>(GetParam()),
                             std::get<2>(GetParam())), 0);
}

TEST_P(TJUnitTest, BufSize) {
  EXPECT_EQ(testBufSize(std::get<0>(GetParam()),
                        std::get<1>(GetParam()),
                        std::get<2>(GetParam())), 0);
}

INSTANTIATE_TEST_SUITE_P(
        TJUnitTests,
        TJUnitTest,
        ::testing::Values(std::make_tuple(NO_YUV, YUV_PAD, NO_AUTO_ALLOC),
                          std::make_tuple(NO_YUV, YUV_PAD, AUTO_ALLOC),
                          std::make_tuple(NO_YUV, NO_YUV_PAD, NO_AUTO_ALLOC),
                          std::make_tuple(NO_YUV, NO_YUV_PAD, AUTO_ALLOC),
                          std::make_tuple(YUV, YUV_PAD, NO_AUTO_ALLOC),
                          std::make_tuple(YUV, YUV_PAD, AUTO_ALLOC),
                          std::make_tuple(YUV, NO_YUV_PAD, NO_AUTO_ALLOC),
                          std::make_tuple(YUV, NO_YUV_PAD, AUTO_ALLOC)));

class TJUnitTestYUV : public ::testing::TestWithParam<std::tuple<int, int>> {};

TEST_P(TJUnitTestYUV, YUVOnlyRGB444) {
  EXPECT_EQ(testYUVOnlyRGB444(std::get<0>(GetParam()),
                              std::get<1>(GetParam())), 0);
}

TEST_P(TJUnitTestYUV, YUVOnlyRGB422) {
  EXPECT_EQ(testYUVOnlyRGB422(std::get<0>(GetParam()),
                              std::get<1>(GetParam())), 0);
}

TEST_P(TJUnitTestYUV, YUVOnlyRGB420) {
  EXPECT_EQ(testYUVOnlyRGB420(std::get<0>(GetParam()),
                              std::get<1>(GetParam())), 0);
}

TEST_P(TJUnitTestYUV, YUVOnlyRGB440) {
  EXPECT_EQ(testYUVOnlyRGB440(std::get<0>(GetParam()),
                              std::get<1>(GetParam())), 0);
}

TEST_P(TJUnitTestYUV, YUVOnlyRGB411) {
  EXPECT_EQ(testYUVOnlyRGB411(std::get<0>(GetParam()),
                              std::get<1>(GetParam())), 0);
}

TEST_P(TJUnitTestYUV, YUVOnlyRGBGray) {
  EXPECT_EQ(testYUVOnlyRGBGray(std::get<0>(GetParam()),
                               std::get<1>(GetParam())), 0);
}

TEST_P(TJUnitTestYUV, YUVOnlyGrayGray) {
  EXPECT_EQ(testYUVOnlyGrayGray(std::get<0>(GetParam()),
                                std::get<1>(GetParam())), 0);
}

INSTANTIATE_TEST_SUITE_P(
        TJUnitTestsYUV,
        TJUnitTestYUV,
        ::testing::Values(std::make_tuple(YUV_PAD, NO_AUTO_ALLOC),
                          std::make_tuple(YUV_PAD, AUTO_ALLOC),
                          std::make_tuple(NO_YUV_PAD, NO_AUTO_ALLOC),
                          std::make_tuple(NO_YUV_PAD, AUTO_ALLOC)));
