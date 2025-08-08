// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included

IPCZ_MSG_BEGIN_INTERFACE(Test)

IPCZ_MSG_BEGIN(BasicTestMessage, IPCZ_MSG_ID(0), IPCZ_MSG_VERSION(0))
  IPCZ_MSG_PARAM(uint32_t, foo)
  IPCZ_MSG_PARAM(uint32_t, bar)
IPCZ_MSG_END()

IPCZ_MSG_BEGIN(MessageWithDataArray, IPCZ_MSG_ID(1), IPCZ_MSG_VERSION(0))
  IPCZ_MSG_PARAM_ARRAY(uint64_t, values)
IPCZ_MSG_END()

IPCZ_MSG_BEGIN(MessageWithDriverObject, IPCZ_MSG_ID(2), IPCZ_MSG_VERSION(0))
  IPCZ_MSG_PARAM_DRIVER_OBJECT(object)
IPCZ_MSG_END()

IPCZ_MSG_BEGIN(MessageWithDriverObjectArray,
               IPCZ_MSG_ID(3),
               IPCZ_MSG_VERSION(0))
  IPCZ_MSG_PARAM_DRIVER_OBJECT_ARRAY(objects)
IPCZ_MSG_END()

IPCZ_MSG_BEGIN(MessageWithDriverArrayAndExtraObject,
               IPCZ_MSG_ID(4),
               IPCZ_MSG_VERSION(0))
  IPCZ_MSG_PARAM_DRIVER_OBJECT_ARRAY(objects)
  IPCZ_MSG_PARAM_DRIVER_OBJECT(extra_object)
IPCZ_MSG_END()

IPCZ_MSG_END_INTERFACE()
