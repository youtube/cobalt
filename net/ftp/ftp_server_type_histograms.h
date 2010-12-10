// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_SERVER_TYPE_HISTOGRAMS_H_
#define NET_FTP_FTP_SERVER_TYPE_HISTOGRAMS_H_
#pragma once

// The UpdateFtpServerTypeHistograms function collects statistics related
// to the types of FTP servers that our users are encountering.

namespace net {

enum FtpServerType {
  // Record cases in which we couldn't parse the server's response. That means
  // a server type we don't recognize, a security attack (when what we're
  // connecting to isn't an FTP server), or a broken server.
  SERVER_UNKNOWN = 0,

  // Types 1-8 are RESERVED (were earlier used for listings recognized by
  // Mozilla's ParseFTPList code).

  SERVER_LS = 9,        // Server using /bin/ls -l listing style.
  SERVER_WINDOWS = 10,  // Server using Windows listing style.
  SERVER_VMS = 11,      // Server using VMS listing style.
  SERVER_NETWARE = 12,  // Server using Netware listing style.

  // Types 13-14 are RESERVED (were earlier used for MLSD listings).

  NUM_OF_SERVER_TYPES
};

void UpdateFtpServerTypeHistograms(FtpServerType type);

}  // namespace net

#endif  // NET_FTP_FTP_SERVER_TYPE_HISTOGRAMS_H_
