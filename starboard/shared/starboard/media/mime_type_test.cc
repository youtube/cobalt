// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/shared/starboard/media/mime_type.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {
namespace {

TEST(MimeTypeTest, RawContentType) {
  {
    const char kContentTypeWithSpace[] = " video/mp4; name0=123; name1=123.4 ";
    MimeType mime_type(kContentTypeWithSpace);
    EXPECT_EQ(mime_type.raw_content_type(), kContentTypeWithSpace);
  }
  {
    const char kInvalidContentType[] = "video /mp4";
    MimeType mime_type(kInvalidContentType);
    EXPECT_EQ(mime_type.raw_content_type(), kInvalidContentType);
  }
}

TEST(MimeTypeTest, EmptyString) {
  MimeType mime_type("");
  EXPECT_FALSE(mime_type.is_valid());
}

// Valid mime type must have a type/subtype without space in between.
TEST(MimeTypeTest, InvalidMimeType) {
  {
    MimeType mime_type("video");
    EXPECT_FALSE(mime_type.is_valid());
  }

  {
    MimeType mime_type("video/");
    EXPECT_FALSE(mime_type.is_valid());
  }

  {
    MimeType mime_type("/mp4");
    EXPECT_FALSE(mime_type.is_valid());
  }

  {
    MimeType mime_type("video /mp4");
    EXPECT_FALSE(mime_type.is_valid());
  }

  {
    MimeType mime_type("video/ mp4");
    EXPECT_FALSE(mime_type.is_valid());
  }

  {
    MimeType mime_type("video / mp4");
    EXPECT_FALSE(mime_type.is_valid());
  }

  {  // "codecs" parameter isn't the first parameter.
    MimeType mime_type("audio/mp4;param1=\" value1 \";codecs=\"abc, def\"");
    EXPECT_FALSE(mime_type.is_valid());
  }

  {  // More than one "codecs" parameters.
    MimeType mime_type(
        "audio/mp4;codecs=\"abc, def\";"
        "param1=\" value1 \";codecs=\"abc, def\"");
    EXPECT_FALSE(mime_type.is_valid());
  }
  {
    // Parameter name must not be empty.
    MimeType mime_type("video/mp4; =value");
    EXPECT_FALSE(mime_type.is_valid());
  }
  {
    // Parameter value must not be empty.
    MimeType mime_type("video/mp4; name=");
    EXPECT_FALSE(mime_type.is_valid());
  }
  {
    // No '|' allowed.
    MimeType mime_type("video/mp4; name=ab|c");
    EXPECT_FALSE(mime_type.is_valid());
  }
}

TEST(MimeTypeTest, ValidContentTypeWithTypeAndSubtypeOnly) {
  {
    MimeType mime_type("video/mp4");
    EXPECT_TRUE(mime_type.is_valid());
    EXPECT_EQ("video", mime_type.type());
    EXPECT_EQ("mp4", mime_type.subtype());
  }

  {
    MimeType mime_type("audio/mp4");
    EXPECT_TRUE(mime_type.is_valid());
  }

  {
    MimeType mime_type("abc/xyz");
    EXPECT_TRUE(mime_type.is_valid());
  }
}

TEST(MimeTypeTest, TypeNotAtBeginning) {
  MimeType mime_type("codecs=\"abc\"; audio/mp4");
  EXPECT_FALSE(mime_type.is_valid());
}

TEST(MimeTypeTest, ValidContentTypeWithParams) {
  {
    MimeType mime_type("video/mp4; name=123");
    EXPECT_TRUE(mime_type.is_valid());
  }

  {
    MimeType mime_type("audio/mp4;codecs=\"abc\"");
    EXPECT_TRUE(mime_type.is_valid());
  }

  {
    MimeType mime_type("  audio/mp4  ;  codecs   =  \"abc\"  ");
    EXPECT_TRUE(mime_type.is_valid());
  }
}

TEST(MimeTypeTest, GetCodecs) {
  {
    MimeType mime_type("audio/mp4;codecs=\"abc\"");
    EXPECT_EQ(1, mime_type.GetCodecs().size());
    EXPECT_EQ("abc", mime_type.GetCodecs()[0]);
  }

  {
    MimeType mime_type("audio/mp4;codecs=\"abc, def\"");
    EXPECT_EQ(2, mime_type.GetCodecs().size());
    EXPECT_EQ("abc", mime_type.GetCodecs()[0]);
    EXPECT_EQ("def", mime_type.GetCodecs()[1]);
  }

  {
    MimeType mime_type("audio/mp4;codecs=\"abc, def\";param1=\" value1 \"");
    EXPECT_EQ(2, mime_type.GetCodecs().size());
  }
}

TEST(MimeTypeTest, ParamCount) {
  {
    MimeType mime_type("video/mp4");
    EXPECT_EQ(0, mime_type.GetParamCount());
  }

  {
    MimeType mime_type("video/mp4; width=1920");
    EXPECT_EQ(1, mime_type.GetParamCount());
  }

  {
    MimeType mime_type("video/mp4; width=1920; height=1080");
    EXPECT_EQ(2, mime_type.GetParamCount());
  }
}

TEST(MimeTypeTest, GetParamType) {
  {
    MimeType mime_type(
        "video/mp4; name0=123; name1=1.2; name2=xyz; name3=true; name4=false");
    EXPECT_EQ(MimeType::kParamTypeInteger, mime_type.GetParamType(0));
    EXPECT_EQ(MimeType::kParamTypeFloat, mime_type.GetParamType(1));
    EXPECT_EQ(MimeType::kParamTypeString, mime_type.GetParamType(2));
    EXPECT_EQ(MimeType::kParamTypeBoolean, mime_type.GetParamType(3));
    EXPECT_EQ(MimeType::kParamTypeBoolean, mime_type.GetParamType(4));
  }

  {
    MimeType mime_type("video/mp4; name0=\"123\"; name1=\"abc\"");
    EXPECT_EQ(MimeType::kParamTypeString, mime_type.GetParamType(0));
    EXPECT_EQ(MimeType::kParamTypeString, mime_type.GetParamType(1));
  }

  {
    MimeType mime_type("video/mp4; name1=\" abc \"");
    EXPECT_EQ(MimeType::kParamTypeString, mime_type.GetParamType(0));
  }
}

TEST(MimeTypeTest, GetParamName) {
  {
    MimeType mime_type("video/mp4; name0=123; name1=1.2; name2=xyz");
    EXPECT_EQ("name0", mime_type.GetParamName(0));
    EXPECT_EQ("name1", mime_type.GetParamName(1));
    EXPECT_EQ("name2", mime_type.GetParamName(2));
  }
}

TEST(MimeTypeTest, GetParamIntValueWithIndex) {
  {
    MimeType mime_type("video/mp4; name=123");
    EXPECT_EQ(123, mime_type.GetParamIntValue(0));
  }

  {
    MimeType mime_type("video/mp4; width=1920; height=1080");
    EXPECT_EQ(1920, mime_type.GetParamIntValue(0));
    EXPECT_EQ(1080, mime_type.GetParamIntValue(1));
  }

  {
    MimeType mime_type("audio/mp4; channels=6");
    EXPECT_EQ(6, mime_type.GetParamIntValue(0));
  }
}

TEST(MimeTypeTest, GetParamFloatValueWithIndex) {
  MimeType mime_type("video/mp4; name0=123; name1=123.4");
  EXPECT_FLOAT_EQ(123.f, mime_type.GetParamFloatValue(0));
  EXPECT_FLOAT_EQ(123.4f, mime_type.GetParamFloatValue(1));
}

TEST(MimeTypeTest, GetParamStringValueWithIndex) {
  {
    MimeType mime_type("video/mp4; name0=123; name1=abc; name2=\"xyz\"");
    EXPECT_EQ("123", mime_type.GetParamStringValue(0));
    EXPECT_EQ("abc", mime_type.GetParamStringValue(1));
    EXPECT_EQ("xyz", mime_type.GetParamStringValue(2));
  }

  {
    MimeType mime_type("video/mp4; name=\" xyz  \"");
    EXPECT_EQ(" xyz  ", mime_type.GetParamStringValue(0));
  }
}

TEST(MimeTypeTest, GetParamBoolValueWithIndex) {
  MimeType mime_type("video/mp4; name0=true; name1=false");
  EXPECT_TRUE(mime_type.GetParamBoolValue(0));
  EXPECT_FALSE(mime_type.GetParamBoolValue(1));
}

TEST(MimeTypeTest, GetParamValueInInvalidType) {
  MimeType mime_type("video/mp4; name0=abc; name1=123.4");
  EXPECT_FLOAT_EQ(0, mime_type.GetParamIntValue(0));
  EXPECT_FLOAT_EQ(0.f, mime_type.GetParamFloatValue(0));
  EXPECT_FLOAT_EQ(0, mime_type.GetParamIntValue(1));
}

TEST(MimeTypeTest, GetParamIntValueWithName) {
  {
    MimeType mime_type("video/mp4; name=123");
    EXPECT_EQ(123, mime_type.GetParamIntValue("name", 0));
    EXPECT_EQ(6, mime_type.GetParamIntValue("channels", 6));
  }

  {
    MimeType mime_type("video/mp4; width=1920; height=1080");
    EXPECT_EQ(1920, mime_type.GetParamIntValue("width", 0));
    EXPECT_EQ(1080, mime_type.GetParamIntValue("height", 0));
  }

  {
    MimeType mime_type("audio/mp4; channels=6");
    EXPECT_EQ(6, mime_type.GetParamIntValue("channels", 0));
  }
}

TEST(MimeTypeTest, GetParamFloatValueWithName) {
  {
    MimeType mime_type("video/mp4; name0=123; name1=123.4");
    EXPECT_FLOAT_EQ(123.f, mime_type.GetParamFloatValue("name0", 0.f));
    EXPECT_FLOAT_EQ(123.4f, mime_type.GetParamFloatValue("name1", 0.f));
    EXPECT_FLOAT_EQ(59.96f, mime_type.GetParamFloatValue("framerate", 59.96f));
  }
}

TEST(MimeTypeTest, GetParamStringValueWithName) {
  {
    MimeType mime_type("video/mp4; name0=123; name1=abc; name2=\"xyz\"");
    EXPECT_EQ("123", mime_type.GetParamStringValue("name0", ""));
    EXPECT_EQ("abc", mime_type.GetParamStringValue("name1", ""));
    EXPECT_EQ("xyz", mime_type.GetParamStringValue("name2", ""));
    EXPECT_EQ("h263", mime_type.GetParamStringValue("codecs", "h263"));
  }

  {
    MimeType mime_type("video/mp4; name=\" xyz  \"");
    EXPECT_EQ(" xyz  ", mime_type.GetParamStringValue("name", ""));
  }
}

TEST(MimeTypeTest, GetParamBooleanValueWithName) {
  MimeType mime_type("video/mp4; name0=true; name1=false; name2=trues");
  EXPECT_TRUE(mime_type.GetParamBoolValue("name0", false));
  EXPECT_FALSE(mime_type.GetParamBoolValue("name1", true));
  // Default value
  EXPECT_FALSE(mime_type.GetParamBoolValue("name2", false));
}

TEST(MimeTypeTest, ReadParamOfDifferentTypes) {
  // Ensure that the parameter type is enforced correctly.
  MimeType mime_type("video/mp4; string=value; int=1; float=1.0; bool=true");
  ASSERT_TRUE(mime_type.is_valid());
  // Read params as their original types.
  EXPECT_EQ(mime_type.GetParamStringValue("string", ""), "value");
  EXPECT_EQ(mime_type.GetParamIntValue("int", 0), 1);
  EXPECT_EQ(mime_type.GetParamFloatValue("float", 0.0), 1.0);
  EXPECT_EQ(mime_type.GetParamBoolValue("bool", false), true);
  // All param types can be read as strings.
  EXPECT_EQ(mime_type.GetParamStringValue("int", ""), "1");
  EXPECT_EQ(mime_type.GetParamStringValue("float", ""), "1.0");
  EXPECT_EQ(mime_type.GetParamStringValue("bool", ""), "true");
  // Int can also be read as float.
  EXPECT_EQ(mime_type.GetParamFloatValue("int", 0.0), 1.0);

  // Test failing validation for ints,
  EXPECT_EQ(mime_type.GetParamIntValue("string", 0), 0);
  EXPECT_EQ(mime_type.GetParamIntValue("float", 0), 0);
  EXPECT_EQ(mime_type.GetParamIntValue("bool", 0), 0);

  // floats,
  EXPECT_EQ(mime_type.GetParamFloatValue("string", 0.0), 0.0);
  EXPECT_EQ(mime_type.GetParamFloatValue("bool", 0.0), 0.0);

  // and bools.
  EXPECT_FALSE(mime_type.GetParamBoolValue("string", false));
  EXPECT_FALSE(mime_type.GetParamBoolValue("int", false));
  EXPECT_FALSE(mime_type.GetParamBoolValue("float", false));
}

TEST(MimeTypeTest, RegisterAndValidateParamsWithPatterns) {
  MimeType mime_type("video/mp4; string=yes");
  EXPECT_TRUE(mime_type.RegisterStringParameter("string", "yes"));
  EXPECT_TRUE(mime_type.RegisterStringParameter("string", "yes|no"));
  EXPECT_TRUE(mime_type.RegisterStringParameter("string", "no|yes|no"));
  EXPECT_TRUE(mime_type.RegisterStringParameter("string", "no|no|yes"));
  EXPECT_TRUE(mime_type.RegisterStringParameter("string", "noyes|yes"));
  EXPECT_TRUE(mime_type.is_valid());
}

TEST(MimeTypeTest, RegisterAndValidateParamsWithShortPatterns) {
  MimeType mime_type("video/mp4; string=y");
  EXPECT_TRUE(mime_type.RegisterStringParameter("string", "y"));
  EXPECT_TRUE(mime_type.RegisterStringParameter("string", "y|n"));
  EXPECT_TRUE(mime_type.RegisterStringParameter("string", "n|y"));
  EXPECT_TRUE(mime_type.is_valid());
}

TEST(MimeTypeTest, RegisterAndValidateParamsWithPartialMatches) {
  {
    MimeType mime_type("video/mp4; string=yes");
    EXPECT_FALSE(mime_type.RegisterStringParameter("string", "yesno|no"));
    EXPECT_FALSE(mime_type.is_valid());
  }
  {
    MimeType mime_type("video/mp4; string=yes");
    EXPECT_FALSE(mime_type.RegisterStringParameter("string", "noyes|no"));
    EXPECT_FALSE(mime_type.is_valid());
  }
  {
    MimeType mime_type("video/mp4; string=yes");
    EXPECT_FALSE(mime_type.RegisterStringParameter("string", "no|yesno"));
    EXPECT_FALSE(mime_type.is_valid());
  }
  {
    MimeType mime_type("video/mp4; string=yes");
    EXPECT_FALSE(mime_type.RegisterStringParameter("string", "no|noyes"));
    EXPECT_FALSE(mime_type.is_valid());
  }
}

TEST(MimeTypeTest, MissingParamReturnsTrueOnRegistration) {
  MimeType mime_type("video/mp4");
  EXPECT_TRUE(mime_type.RegisterStringParameter("string"));
  EXPECT_TRUE(mime_type.is_valid());
  EXPECT_EQ(mime_type.GetParamStringValue("string", "default"), "default");
}

TEST(MimeTypeTest, RegisterAndValidateParamsWithEmptyishPattern) {
  {
    MimeType mime_type("video/mp4; string=yes");
    EXPECT_FALSE(mime_type.RegisterStringParameter("string", "|"));
  }
  {
    MimeType mime_type("video/mp4; string=yes");
    EXPECT_FALSE(mime_type.RegisterStringParameter("string", "||"));
  }
}

TEST(MimeTypeTest, CannotRegisterParamWithInvalidMimeType) {
  MimeType mime_type("video/mp4; string=");
  ASSERT_FALSE(mime_type.is_valid());
  EXPECT_FALSE(mime_type.RegisterStringParameter("string"));
}

}  // namespace
}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
