# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//console-header.star", "HEADER")

luci.console_view(
    name = "chromium.webrtc.fyi",
    repo = "https://webrtc.googlesource.com/src",
    refs = ["refs/heads/master", "refs/heads/main"],
    header = HEADER,
    entries = [
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI Android Builder (dbg)",
            category = "android|debug|builder",
            short_name = "32",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI Android Builder ARM64 (dbg)",
            category = "android|debug|builder",
            short_name = "64",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI Android Tests (dbg)",
            category = "android|debug|tester",
            short_name = "32",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI Android Tests ARM64 (dbg)",
            category = "android|debug|tester",
            short_name = "64",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI Android Builder",
            category = "android|release",
            short_name = "32",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI Linux Builder (dbg)",
            category = "linux|debug",
            short_name = "bld",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI Linux Builder",
            category = "linux|release",
            short_name = "bld",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI Linux Tester",
            category = "linux|release",
            short_name = "tst",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI Mac Builder (dbg)",
            category = "mac|debug",
            short_name = "bld",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI Mac Builder",
            category = "mac|release",
            short_name = "bld",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI Mac Tester",
            category = "mac|release",
            short_name = "tst",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI Win Builder (dbg)",
            category = "win|debug",
            short_name = "bld",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI Win Builder",
            category = "win|release|builder",
            short_name = "32",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI Win10 Tester",
            category = "win|release|tester",
            short_name = "10",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI ios-device",
            category = "ios",
            short_name = "dev",
        ),
        luci.console_view_entry(
            builder = "webrtc.fyi/WebRTC Chromium FYI ios-simulator",
            category = "ios",
            short_name = "sim",
        ),
    ],
)
