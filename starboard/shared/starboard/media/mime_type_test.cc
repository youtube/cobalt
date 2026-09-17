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

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(MimeTypeTest, EmptyString) {
  auto mime_type = MimeType::Create("");
  EXPECT_FALSE(mime_type);
}

// Valid mime type must have a type/subtype without space in between.
TEST(MimeTypeTest, InvalidMimeType) {
  {
    auto mime_type = MimeType::Create("video");
    EXPECT_FALSE(mime_type);
  }

  {
    auto mime_type = MimeType::Create("video/");
    EXPECT_FALSE(mime_type);
  }

  {
    auto mime_type = MimeType::Create("/mp4");
    EXPECT_FALSE(mime_type);
  }

  {
    auto mime_type = MimeType::Create("video /mp4");
    EXPECT_FALSE(mime_type);
  }

  {
    auto mime_type = MimeType::Create("video/ mp4");
    EXPECT_FALSE(mime_type);
  }

  {
    auto mime_type = MimeType::Create("video / mp4");
    EXPECT_FALSE(mime_type);
  }

  {  // "codecs" parameter isn't the first parameter.
    auto mime_type =
        MimeType::Create("audio/mp4;param1=\" value1 \";codecs=\"abc, def\"");
    EXPECT_FALSE(mime_type);
  }

  {  // More than one "codecs" parameters.
    auto mime_type = MimeType::Create(
        "audio/mp4;codecs=\"abc, def\";"
        "param1=\" value1 \";codecs=\"abc, def\"");
    EXPECT_FALSE(mime_type);
  }
  {
    // Parameter name must not be empty.
    auto mime_type = MimeType::Create("video/mp4; =value");
    EXPECT_FALSE(mime_type);
  }
  {
    // Parameter value must not be empty.
    auto mime_type = MimeType::Create("video/mp4; name=");
    EXPECT_FALSE(mime_type);
  }
  {
    // No '|' allowed.
    auto mime_type = MimeType::Create("video/mp4; name=ab|c");
    EXPECT_FALSE(mime_type);
  }
}

TEST(MimeTypeTest, ValidContentTypeWithTypeAndSubtypeOnly) {
  {
    auto mime_type = MimeType::Create("video/mp4");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ("video", mime_type->type());
    EXPECT_EQ("mp4", mime_type->subtype());
  }

  {
    auto mime_type = MimeType::Create("audio/mp4");
    EXPECT_TRUE(mime_type);
  }

  {
    auto mime_type = MimeType::Create("abc/xyz");
    EXPECT_TRUE(mime_type);
  }
}

TEST(MimeTypeTest, TypeNotAtBeginning) {
  auto mime_type = MimeType::Create("codecs=\"abc\"; audio/mp4");
  EXPECT_FALSE(mime_type);
}

TEST(MimeTypeTest, ValidContentTypeWithParams) {
  {
    auto mime_type = MimeType::Create("video/mp4; name=123");
    EXPECT_TRUE(mime_type);
  }

  {
    auto mime_type = MimeType::Create("audio/mp4;codecs=\"abc\"");
    EXPECT_TRUE(mime_type);
  }

  {
    auto mime_type = MimeType::Create("  audio/mp4  ;  codecs   =  \"abc\"  ");
    EXPECT_TRUE(mime_type);
  }
}

TEST(MimeTypeTest, GetCodecs) {
  {
    auto mime_type = MimeType::Create("audio/mp4;codecs=\"abc\"");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(1U, mime_type->GetCodecs().size());
    EXPECT_EQ("abc", mime_type->GetCodecs()[0]);
  }

  {
    auto mime_type = MimeType::Create("audio/mp4;codecs=\"abc, def\"");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(2U, mime_type->GetCodecs().size());
    EXPECT_EQ("abc", mime_type->GetCodecs()[0]);
    EXPECT_EQ("def", mime_type->GetCodecs()[1]);
  }

  {
    auto mime_type =
        MimeType::Create("audio/mp4;codecs=\"abc, def\";param1=\" value1 \"");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(2U, mime_type->GetCodecs().size());
  }
}

TEST(MimeTypeTest, ParamCount) {
  {
    auto mime_type = MimeType::Create("video/mp4");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(0, mime_type->GetParamCount());
  }

  {
    auto mime_type = MimeType::Create("video/mp4; width=1920");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(1, mime_type->GetParamCount());
  }

  {
    auto mime_type = MimeType::Create("video/mp4; width=1920; height=1080");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(2, mime_type->GetParamCount());
  }
}

TEST(MimeTypeTest, GetParamType) {
  {
    auto mime_type = MimeType::Create(
        "video/mp4; name0=123; name1=1.2; name2=xyz; name3=true; name4=false");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(MimeType::kParamTypeInteger, mime_type->GetParamType(0));
    EXPECT_EQ(MimeType::kParamTypeFloat, mime_type->GetParamType(1));
    EXPECT_EQ(MimeType::kParamTypeString, mime_type->GetParamType(2));
    EXPECT_EQ(MimeType::kParamTypeBoolean, mime_type->GetParamType(3));
    EXPECT_EQ(MimeType::kParamTypeBoolean, mime_type->GetParamType(4));
  }

  {
    auto mime_type =
        MimeType::Create("video/mp4; name0=\"123\"; name1=\"abc\"");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(MimeType::kParamTypeString, mime_type->GetParamType(0));
    EXPECT_EQ(MimeType::kParamTypeString, mime_type->GetParamType(1));
  }

  {
    auto mime_type = MimeType::Create("video/mp4; name1=\" abc \"");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(MimeType::kParamTypeString, mime_type->GetParamType(0));
  }
}

TEST(MimeTypeTest, GetParamIntValueWithIndex) {
  {
    auto mime_type = MimeType::Create("video/mp4; name=123");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(123, mime_type->GetParamIntValue(0));
  }

  {
    auto mime_type = MimeType::Create("video/mp4; width=1920; height=1080");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(1920, mime_type->GetParamIntValue(0));
    EXPECT_EQ(1080, mime_type->GetParamIntValue(1));
  }

  {
    auto mime_type = MimeType::Create("audio/mp4; channels=6");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(6, mime_type->GetParamIntValue(0));
  }
}

TEST(MimeTypeTest, GetParamFloatValueWithIndex) {
  auto mime_type = MimeType::Create("video/mp4; name0=123; name1=123.4");
  ASSERT_TRUE(mime_type);
  EXPECT_FLOAT_EQ(123.f, mime_type->GetParamFloatValue(0));
  EXPECT_FLOAT_EQ(123.4f, mime_type->GetParamFloatValue(1));
}

TEST(MimeTypeTest, GetParamStringValueWithIndex) {
  {
    auto mime_type =
        MimeType::Create("video/mp4; name0=123; name1=abc; name2=\"xyz\"");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ("123", mime_type->GetParamStringValue(0));
    EXPECT_EQ("abc", mime_type->GetParamStringValue(1));
    EXPECT_EQ("xyz", mime_type->GetParamStringValue(2));
  }

  {
    auto mime_type = MimeType::Create("video/mp4; name=\" xyz  \"");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(" xyz  ", mime_type->GetParamStringValue(0));
  }
}

TEST(MimeTypeTest, GetParamBoolValueWithIndex) {
  auto mime_type = MimeType::Create("video/mp4; name0=true; name1=false");
  ASSERT_TRUE(mime_type);
  EXPECT_TRUE(mime_type->GetParamBoolValue(0));
  EXPECT_FALSE(mime_type->GetParamBoolValue(1));
}

TEST(MimeTypeTest, GetParamValueInInvalidType) {
  auto mime_type = MimeType::Create("video/mp4; name0=abc; name1=123.4");
  ASSERT_TRUE(mime_type);
  EXPECT_FLOAT_EQ(0, mime_type->GetParamIntValue(0));
  EXPECT_FLOAT_EQ(0.f, mime_type->GetParamFloatValue(0));
  EXPECT_FLOAT_EQ(0, mime_type->GetParamIntValue(1));
}

TEST(MimeTypeTest, GetParamIntValueWithName) {
  {
    auto mime_type = MimeType::Create("video/mp4; name=123");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(123, mime_type->GetParamIntValue(MimeParam("name"), 0));
    EXPECT_EQ(6, mime_type->GetParamIntValue(kMimeParamChannels, 6));
  }

  {
    auto mime_type = MimeType::Create("video/mp4; width=1920; height=1080");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(1920, mime_type->GetParamIntValue(kMimeParamWidth, 0));
    EXPECT_EQ(1080, mime_type->GetParamIntValue(kMimeParamHeight, 0));
  }

  {
    auto mime_type = MimeType::Create("audio/mp4; channels=6");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(6, mime_type->GetParamIntValue(kMimeParamChannels, 0));
  }
}

TEST(MimeTypeTest, GetParamFloatValueWithName) {
  {
    auto mime_type = MimeType::Create("video/mp4; name0=123; name1=123.4");
    ASSERT_TRUE(mime_type);
    EXPECT_FLOAT_EQ(123.f,
                    mime_type->GetParamFloatValue(MimeParam("name0"), 0.f));
    EXPECT_FLOAT_EQ(123.4f,
                    mime_type->GetParamFloatValue(MimeParam("name1"), 0.f));
    EXPECT_FLOAT_EQ(59.96f,
                    mime_type->GetParamFloatValue(kMimeParamFramerate, 59.96f));
  }
}

TEST(MimeTypeTest, GetParamStringValueWithName) {
  {
    auto mime_type =
        MimeType::Create("video/mp4; name0=123; name1=abc; name2=\"xyz\"");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ("123", mime_type->GetParamStringValue(MimeParam("name0"), ""));
    EXPECT_EQ("abc", mime_type->GetParamStringValue(MimeParam("name1"), ""));
    EXPECT_EQ("xyz", mime_type->GetParamStringValue(MimeParam("name2"), ""));
    EXPECT_EQ("h263", mime_type->GetParamStringValue(kMimeParamCodecs, "h263"));
  }

  {
    auto mime_type = MimeType::Create("video/mp4; name=\" xyz  \"");
    ASSERT_TRUE(mime_type);
    EXPECT_EQ(" xyz  ", mime_type->GetParamStringValue(MimeParam("name"), ""));
  }
}

TEST(MimeTypeTest, GetParamBooleanValueWithName) {
  auto mime_type =
      MimeType::Create("video/mp4; name0=true; name1=false; name2=trues");
  ASSERT_TRUE(mime_type);
  EXPECT_TRUE(mime_type->GetParamBoolValue(MimeParam("name0"), false));
  EXPECT_FALSE(mime_type->GetParamBoolValue(MimeParam("name1"), true));
  // Default value
  EXPECT_FALSE(mime_type->GetParamBoolValue(MimeParam("name2"), false));
}

TEST(MimeTypeTest, ReadParamOfDifferentTypes) {
  // Ensure that the parameter type is enforced correctly.
  auto mime_type =
      MimeType::Create("video/mp4; string=value; int=1; float=1.0; bool=true");
  ASSERT_TRUE(mime_type);
  // Read params as their original types.
  EXPECT_EQ(mime_type->GetParamStringValue(MimeParam("string"), ""), "value");
  EXPECT_EQ(mime_type->GetParamIntValue(MimeParam("int"), 0), 1);
  EXPECT_EQ(mime_type->GetParamFloatValue(MimeParam("float"), 0.0), 1.0);
  EXPECT_EQ(mime_type->GetParamBoolValue(MimeParam("bool"), false), true);
  // All param types can be read as strings.
  EXPECT_EQ(mime_type->GetParamStringValue(MimeParam("int"), ""), "1");
  EXPECT_EQ(mime_type->GetParamStringValue(MimeParam("float"), ""), "1.0");
  EXPECT_EQ(mime_type->GetParamStringValue(MimeParam("bool"), ""), "true");
  // Int can also be read as float.
  EXPECT_EQ(mime_type->GetParamFloatValue(MimeParam("int"), 0.0), 1.0);

  // Test failing validation for ints,
  EXPECT_EQ(mime_type->GetParamIntValue(MimeParam("string"), 0), 0);
  EXPECT_EQ(mime_type->GetParamIntValue(MimeParam("float"), 0), 0);
  EXPECT_EQ(mime_type->GetParamIntValue(MimeParam("bool"), 0), 0);

  // floats,
  EXPECT_EQ(mime_type->GetParamFloatValue(MimeParam("string"), 0.0), 0.0);
  EXPECT_EQ(mime_type->GetParamFloatValue(MimeParam("bool"), 0.0), 0.0);

  // and bools.
  EXPECT_FALSE(mime_type->GetParamBoolValue(MimeParam("string"), false));
  EXPECT_FALSE(mime_type->GetParamBoolValue(MimeParam("int"), false));
  EXPECT_FALSE(mime_type->GetParamBoolValue(MimeParam("float"), false));
}

TEST(MimeTypeTest, ParseInvalidParamString) {
  EXPECT_FALSE(MimeType::ParseParamString("", nullptr));
  EXPECT_FALSE(MimeType::ParseParamString("invalid", nullptr));
  EXPECT_FALSE(MimeType::ParseParamString("val=0;", nullptr));
  EXPECT_FALSE(MimeType::ParseParamString("val=a|b", nullptr));
  EXPECT_FALSE(MimeType::ParseParamString("val=", nullptr));
}

TEST(MimeTypeTest, ParseValidParamString) {
  MimeType::Param result;

  EXPECT_TRUE(MimeType::ParseParamString("val=0", &result));
  EXPECT_EQ(result.name, "val");
  EXPECT_EQ(result.type, MimeType::kParamTypeInteger);
  EXPECT_EQ(result.int_value, 0);

  EXPECT_TRUE(MimeType::ParseParamString("val=1", &result));
  EXPECT_EQ(result.name, "val");
  EXPECT_EQ(result.type, MimeType::kParamTypeInteger);
  EXPECT_EQ(result.int_value, 1);

  EXPECT_TRUE(MimeType::ParseParamString("val=-1", &result));
  EXPECT_EQ(result.name, "val");
  EXPECT_EQ(result.type, MimeType::kParamTypeInteger);
  EXPECT_EQ(result.int_value, -1);

  EXPECT_TRUE(MimeType::ParseParamString("val=0.0", &result));
  EXPECT_EQ(result.name, "val");
  EXPECT_EQ(result.type, MimeType::kParamTypeFloat);
  EXPECT_EQ(result.float_value, 0.0f);

  EXPECT_TRUE(MimeType::ParseParamString("val=1.0", &result));
  EXPECT_EQ(result.name, "val");
  EXPECT_EQ(result.type, MimeType::kParamTypeFloat);
  EXPECT_EQ(result.float_value, 1.0f);

  EXPECT_TRUE(MimeType::ParseParamString("val=-1.0", &result));
  EXPECT_EQ(result.name, "val");
  EXPECT_EQ(result.type, MimeType::kParamTypeFloat);
  EXPECT_EQ(result.float_value, -1.0f);

  EXPECT_TRUE(MimeType::ParseParamString("val=true", &result));
  EXPECT_EQ(result.name, "val");
  EXPECT_EQ(result.type, MimeType::kParamTypeBoolean);
  EXPECT_EQ(result.bool_value, true);

  EXPECT_TRUE(MimeType::ParseParamString("val=false", &result));
  EXPECT_EQ(result.name, "val");
  EXPECT_EQ(result.type, MimeType::kParamTypeBoolean);
  EXPECT_EQ(result.bool_value, false);

  EXPECT_TRUE(MimeType::ParseParamString("val=\"\"", &result));
  EXPECT_EQ(result.name, "val");
  EXPECT_EQ(result.type, MimeType::kParamTypeString);
  EXPECT_EQ(result.string_value, "");

  EXPECT_TRUE(MimeType::ParseParamString("val=\"abc\"", &result));
  EXPECT_EQ(result.name, "val");
  EXPECT_EQ(result.type, MimeType::kParamTypeString);
  EXPECT_EQ(result.string_value, "abc");

  EXPECT_TRUE(MimeType::ParseParamString("val=abc", &result));
  EXPECT_EQ(result.name, "val");
  EXPECT_EQ(result.type, MimeType::kParamTypeString);
  EXPECT_EQ(result.string_value, "abc");
}

TEST(MimeTypeTest, ValidateParamsWithPatterns) {
  auto mime_type = MimeType::Create("video/mp4; string=yes");
  ASSERT_TRUE(mime_type);
  EXPECT_TRUE(mime_type->ValidateStringParameter(MimeParam("string"), "yes"));
  EXPECT_TRUE(
      mime_type->ValidateStringParameter(MimeParam("string"), "yes|no"));
  EXPECT_TRUE(
      mime_type->ValidateStringParameter(MimeParam("string"), "no|yes|no"));
  EXPECT_TRUE(
      mime_type->ValidateStringParameter(MimeParam("string"), "no|no|yes"));
  EXPECT_TRUE(
      mime_type->ValidateStringParameter(MimeParam("string"), "noyes|yes"));
  EXPECT_FALSE(mime_type->ValidateStringParameter(MimeParam("string"), "no"));
}

TEST(MimeTypeTest, ValidateParamsWithShortPatterns) {
  auto mime_type = MimeType::Create("video/mp4; string=y");
  ASSERT_TRUE(mime_type);
  EXPECT_TRUE(mime_type->ValidateStringParameter(MimeParam("string"), "y"));
  EXPECT_TRUE(mime_type->ValidateStringParameter(MimeParam("string"), "y|n"));
  EXPECT_TRUE(mime_type->ValidateStringParameter(MimeParam("string"), "n|y"));
  EXPECT_FALSE(mime_type->ValidateStringParameter(MimeParam("string"), "n"));
}

TEST(MimeTypeTest, ValidateParamsWithPartialMatches) {
  auto mime_type = MimeType::Create("video/mp4; string=yes");
  ASSERT_TRUE(mime_type);
  EXPECT_FALSE(
      mime_type->ValidateStringParameter(MimeParam("string"), "yesno|no"));
  EXPECT_FALSE(
      mime_type->ValidateStringParameter(MimeParam("string"), "noyes|no"));
  EXPECT_FALSE(
      mime_type->ValidateStringParameter(MimeParam("string"), "no|yesno"));
  EXPECT_FALSE(
      mime_type->ValidateStringParameter(MimeParam("string"), "no|noyes"));
}

TEST(MimeTypeTest, ValidateMissingParam) {
  auto mime_type = MimeType::Create("video/mp4");
  ASSERT_TRUE(mime_type);
  EXPECT_TRUE(mime_type->ValidateStringParameter(MimeParam("string")));
  EXPECT_EQ(mime_type->GetParamStringValue(MimeParam("string"), "default"),
            "default");
}

TEST(MimeTypeTest, ValidateParamsWithEmptyishPattern) {
  auto mime_type = MimeType::Create("video/mp4; string=yes");
  ASSERT_TRUE(mime_type);
  EXPECT_TRUE(mime_type->ValidateStringParameter(MimeParam("string"), ""));
  EXPECT_FALSE(mime_type->ValidateStringParameter(MimeParam("string"), "|"));
  EXPECT_FALSE(mime_type->ValidateStringParameter(MimeParam("string"), "||"));
}

TEST(MimeTypeTest, ValidateParamWithInvalidMimeType) {
  auto mime_type = MimeType::Create("video/mp4; string=");
  ASSERT_FALSE(mime_type);
}

TEST(MimeTypeTest, GetAndValidateParamWithKey) {
  auto mime_type = MimeType::Create(
      "video/mp4; codecs=\"avc1.4d4015\"; width=1920; height=1080; "
      "framerate=60; bitrate=5000000; decode-to-texture=true; "
      "tunnelmode=false");
  ASSERT_TRUE(mime_type);

  EXPECT_TRUE(mime_type->ValidateIntParameter(kMimeParamWidth));
  EXPECT_TRUE(mime_type->ValidateIntParameter(kMimeParamHeight));
  EXPECT_TRUE(mime_type->ValidateFloatParameter(kMimeParamFramerate));
  EXPECT_TRUE(mime_type->ValidateIntParameter(kMimeParamBitrate));
  EXPECT_TRUE(mime_type->ValidateBoolParameter(kMimeParamDecodeToTexture));
  EXPECT_TRUE(mime_type->ValidateBoolParameter(MimeParam("tunnelmode")));

  EXPECT_EQ("avc1.4d4015",
            mime_type->GetParamStringValue(kMimeParamCodecs, ""));
  EXPECT_EQ(1920, mime_type->GetParamIntValue(kMimeParamWidth, 0));
  EXPECT_EQ(1080, mime_type->GetParamIntValue(kMimeParamHeight, 0));
  EXPECT_FLOAT_EQ(60.0f,
                  mime_type->GetParamFloatValue(kMimeParamFramerate, 0.0f));
  EXPECT_EQ(5000000, mime_type->GetParamIntValue(kMimeParamBitrate, 0));
  EXPECT_TRUE(mime_type->GetParamBoolValue(kMimeParamDecodeToTexture, false));
  EXPECT_FALSE(mime_type->GetParamBoolValue(MimeParam("tunnelmode"), true));

  // Defaults for unprovided parameters
  EXPECT_EQ(2, mime_type->GetParamIntValue(kMimeParamChannels, 2));
  EXPECT_EQ("default_eotf",
            mime_type->GetParamStringValue(kMimeParamEotf, "default_eotf"));
}

}  // namespace

}  // namespace starboard
