{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'crypto',
      'type': '<(component)',
      'product_name': 'crcrypto',  # Avoid colliding with OpenSSL's libcrypto
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/third_party/boringssl/boringssl.gyp:crypto',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
      'defines': [
        'CRYPTO_IMPLEMENTATION',
      ],
      'msvs_disabled_warnings': [
        4018,
      ],
      'sources': [
        'aead.cc',
        'aead.h',
        'crypto_export.h',
        'ec_private_key.cc',
        'ec_private_key.h',
        'ec_signature_creator.cc',
        'ec_signature_creator.h',
        'ec_signature_creator_impl.cc',
        'ec_signature_creator_impl.h',
        'encryptor.cc',
        'encryptor.h',
        'hkdf.cc',
        'hkdf.h',
        'hmac.cc',
        'hmac.h',
        'mac_security_services_lock.cc',
        'mac_security_services_lock.h',

        # TODO(brettw) these mocks should be moved to a test_support_crypto
        # target if possible.
        'openssl_util.cc',
        'openssl_util.h',
        'p224.cc',
        'p224.h',
        'p224_spake.cc',
        'p224_spake.h',
        'random.cc',
        'random.h',
        'rsa_private_key.cc',
        'rsa_private_key.h',
        'secure_hash.cc',
        'secure_hash.h',
        'secure_util.cc',
        'secure_util.h',
        'sha2.cc',
        'sha2.h',
        'signature_creator.cc',
        'signature_creator.h',
        'signature_verifier.cc',
        'signature_verifier.h',
        'symmetric_key.cc',
        'symmetric_key.h',
        'wincrypt_shim.h',
      ],
    },
    {
      'target_name': 'crypto_unittests',
      'type': '<(gtest_target_type)',
      'sources': [
        'aead_unittest.cc',
        'ec_private_key_unittest.cc',
        'ec_signature_creator_unittest.cc',
        'encryptor_unittest.cc',
        'hmac_unittest.cc',
        'p224_spake_unittest.cc',
        'p224_unittest.cc',
        'random_unittest.cc',
        'rsa_private_key_unittest.cc',
        'secure_hash_unittest.cc',
        'sha2_unittest.cc',
        'signature_creator_unittest.cc',
        'signature_verifier_unittest.cc',
        'symmetric_key_unittest.cc',
      ],
      'dependencies': [
        'crypto',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'includes': ['<(DEPTH)/base/test/test.gypi'],
    },
  ],
  'conditions': [
    ['cobalt==1', {
      'targets': [
        {
          'target_name': 'crypto_unittests_deploy',
          'type': 'none',
          'dependencies': [
            'crypto_unittests',
          ],
          'variables': {
            'executable_name': 'crypto_unittests',
          },
          'includes': [ '../starboard/build/deploy.gypi' ],
        },
      ],
    }],
  ],
}
