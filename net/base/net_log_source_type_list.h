// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: No header guards are used, since this file is intended to be expanded
// directly within a block where the SOURCE_TYPE macro is defined.

SOURCE_TYPE(NONE, -1)

SOURCE_TYPE(URL_REQUEST, 0)
SOURCE_TYPE(SOCKET_STREAM, 1)
SOURCE_TYPE(INIT_PROXY_RESOLVER, 2)
SOURCE_TYPE(CONNECT_JOB, 3)
SOURCE_TYPE(SOCKET, 4)
SOURCE_TYPE(SPDY_SESSION, 5)
SOURCE_TYPE(NETWORK_CHANGE_NOTIFIER, 6)

SOURCE_TYPE(COUNT, 7) // Always keep this as the last entry.
