// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/address_sorter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "net/base/address_list.h"
#include "net/base/net_util.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

IPEndPoint MakeEndPoint(const std::string& str) {
  IPAddressNumber addr;
  CHECK(ParseIPLiteralToNumber(str, &addr));
  return IPEndPoint(addr, 0);
}

void OnSortComplete(AddressList* result_buf,
                    const CompletionCallback& callback,
                    bool success,
                    const AddressList& result) {
  EXPECT_TRUE(success);
  if (success)
    *result_buf = result;
  callback.Run(OK);
}

TEST(AddressSorterTest, Sort) {
  scoped_ptr<AddressSorter> sorter(AddressSorter::CreateAddressSorter());
  AddressList list;
  list.push_back(MakeEndPoint("10.0.0.1"));
  list.push_back(MakeEndPoint("8.8.8.8"));
  list.push_back(MakeEndPoint("::1"));
  list.push_back(MakeEndPoint("2001:4860:4860::8888"));

  AddressList result;
  TestCompletionCallback callback;
  sorter->Sort(list, base::Bind(&OnSortComplete, &result,
                                callback.callback()));
  callback.WaitForResult();
}

}  // namespace
}  // namespace net
