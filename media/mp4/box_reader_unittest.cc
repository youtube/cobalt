// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/mp4/box_reader.h"
#include "media/mp4/rcheck.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace mp4 {

static const uint8 kTestBox[] = {
  // Test box containing three children
  0x00, 0x00, 0x00, 0x40, 't', 'e', 's', 't',
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
  0xf9, 0x0a, 0x0b, 0x0c, 0xfd, 0x0e, 0x0f, 0x10,
  // Ordinary child box
  0x00, 0x00, 0x00, 0x0c,  'c',  'h',  'l',  'd', 0xde, 0xad, 0xbe, 0xef,
  // Extended-size child box
  0x00, 0x00, 0x00, 0x01,  'c',  'h',  'l',  'd',
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14,
  0xfa, 0xce, 0xca, 0xfe,
  // Empty box
  0x00, 0x00, 0x00, 0x08,  'm',  'p',  't',  'y',
  // Trailing garbage
  0x00 };

struct EmptyBox : Box {
  virtual bool Parse(BoxReader* reader) OVERRIDE {
    return true;
  }
  virtual FourCC BoxType() const OVERRIDE { return FOURCC_MPTY; }
};

struct ChildBox : Box {
  uint32 val;

  virtual bool Parse(BoxReader* reader) OVERRIDE {
    return reader->Read4(&val);
  }
  virtual FourCC BoxType() const OVERRIDE { return FOURCC_CHLD; }
};

struct TestBox : Box {
  uint8 a, b;
  uint16 c;
  int32 d;
  int64 e;

  std::vector<ChildBox> kids;
  EmptyBox mpty;

  virtual bool Parse(BoxReader* reader) OVERRIDE {
    RCHECK(reader->ReadFullBoxHeader() &&
           reader->Read1(&a) &&
           reader->Read1(&b) &&
           reader->Read2(&c) &&
           reader->Read4s(&d) &&
           reader->Read4sInto8s(&e));
    return reader->ScanChildren() &&
           reader->ReadChildren(&kids) &&
           reader->MaybeReadChild(&mpty);
  }
  virtual FourCC BoxType() const OVERRIDE { return FOURCC_TEST; }

  TestBox();
  ~TestBox();
};

TestBox::TestBox() {}
TestBox::~TestBox() {}

class BoxReaderTest : public testing::Test {
 protected:
  std::vector<uint8> GetBuf() {
    return std::vector<uint8>(kTestBox, kTestBox + sizeof(kTestBox));
  }
};

TEST_F(BoxReaderTest, ExpectedOperationTest) {
  std::vector<uint8> buf = GetBuf();
  bool err;
  scoped_ptr<BoxReader> reader(
      BoxReader::ReadTopLevelBox(&buf[0], buf.size(), &err));
  EXPECT_FALSE(err);
  EXPECT_TRUE(reader.get());

  TestBox box;
  EXPECT_TRUE(box.Parse(reader.get()));
  EXPECT_EQ(0x01, reader->version());
  EXPECT_EQ(0x020304u, reader->flags());
  EXPECT_EQ(0x05, box.a);
  EXPECT_EQ(0x06, box.b);
  EXPECT_EQ(0x0708, box.c);
  EXPECT_EQ(static_cast<int32>(0xf90a0b0c), box.d);
  EXPECT_EQ(static_cast<int32>(0xfd0e0f10), box.e);

  EXPECT_EQ(2u, box.kids.size());
  EXPECT_EQ(0xdeadbeef, box.kids[0].val);
  EXPECT_EQ(0xfacecafe, box.kids[1].val);

  // Accounting for the extra byte outside of the box above
  EXPECT_EQ(buf.size(), static_cast<uint64>(reader->size() + 1));
}

TEST_F(BoxReaderTest, OuterTooShortTest) {
  std::vector<uint8> buf = GetBuf();
  bool err;

  // Create a soft failure by truncating the outer box.
  scoped_ptr<BoxReader> r(
      BoxReader::ReadTopLevelBox(&buf[0], buf.size() - 2, &err));

  EXPECT_FALSE(err);
  EXPECT_FALSE(r.get());
}

TEST_F(BoxReaderTest, InnerTooLongTest) {
  std::vector<uint8> buf = GetBuf();
  bool err;

  // Make an inner box too big for its outer box.
  buf[25] = 1;
  scoped_ptr<BoxReader> reader(
      BoxReader::ReadTopLevelBox(&buf[0], buf.size(), &err));

  TestBox box;
  EXPECT_FALSE(box.Parse(reader.get()));
}

TEST_F(BoxReaderTest, WrongFourCCTest) {
  std::vector<uint8> buf = GetBuf();
  bool err;

  // Use an unknown FourCC both on an outer box and an inner one.
  buf[5] = 1;
  buf[28] = 1;
  scoped_ptr<BoxReader> reader(
      BoxReader::ReadTopLevelBox(&buf[0], buf.size(), &err));

  TestBox box;
  std::vector<ChildBox> kids;
  // This should still work; the outer box reader doesn't care about the FourCC,
  // since it assumes you've already examined it before deciding what to parse.
  EXPECT_TRUE(box.Parse(reader.get()));
  EXPECT_EQ(0x74017374, reader->type());
  // Parsing the TestBox should have left the modified inner box unread, which
  // we collect here.
  EXPECT_TRUE(reader->ReadAllChildren(&kids));
  EXPECT_EQ(1u, kids.size());
  EXPECT_EQ(0xdeadbeef, kids[0].val);
}

TEST_F(BoxReaderTest, ChildrenTest) {
  std::vector<uint8> buf = GetBuf();
  bool err;
  scoped_ptr<BoxReader> reader(
      BoxReader::ReadTopLevelBox(&buf[0], buf.size(), &err));

  EXPECT_TRUE(reader->SkipBytes(16) && reader->ScanChildren());

  EmptyBox mpty;
  EXPECT_TRUE(reader->ReadChild(&mpty));
  EXPECT_FALSE(reader->ReadChild(&mpty));
  EXPECT_TRUE(reader->MaybeReadChild(&mpty));

  std::vector<ChildBox> kids;

  EXPECT_TRUE(reader->ReadAllChildren(&kids));
  EXPECT_EQ(2u, kids.size());
  kids.clear();
  EXPECT_FALSE(reader->ReadChildren(&kids));
  EXPECT_TRUE(reader->MaybeReadChildren(&kids));
}

}  // namespace mp4
}  // namespace media
