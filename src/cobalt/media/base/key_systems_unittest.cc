// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(sandersd): Refactor to remove recomputed codec arrays, and generally
// shorten and improve coverage.
//   - http://crbug.com/417444
//   - http://crbug.com/457438
// TODO(sandersd): Add tests to cover codec vectors with empty items.
// http://crbug.com/417461

#include <string>
#include <vector>

#include "base/logging.h"
#include "cobalt/media/base/eme_constants.h"
#include "cobalt/media/base/key_systems.h"
#include "cobalt/media/base/media.h"
#include "cobalt/media/base/media_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// These are the (fake) key systems that are registered for these tests.
// kUsesAes uses the AesDecryptor like Clear Key.
// kExternal uses an external CDM, such as Pepper-based or Android platform CDM.
const char kUsesAes[] = "x-org.example.clear";
const char kUseAesNameForUMA[] = "UseAes";
const char kExternal[] = "x-com.example.test";
const char kExternalNameForUMA[] = "External";

const char kClearKey[] = "org.w3.clearkey";
const char kExternalClearKey[] = "org.chromium.externalclearkey";

const char kAudioWebM[] = "audio/webm";
const char kVideoWebM[] = "video/webm";
const char kAudioFoo[] = "audio/foo";
const char kVideoFoo[] = "video/foo";

// Pick some arbitrary bit fields as long as they are not in conflict with the
// real ones.
enum TestCodec {
  TEST_CODEC_FOO_AUDIO = 1 << 10,  // An audio codec for foo container.
  TEST_CODEC_FOO_AUDIO_ALL = TEST_CODEC_FOO_AUDIO,
  TEST_CODEC_FOO_VIDEO = 1 << 11,  // A video codec for foo container.
  TEST_CODEC_FOO_VIDEO_ALL = TEST_CODEC_FOO_VIDEO,
  TEST_CODEC_FOO_ALL = TEST_CODEC_FOO_AUDIO_ALL | TEST_CODEC_FOO_VIDEO_ALL
};

static_assert((TEST_CODEC_FOO_ALL & EME_CODEC_ALL) == EME_CODEC_NONE,
              "test codec masks should only use invalid codec masks");

class TestKeySystemProperties : public KeySystemProperties {
 public:
  bool IsSupportedInitDataType(EmeInitDataType init_data_type) const override {
    return init_data_type == EmeInitDataType::WEBM;
  }
  SupportedCodecs GetSupportedCodecs() const override {
    return EME_CODEC_WEBM_ALL | TEST_CODEC_FOO_ALL;
  }
  EmeConfigRule GetRobustnessConfigRule(
      EmeMediaType media_type,
      const std::string& requested_robustness) const override {
    return requested_robustness.empty() ? EmeConfigRule::SUPPORTED
                                        : EmeConfigRule::NOT_SUPPORTED;
  }
  EmeSessionTypeSupport GetPersistentReleaseMessageSessionSupport()
      const override {
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }
};

class AesKeySystemProperties : public TestKeySystemProperties {
 public:
  explicit AesKeySystemProperties(const std::string& name) : name_(name) {}

  std::string GetKeySystemName() const override { return name_; }
  EmeSessionTypeSupport GetPersistentLicenseSessionSupport() const override {
    return EmeSessionTypeSupport::NOT_SUPPORTED;
  }
  EmeFeatureSupport GetPersistentStateSupport() const override {
    return EmeFeatureSupport::NOT_SUPPORTED;
  }
  EmeFeatureSupport GetDistinctiveIdentifierSupport() const override {
    return EmeFeatureSupport::NOT_SUPPORTED;
  }
  bool UseAesDecryptor() const override { return true; }

 private:
  std::string name_;
};

class ExternalKeySystemProperties : public TestKeySystemProperties {
 public:
  std::string GetKeySystemName() const override { return kExternal; }
  EmeSessionTypeSupport GetPersistentLicenseSessionSupport() const override {
    return EmeSessionTypeSupport::SUPPORTED;
  }
  EmeFeatureSupport GetPersistentStateSupport() const override {
    return EmeFeatureSupport::ALWAYS_ENABLED;
  }
  EmeFeatureSupport GetDistinctiveIdentifierSupport() const override {
    return EmeFeatureSupport::ALWAYS_ENABLED;
  }
  std::string GetPepperType() const override {
    return "application/x-ppapi-external-cdm";
  }
};

// Adapt IsSupportedKeySystemWithMediaMimeType() to the new API,
// IsSupportedCodecCombination().
static bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type, const std::vector<std::string>& codecs,
    const std::string& key_system) {
  return (KeySystems::GetInstance()->GetContentTypeConfigRule(
              key_system, EmeMediaType::VIDEO, mime_type, codecs) !=
          EmeConfigRule::NOT_SUPPORTED);
}

static bool IsSupportedKeySystemWithAudioMimeType(
    const std::string& mime_type, const std::vector<std::string>& codecs,
    const std::string& key_system) {
  return (KeySystems::GetInstance()->GetContentTypeConfigRule(
              key_system, EmeMediaType::AUDIO, mime_type, codecs) !=
          EmeConfigRule::NOT_SUPPORTED);
}

static bool IsSupportedKeySystem(const std::string& key_system) {
  return KeySystems::GetInstance()->IsSupportedKeySystem(key_system);
}

// Adds test container and codec masks.
// This function must be called after SetMediaClient() if a MediaClient will be
// provided.
// More details: AddXxxMask() will create KeySystems if it hasn't been created.
// During KeySystems's construction GetMediaClient() will be used to add key
// systems. In test code, the MediaClient is set by SetMediaClient().
// Therefore, SetMediaClient() must be called before this function to make sure
// MediaClient in effect when constructing KeySystems.
static void AddContainerAndCodecMasksForTest() {
  // Since KeySystems is a singleton. Make sure we only add test container and
  // codec masks once per process.
  static bool is_test_masks_added = false;

  if (is_test_masks_added) return;

  AddCodecMask(EmeMediaType::AUDIO, "fooaudio", TEST_CODEC_FOO_AUDIO);
  AddCodecMask(EmeMediaType::VIDEO, "foovideo", TEST_CODEC_FOO_VIDEO);
  AddMimeTypeCodecMask("audio/foo", TEST_CODEC_FOO_AUDIO_ALL);
  AddMimeTypeCodecMask("video/foo", TEST_CODEC_FOO_VIDEO_ALL);

  is_test_masks_added = true;
}

static bool CanRunExternalKeySystemTests() {
#if defined(OS_ANDROID)
  if (HasPlatformDecoderSupport()) return true;

  EXPECT_FALSE(IsSupportedKeySystem(kExternal));
  return false;
#else
  return true;
#endif
}

class TestMediaClient : public MediaClient {
 public:
  TestMediaClient();
  ~TestMediaClient() OVERRIDE;

  // MediaClient implementation.
  void AddKeySystemsInfoForUMA(
      std::vector<KeySystemInfoForUMA>* key_systems_info_for_uma) final;
  bool IsKeySystemsUpdateNeeded() final;
  void AddSupportedKeySystems(std::vector<std::unique_ptr<KeySystemProperties>>*
                                  key_systems_properties) OVERRIDE;
  void RecordRapporURL(const std::string& metric, const GURL& url) final;
  bool IsSupportedVideoConfig(media::VideoCodec codec,
                              media::VideoCodecProfile profile,
                              int level) final;

  // Helper function to test the case where IsKeySystemsUpdateNeeded() is true
  // after AddSupportedKeySystems() is called.
  void SetKeySystemsUpdateNeeded();

  // Helper function to disable "kExternal" key system support so that we can
  // test the key system update case.
  void DisableExternalKeySystemSupport();

 private:
  bool is_update_needed_;
  bool supports_external_key_system_;
};

TestMediaClient::TestMediaClient()
    : is_update_needed_(true), supports_external_key_system_(true) {}

TestMediaClient::~TestMediaClient() {}

void TestMediaClient::AddKeySystemsInfoForUMA(
    std::vector<KeySystemInfoForUMA>* key_systems_info_for_uma) {
  key_systems_info_for_uma->push_back(
      media::KeySystemInfoForUMA(kUsesAes, kUseAesNameForUMA));
  key_systems_info_for_uma->push_back(
      media::KeySystemInfoForUMA(kExternal, kExternalNameForUMA));
}

bool TestMediaClient::IsKeySystemsUpdateNeeded() { return is_update_needed_; }

void TestMediaClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<KeySystemProperties>>* key_systems) {
  DCHECK(is_update_needed_);

  key_systems->emplace_back(new AesKeySystemProperties(kUsesAes));

  if (supports_external_key_system_)
    key_systems->emplace_back(new ExternalKeySystemProperties());

  is_update_needed_ = false;
}

void TestMediaClient::RecordRapporURL(const std::string& /* metric */,
                                      const GURL& /* url */) {
  NOTIMPLEMENTED();
}

bool TestMediaClient::IsSupportedVideoConfig(media::VideoCodec codec,
                                             media::VideoCodecProfile profile,
                                             int level) {
  return true;
}

void TestMediaClient::SetKeySystemsUpdateNeeded() { is_update_needed_ = true; }

void TestMediaClient::DisableExternalKeySystemSupport() {
  supports_external_key_system_ = false;
}

class PotentiallySupportedNamesTestMediaClient : public TestMediaClient {
  void AddSupportedKeySystems(std::vector<std::unique_ptr<KeySystemProperties>>*
                                  key_systems_properties) final;
};

void PotentiallySupportedNamesTestMediaClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<KeySystemProperties>>* key_systems) {
  // org.w3.clearkey is automatically registered.
  key_systems->emplace_back(new AesKeySystemProperties("com.widevine.alpha"));
  key_systems->emplace_back(
      new AesKeySystemProperties("org.chromium.externalclearkey"));
  key_systems->emplace_back(
      new AesKeySystemProperties("org.chromium.externalclearkey.something"));
  key_systems->emplace_back(
      new AesKeySystemProperties("com.chromecast.something"));
  key_systems->emplace_back(new AesKeySystemProperties("x-something"));
}

class KeySystemsPotentiallySupportedNamesTest : public testing::Test {
 protected:
  KeySystemsPotentiallySupportedNamesTest() {
    SetMediaClient(&test_media_client_);
  }

  ~KeySystemsPotentiallySupportedNamesTest() override {
    // Clear the use of |test_media_client_|, which was set in SetUp().
    SetMediaClient(NULL);
  }

 private:
  PotentiallySupportedNamesTestMediaClient test_media_client_;
};

class KeySystemsTest : public testing::Test {
 protected:
  KeySystemsTest() {
    vp8_codec_.push_back("vp8");

    vp80_codec_.push_back("vp8.0");

    vp9_codec_.push_back("vp9");

    vp90_codec_.push_back("vp9.0");

    vorbis_codec_.push_back("vorbis");

    vp8_and_vorbis_codecs_.push_back("vp8");
    vp8_and_vorbis_codecs_.push_back("vorbis");

    vp9_and_vorbis_codecs_.push_back("vp9");
    vp9_and_vorbis_codecs_.push_back("vorbis");

    foovideo_codec_.push_back("foovideo");

    foovideo_extended_codec_.push_back("foovideo.4D400C");

    foovideo_dot_codec_.push_back("foovideo.");

    fooaudio_codec_.push_back("fooaudio");

    foovideo_and_fooaudio_codecs_.push_back("foovideo");
    foovideo_and_fooaudio_codecs_.push_back("fooaudio");

    unknown_codec_.push_back("unknown");

    mixed_codecs_.push_back("vorbis");
    mixed_codecs_.push_back("foovideo");

    SetMediaClient(&test_media_client_);
  }

  void SetUp() override { AddContainerAndCodecMasksForTest(); }

  ~KeySystemsTest() override {
    // Clear the use of |test_media_client_|, which was set in SetUp().
    SetMediaClient(NULL);
  }

  void UpdateClientKeySystems() {
    test_media_client_.SetKeySystemsUpdateNeeded();
    test_media_client_.DisableExternalKeySystemSupport();
  }

  typedef std::vector<std::string> CodecVector;

  const CodecVector& no_codecs() const { return no_codecs_; }

  const CodecVector& vp8_codec() const { return vp8_codec_; }
  const CodecVector& vp80_codec() const { return vp80_codec_; }
  const CodecVector& vp9_codec() const { return vp9_codec_; }
  const CodecVector& vp90_codec() const { return vp90_codec_; }

  const CodecVector& vorbis_codec() const { return vorbis_codec_; }

  const CodecVector& vp8_and_vorbis_codecs() const {
    return vp8_and_vorbis_codecs_;
  }
  const CodecVector& vp9_and_vorbis_codecs() const {
    return vp9_and_vorbis_codecs_;
  }

  const CodecVector& foovideo_codec() const { return foovideo_codec_; }
  const CodecVector& foovideo_extended_codec() const {
    return foovideo_extended_codec_;
  }
  const CodecVector& foovideo_dot_codec() const { return foovideo_dot_codec_; }
  const CodecVector& fooaudio_codec() const { return fooaudio_codec_; }
  const CodecVector& foovideo_and_fooaudio_codecs() const {
    return foovideo_and_fooaudio_codecs_;
  }

  const CodecVector& unknown_codec() const { return unknown_codec_; }

  const CodecVector& mixed_codecs() const { return mixed_codecs_; }

 private:
  const CodecVector no_codecs_;
  CodecVector vp8_codec_;
  CodecVector vp80_codec_;
  CodecVector vp9_codec_;
  CodecVector vp90_codec_;
  CodecVector vorbis_codec_;
  CodecVector vp8_and_vorbis_codecs_;
  CodecVector vp9_and_vorbis_codecs_;

  CodecVector foovideo_codec_;
  CodecVector foovideo_extended_codec_;
  CodecVector foovideo_dot_codec_;
  CodecVector fooaudio_codec_;
  CodecVector foovideo_and_fooaudio_codecs_;

  CodecVector unknown_codec_;

  CodecVector mixed_codecs_;

  TestMediaClient test_media_client_;
};

// TODO(ddorwin): Consider moving GetPepperType() calls out to their own test.

TEST_F(KeySystemsTest, EmptyKeySystem) {
  EXPECT_FALSE(IsSupportedKeySystem(std::string()));
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(std::string()));
}

// Clear Key is the only key system registered in content.
TEST_F(KeySystemsTest, ClearKey) {
  EXPECT_TRUE(IsSupportedKeySystem(kClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(),
                                                    kClearKey));

  EXPECT_EQ("ClearKey", GetKeySystemNameForUMA(kClearKey));
}

TEST_F(KeySystemsTest, ClearKeyWithInitDataType) {
  EXPECT_TRUE(IsSupportedKeySystem(kClearKey));
  EXPECT_TRUE(
      IsSupportedKeySystemWithInitDataType(kClearKey, EmeInitDataType::WEBM));
  EXPECT_TRUE(
      IsSupportedKeySystemWithInitDataType(kClearKey, EmeInitDataType::KEYIDS));

  // All other InitDataTypes are not supported.
  EXPECT_FALSE(IsSupportedKeySystemWithInitDataType(kClearKey,
                                                    EmeInitDataType::UNKNOWN));
}

// The key system is not registered and therefore is unrecognized.
TEST_F(KeySystemsTest, Basic_UnrecognizedKeySystem) {
  static const char* const kUnrecognized = "x-org.example.unrecognized";

  EXPECT_FALSE(IsSupportedKeySystem(kUnrecognized));

  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(kUnrecognized));
  EXPECT_FALSE(CanUseAesDecryptor(kUnrecognized));

#if defined(ENABLE_PEPPER_CDMS)
  std::string type;
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  EXPECT_DEATH(type = GetPepperType(kUnrecognized),
               "x-org.example.unrecognized is not a known system");
#endif
  EXPECT_TRUE(type.empty());
#endif
}

TEST_F(KeySystemsTest, Basic_UsesAesDecryptor) {
  EXPECT_TRUE(IsSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(), kUsesAes));

  // No UMA value for this test key system.
  EXPECT_EQ("UseAes", GetKeySystemNameForUMA(kUsesAes));

  EXPECT_TRUE(CanUseAesDecryptor(kUsesAes));
#if defined(ENABLE_PEPPER_CDMS)
  std::string type;
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  EXPECT_DEATH(type = GetPepperType(kUsesAes),
               "x-org.example.clear is not Pepper-based");
#endif
  EXPECT_TRUE(type.empty());
#endif
}

TEST_F(KeySystemsTest,
       IsSupportedKeySystemWithMediaMimeType_UsesAesDecryptor_TypesContainer1) {
  // Valid video types.
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp8_codec(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp80_codec(),
                                                    kUsesAes));
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp9_codec(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp90_codec(),
                                                    kUsesAes));

  // Audio in a video container.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_and_vorbis_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_and_vorbis_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vorbis_codec(),
                                                     kUsesAes));

  // Non-Webm codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, foovideo_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, unknown_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, mixed_codecs(),
                                                     kUsesAes));

  // Valid audio types.
  EXPECT_TRUE(
      IsSupportedKeySystemWithAudioMimeType(kAudioWebM, no_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(kAudioWebM, vorbis_codec(),
                                                    kUsesAes));

  // Non-audio codecs.
  EXPECT_FALSE(
      IsSupportedKeySystemWithMediaMimeType(kAudioWebM, vp8_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp8_and_vorbis_codecs(), kUsesAes));
  EXPECT_FALSE(
      IsSupportedKeySystemWithMediaMimeType(kAudioWebM, vp9_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp9_and_vorbis_codecs(), kUsesAes));

  // Non-Webm codec.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, fooaudio_codec(), kUsesAes));
}

TEST_F(KeySystemsTest, IsSupportedKeySystem_InvalidVariants) {
  // Case sensitive.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.ClEaR"));

  // TLDs are not allowed.
  EXPECT_FALSE(IsSupportedKeySystem("org."));
  EXPECT_FALSE(IsSupportedKeySystem("com"));

  // Extra period.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.clear."));

  // Prefix.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example."));
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example"));

  // Incomplete.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.clea"));

  // Extra character.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.clearz"));

  // There are no child key systems for UsesAes.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.clear.foo"));
}

TEST_F(KeySystemsTest, IsSupportedKeySystemWithMediaMimeType_NoType) {
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(std::string(), no_codecs(),
                                                     kUsesAes));

  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(std::string(), no_codecs(),
                                                     "x-org.example.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), "x-org.example.clear.foo"));
}

// Tests the second registered container type.
// TODO(ddorwin): Combined with TypesContainer1 in a future CL.
TEST_F(KeySystemsTest,
       IsSupportedKeySystemWithMediaMimeType_UsesAesDecryptor_TypesContainer2) {
  // Valid video types.
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoFoo, no_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoFoo, foovideo_codec(),
                                                    kUsesAes));

  // Audio in a video container.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_and_fooaudio_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, fooaudio_codec(), kUsesAes));

  // Extended codecs fail because this is handled by SimpleWebMimeRegistryImpl.
  // They should really pass canPlayType().
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_extended_codec(), kUsesAes));

  // Invalid codec format.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_dot_codec(), kUsesAes));

  // Non-container2 codec.
  EXPECT_FALSE(
      IsSupportedKeySystemWithMediaMimeType(kVideoFoo, vp8_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoFoo, unknown_codec(),
                                                     kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoFoo, mixed_codecs(),
                                                     kUsesAes));

  // Valid audio types.
  EXPECT_TRUE(
      IsSupportedKeySystemWithAudioMimeType(kAudioFoo, no_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(kAudioFoo, fooaudio_codec(),
                                                    kUsesAes));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, foovideo_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, foovideo_and_fooaudio_codecs(), kUsesAes));

  // Non-container2 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(kAudioFoo, vorbis_codec(),
                                                     kUsesAes));
}

//
// Non-AesDecryptor-based key system.
//

TEST_F(KeySystemsTest, Basic_ExternalDecryptor) {
  if (!CanRunExternalKeySystemTests()) return;

  EXPECT_TRUE(IsSupportedKeySystem(kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(),
                                                    kExternal));

  EXPECT_FALSE(CanUseAesDecryptor(kExternal));
#if defined(ENABLE_PEPPER_CDMS)
  EXPECT_EQ("application/x-ppapi-external-cdm", GetPepperType(kExternal));
#endif  // defined(ENABLE_PEPPER_CDMS)
}

TEST_F(
    KeySystemsTest,
    IsSupportedKeySystemWithMediaMimeType_ExternalDecryptor_TypesContainer1) {
  if (!CanRunExternalKeySystemTests()) return;

  // Valid video types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(),
                                                    kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp8_codec(),
                                                    kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp80_codec(),
                                                    kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp9_codec(),
                                                    kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vp90_codec(),
                                                    kExternal));

  // Audio in a video container.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_and_vorbis_codecs(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_and_vorbis_codecs(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, vorbis_codec(),
                                                     kExternal));

  // Non-Webm codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, foovideo_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, unknown_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, mixed_codecs(),
                                                     kExternal));

  // Valid audio types.
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(kAudioWebM, no_codecs(),
                                                    kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(kAudioWebM, vorbis_codec(),
                                                    kExternal));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(kAudioWebM, vp8_codec(),
                                                     kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, vp8_and_vorbis_codecs(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(kAudioWebM, vp9_codec(),
                                                     kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, vp9_and_vorbis_codecs(), kExternal));

  // Non-Webm codec.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, fooaudio_codec(), kExternal));
}

TEST_F(
    KeySystemsTest,
    IsSupportedKeySystemWithMediaMimeType_ExternalDecryptor_TypesContainer2) {
  if (!CanRunExternalKeySystemTests()) return;

  // Valid video types.
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoFoo, no_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoFoo, foovideo_codec(),
                                                    kExternal));

  // Audio in a video container.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_and_fooaudio_codecs(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, fooaudio_codec(), kExternal));

  // Extended codecs fail because this is handled by SimpleWebMimeRegistryImpl.
  // They should really pass canPlayType().
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_extended_codec(), kExternal));

  // Invalid codec format.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_dot_codec(), kExternal));

  // Non-container2 codecs.
  EXPECT_FALSE(
      IsSupportedKeySystemWithMediaMimeType(kVideoFoo, vp8_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoFoo, unknown_codec(),
                                                     kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(kVideoFoo, mixed_codecs(),
                                                     kExternal));

  // Valid audio types.
  EXPECT_TRUE(
      IsSupportedKeySystemWithAudioMimeType(kAudioFoo, no_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(kAudioFoo, fooaudio_codec(),
                                                    kExternal));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, foovideo_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, foovideo_and_fooaudio_codecs(), kExternal));

  // Non-container2 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(kAudioFoo, vorbis_codec(),
                                                     kExternal));
}

TEST_F(KeySystemsTest, KeySystemNameForUMA) {
  EXPECT_EQ("ClearKey", GetKeySystemNameForUMA(kClearKey));

  // External Clear Key never has a UMA name.
  if (CanRunExternalKeySystemTests())
    EXPECT_EQ("Unknown", GetKeySystemNameForUMA(kExternalClearKey));
}

TEST_F(KeySystemsTest, KeySystemsUpdate) {
  EXPECT_TRUE(IsSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(), kUsesAes));

  if (CanRunExternalKeySystemTests()) {
    EXPECT_TRUE(IsSupportedKeySystem(kExternal));
    EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(),
                                                      kExternal));
  }

  UpdateClientKeySystems();

  EXPECT_TRUE(IsSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(
      IsSupportedKeySystemWithMediaMimeType(kVideoWebM, no_codecs(), kUsesAes));
  if (CanRunExternalKeySystemTests())
    EXPECT_FALSE(IsSupportedKeySystem(kExternal));
}

TEST_F(KeySystemsPotentiallySupportedNamesTest, PotentiallySupportedNames) {
  EXPECT_FALSE(IsSupportedKeySystem("org.w3"));
  EXPECT_FALSE(IsSupportedKeySystem("org.w3."));
  EXPECT_FALSE(IsSupportedKeySystem("org.w3.clearke"));
  EXPECT_TRUE(IsSupportedKeySystem("org.w3.clearkey"));
  EXPECT_FALSE(IsSupportedKeySystem("org.w3.clearkeys"));

  EXPECT_FALSE(IsSupportedKeySystem("com.widevine"));
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine."));
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine.alph"));
  EXPECT_TRUE(IsSupportedKeySystem("com.widevine.alpha"));
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine.beta"));
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine.alphabeta"));
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine.alpha.beta"));

  EXPECT_FALSE(IsSupportedKeySystem("org.chromium"));
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium."));
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium.externalclearke"));
  EXPECT_TRUE(IsSupportedKeySystem("org.chromium.externalclearkey"));
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium.externalclearkeys"));
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium.externalclearkey."));
  EXPECT_TRUE(IsSupportedKeySystem("org.chromium.externalclearkey.something"));
  EXPECT_FALSE(
      IsSupportedKeySystem("org.chromium.externalclearkey.something.else"));
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium.externalclearkey.other"));
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium.other"));

  EXPECT_FALSE(IsSupportedKeySystem("com.chromecast"));
  EXPECT_FALSE(IsSupportedKeySystem("com.chromecast."));
  EXPECT_TRUE(IsSupportedKeySystem("com.chromecast.something"));
  EXPECT_FALSE(IsSupportedKeySystem("com.chromecast.something.else"));
  EXPECT_FALSE(IsSupportedKeySystem("com.chromecast.other"));

  EXPECT_FALSE(IsSupportedKeySystem("x-"));
  EXPECT_TRUE(IsSupportedKeySystem("x-something"));
  EXPECT_FALSE(IsSupportedKeySystem("x-something.else"));
  EXPECT_FALSE(IsSupportedKeySystem("x-other"));
}

}  // namespace media
