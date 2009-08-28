// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_SERVER_TYPE_HISTOGRAMS_H_
#define NET_FTP_FTP_SERVER_TYPE_HISTOGRAMS_H_

// The UpdateFtpServerTypeHistograms function collects statistics related
// to the types of FTP servers that our users are encountering.  The
// information will help us decide for what servers we can safely drop support.
// The parsing code for each type of server is very complex, which poses a
// security risk. In fact, in current shape it's a disaster waiting to happen.

namespace net {

enum FtpServerType {
  // Record cases in which we couldn't parse the server's response. That means
  // a server type we don't recognize, a security attack (when what we're
  // connecting to isn't an FTP server), or a broken server.
  SERVER_UNKNOWN = 0,

  SERVER_LSL = 1,   // Server using /bin/ls -l and variants.
  SERVER_DLS = 2,   // Server using /bin/dls.
  SERVER_EPLF = 3,  // Server using EPLF format.
  SERVER_DOS = 4,   // WinNT server configured for old style listing.
  SERVER_VMS = 5,   // VMS (including variants).
  SERVER_CMS = 6,   // IBM VM/CMS, VM/ESA, z/VM formats.
  SERVER_OS2 = 7,   // OS/2 FTP Server.
  SERVER_W16 = 8,   // win16 hosts: SuperTCP or NetManage Chameleon.

  NUM_OF_SERVER_TYPES
};

void UpdateFtpServerTypeHistograms(FtpServerType type);

}  // namespace net

#endif  // NET_FTP_FTP_SERVER_TYPE_HISTOGRAMS_H_
