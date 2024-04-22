// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/nix/xdg_util.h"

#include "base/environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace base {
namespace nix {

namespace {

class MockEnvironment : public Environment {
 public:
  MOCK_METHOD2(GetVar, bool(StringPiece, std::string* result));
  MOCK_METHOD2(SetVar, bool(StringPiece, const std::string& new_value));
  MOCK_METHOD1(UnSetVar, bool(StringPiece));
};

// Needs to be const char* to make gmock happy.
const char* const kDesktopGnome = "gnome";
const char* const kDesktopGnomeFallback = "gnome-fallback";
const char* const kDesktopMATE = "mate";
const char* const kDesktopKDE4 = "kde4";
const char* const kDesktopKDE = "kde";
const char* const kDesktopXFCE = "xfce";
const char* const kXdgDesktopCinnamon = "X-Cinnamon";
const char* const kXdgDesktopDeepin = "Deepin";
const char* const kXdgDesktopGNOME = "GNOME";
const char* const kXdgDesktopGNOMEClassic = "GNOME:GNOME-Classic";
const char* const kXdgDesktopKDE = "KDE";
const char* const kXdgDesktopPantheon = "Pantheon";
const char* const kXdgDesktopUKUI = "UKUI";
const char* const kXdgDesktopUnity = "Unity";
const char* const kXdgDesktopUnity7 = "Unity:Unity7";
const char* const kXdgDesktopUnity8 = "Unity:Unity8";
const char* const kKDESessionKDE5 = "5";
const char* const kKDESessionKDE6 = "6";

const char kDesktopSession[] = "DESKTOP_SESSION";
const char kKDESession[] = "KDE_SESSION_VERSION";

const char* const kSessionUnknown = "invalid session";
const char* const kSessionUnspecified = "unspecified";
const char* const kSessionTty = "tty";
const char* const kSessionMir = "mir";
const char* const kSessionX11 = "x11";
const char* const kSessionWayland = "wayland";
const char* const kSessionWaylandCapital = "Wayland";
const char* const kSessionWaylandWhitespace = "wayland ";

}  // namespace

TEST(XDGUtilTest, GetDesktopEnvironmentGnome) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kDesktopSession), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDesktopGnome), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_GNOME, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetDesktopEnvironmentMATE) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kDesktopSession), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDesktopMATE), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_GNOME, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetDesktopEnvironmentKDE4) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kDesktopSession), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDesktopKDE4), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_KDE4, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetDesktopEnvironmentKDE3) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kDesktopSession), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDesktopKDE), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_KDE3, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetDesktopEnvironmentXFCE) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kDesktopSession), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDesktopXFCE), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_XFCE, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetXdgDesktopCinnamon) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgCurrentDesktopEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kXdgDesktopCinnamon), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_CINNAMON, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetXdgDesktopDeepin) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgCurrentDesktopEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kXdgDesktopDeepin), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_DEEPIN, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetXdgDesktopGnome) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgCurrentDesktopEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kXdgDesktopGNOME), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_GNOME, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetXdgDesktopGnomeClassic) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgCurrentDesktopEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kXdgDesktopGNOMEClassic), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_GNOME, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetXdgDesktopGnomeFallback) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgCurrentDesktopEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kXdgDesktopUnity), Return(true)));
  EXPECT_CALL(getter, GetVar(Eq(kDesktopSession), _))
      .WillOnce(DoAll(SetArgPointee<1>(kDesktopGnomeFallback), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_GNOME, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetXdgDesktopKDE5) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgCurrentDesktopEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kXdgDesktopKDE), Return(true)));
  EXPECT_CALL(getter, GetVar(Eq(kKDESession), _))
      .WillOnce(DoAll(SetArgPointee<1>(kKDESessionKDE5), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_KDE5, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetXdgDesktopKDE6) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgCurrentDesktopEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kXdgDesktopKDE), Return(true)));
  EXPECT_CALL(getter, GetVar(Eq(kKDESession), _))
      .WillOnce(DoAll(SetArgPointee<1>(kKDESessionKDE6), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_KDE6, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetXdgDesktopKDE4) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgCurrentDesktopEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kXdgDesktopKDE), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_KDE4, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetXdgDesktopPantheon) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgCurrentDesktopEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kXdgDesktopPantheon), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_PANTHEON, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetXdgDesktopUKUI) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgCurrentDesktopEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kXdgDesktopUKUI), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_UKUI, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetXdgDesktopUnity) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgCurrentDesktopEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kXdgDesktopUnity), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_UNITY, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetXdgDesktopUnity7) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgCurrentDesktopEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kXdgDesktopUnity7), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_UNITY, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetXdgDesktopUnity8) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgCurrentDesktopEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kXdgDesktopUnity8), Return(true)));

  EXPECT_EQ(DESKTOP_ENVIRONMENT_UNITY, GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetXdgSessiontypeUnset) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));

  EXPECT_EQ(SessionType::kUnset, GetSessionType(getter));
}

TEST(XDGUtilTest, GetXdgSessionTypeOther) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgSessionTypeEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kSessionUnknown), Return(true)));

  EXPECT_EQ(SessionType::kOther, GetSessionType(getter));
}

TEST(XDGUtilTest, GetXdgSessionTypeUnspecified) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgSessionTypeEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kSessionUnspecified), Return(true)));

  EXPECT_EQ(SessionType::kUnspecified, GetSessionType(getter));
}

TEST(XDGUtilTest, GetXdgSessionTypeTty) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgSessionTypeEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kSessionTty), Return(true)));

  EXPECT_EQ(SessionType::kTty, GetSessionType(getter));
}

TEST(XDGUtilTest, GetXdgSessionTypeMir) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgSessionTypeEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kSessionMir), Return(true)));

  EXPECT_EQ(SessionType::kMir, GetSessionType(getter));
}

TEST(XDGUtilTest, GetXdgSessionTypeX11) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgSessionTypeEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kSessionX11), Return(true)));

  EXPECT_EQ(SessionType::kX11, GetSessionType(getter));
}

TEST(XDGUtilTest, GetXdgSessionTypeWayland) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgSessionTypeEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kSessionWayland), Return(true)));

  EXPECT_EQ(SessionType::kWayland, GetSessionType(getter));
}

TEST(XDGUtilTest, GetXdgSessionTypeWaylandCapital) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgSessionTypeEnvVar), _))
      .WillOnce(DoAll(SetArgPointee<1>(kSessionWaylandCapital), Return(true)));

  EXPECT_EQ(SessionType::kWayland, GetSessionType(getter));
}

TEST(XDGUtilTest, GetXdgSessionTypeWaylandWhitespace) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(Eq(kXdgSessionTypeEnvVar), _))
      .WillOnce(
          DoAll(SetArgPointee<1>(kSessionWaylandWhitespace), Return(true)));

  EXPECT_EQ(SessionType::kWayland, GetSessionType(getter));
}

}  // namespace nix
}  // namespace base
