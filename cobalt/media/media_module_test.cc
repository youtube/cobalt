#include "cobalt/media/media_module.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace media {
namespace {

using ::testing::NotNull;

#if SB_API_VERSION >= 14
TEST(MediaSourceTest, DoesNotSupportMp3Adaptive) {
  std::unique_ptr<CanPlayTypeHandler> handler =
      MediaModule::CreateCanPlayTypeHandler();
  ASSERT_THAT(handler, NotNull());
  EXPECT_FALSE(handler->CanPlayAdaptive("audio/mpeg; codecs=\"mp3\"",
                                        /*key_system=*/""));
  EXPECT_FALSE(handler->CanPlayAdaptive("audio/mpeg", /*key_system=*/""));
}

TEST(MediaSourceTest, DoesNotSupportMp3Progressive) {
  std::unique_ptr<CanPlayTypeHandler> handler =
      MediaModule::CreateCanPlayTypeHandler();
  ASSERT_THAT(handler, NotNull());
  EXPECT_FALSE(handler->CanPlayProgressive("audio/mpeg; codecs=\"mp3\""));
  EXPECT_FALSE(handler->CanPlayProgressive("audio/mpeg"));
}

TEST(MediaSourceTest, DoesNotSupportFlacAdaptive) {
  std::unique_ptr<CanPlayTypeHandler> handler =
      MediaModule::CreateCanPlayTypeHandler();
  ASSERT_THAT(handler, NotNull());
  EXPECT_FALSE(handler->CanPlayAdaptive("audio/ogg; codecs=\"flac\"",
                                        /*key_system=*/""));
  EXPECT_FALSE(handler->CanPlayAdaptive("audio/flac; codecs=\"flac\"",
                                        /*key_system=*/""));
}

TEST(MediaSourceTest, DoesNotSupportFlacProgressive) {
  std::unique_ptr<CanPlayTypeHandler> handler =
      MediaModule::CreateCanPlayTypeHandler();
  ASSERT_THAT(handler, NotNull());
  EXPECT_FALSE(handler->CanPlayProgressive("audio/ogg; codecs=\"flac\""));
  EXPECT_FALSE(handler->CanPlayProgressive("audio/flac; codecs=\"flac\""));
}

TEST(MediaSourceTest, DoesNotSupportPcmAdaptive) {
  std::unique_ptr<CanPlayTypeHandler> handler =
      MediaModule::CreateCanPlayTypeHandler();
  ASSERT_THAT(handler, NotNull());
  EXPECT_FALSE(handler->CanPlayAdaptive("audio/wav; codecs=\"1\"",
                                        /*key_system=*/""));
}

TEST(MediaSourceTest, DoesNotSupportPcmProgressive) {
  std::unique_ptr<CanPlayTypeHandler> handler =
      MediaModule::CreateCanPlayTypeHandler();
  ASSERT_THAT(handler, NotNull());
  EXPECT_FALSE(handler->CanPlayProgressive("audio/wav; codecs=\"1\""));
}

#endif  // SB_API_VERSION >= 14

}  // namespace
}  // namespace media
}  // namespace cobalt
