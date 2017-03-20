# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    # Put all transitive dependencies for Windows HMAC here.
    # This is required so that we can build them for nacl win64.
    'hmac_win64_related_sources': [
      'hmac.cc',
      'hmac.h',
      'hmac_win.cc',
      'secure_util.cc',
      'secure_util.h',
      'symmetric_key.h',
      'symmetric_key_win.cc',
      'third_party/nss/chromium-sha256.h',
      'third_party/nss/sha512.cc',
    ],
  },
  'targets': [
    {
      'target_name': 'crypto',
      'type': '<(component)',
      'product_name': 'crcrypto',  # Avoid colliding with OpenSSL's libcrypto
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'defines': [
        'CRYPTO_IMPLEMENTATION',
      ],
      'msvs_disabled_warnings': [
        4018,
      ],
      'conditions': [
        [ 'os_posix == 1 and OS != "mac" and OS != "ios" and OS != "android" and OS != "lb_shell"', {
          'dependencies': [
            '../build/linux/system.gyp:ssl',
          ],
          'export_dependent_settings': [
            '../build/linux/system.gyp:ssl',
          ],
          'conditions': [
            [ 'chromeos==1', {
                'sources/': [ ['include', '_chromeos\\.cc$'] ]
              },
            ],
          ],
        }, {  # os_posix != 1 or OS == "mac" or OS == "ios" or OS == "android"
            'sources/': [
              ['exclude', '_nss\.cc$'],
              ['include', 'ec_private_key_nss\.cc$'],
              ['include', 'ec_signature_creator_nss\.cc$'],
              ['include', 'encryptor_nss\.cc$'],
              ['include', 'hmac_nss\.cc$'],
              ['include', 'signature_verifier_nss\.cc$'],
              ['include', 'symmetric_key_nss\.cc$'],
            ],
            'sources!': [
              'hmac_win.cc',
              'openpgp_symmetric_encryption.cc',
              'openpgp_symmetric_encryption.h',
              'symmetric_key_win.cc',
            ],
        }],
        [ 'OS != "mac" and OS != "ios"', {
          'sources!': [
            'apple_keychain.h',
            'mock_apple_keychain.cc',
            'mock_apple_keychain.h',
          ],
        }],
        [ 'OS == "android"', {
            'dependencies': [
              '../third_party/openssl/openssl.gyp:openssl',
            ],
            'sources/': [
              ['exclude', 'ec_private_key_nss\.cc$'],
              ['exclude', 'ec_signature_creator_nss\.cc$'],
              ['exclude', 'encryptor_nss\.cc$'],
              ['exclude', 'hmac_nss\.cc$'],
              ['exclude', 'signature_verifier_nss\.cc$'],
              ['exclude', 'symmetric_key_nss\.cc$'],
            ],
        }],
        [ 'os_bsd==1', {
          'link_settings': {
            'libraries': [
              '-L/usr/local/lib -lexecinfo',
              ],
            },
          },
        ],
        [ 'OS == "ios"', {
          'sources!': [
            # This class is stubbed out on iOS.
            'rsa_private_key.cc',
          ],
        }],
        [ 'OS == "mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Security.framework',
            ],
          },
        }, {  # OS != "mac"
          'sources!': [
            'cssm_init.cc',
            'cssm_init.h',
            'mac_security_services_lock.cc',
            'mac_security_services_lock.h',
          ],
        }],
        [ 'OS == "mac" or OS == "ios" or OS == "win"', {
          'dependencies': [
            '../third_party/nss/nss.gyp:nspr',
            '../third_party/nss/nss.gyp:nss',
          ],
          'export_dependent_settings': [
            '../third_party/nss/nss.gyp:nspr',
            '../third_party/nss/nss.gyp:nss',
          ],
        }],
        [ 'OS != "win"', {
          'sources!': [
            'capi_util.h',
            'capi_util.cc',
          ],
        }],
        ['OS == "lb_shell"', {
          'dependencies' : [
            '<(lbshell_root)/build/projects/posix_emulation.gyp:posix_emulation',
          ],
        }],
        ['OS == "starboard"', {
          'dependencies' : [
            '<(DEPTH)/starboard/starboard.gyp:starboard',
          ],
        }],
        ['OS == "lb_shell" or OS == "starboard"', {
          'dependencies' : [
            '../third_party/openssl/openssl.gyp:openssl',
          ],
          'sources/': [
            ['exclude', '_nss.cc$'],
          ],
        }],
        [ 'use_openssl==1', {
            # TODO(joth): Use a glob to match exclude patterns once the
            #             OpenSSL file set is complete.
            'sources!': [
              'ec_private_key_nss.cc',
              'ec_signature_creator_nss.cc',
              'encryptor_nss.cc',
              'hmac_nss.cc',
              'nss_util.cc',
              'nss_util.h',
              'openpgp_symmetric_encryption.cc',
              'rsa_private_key_nss.cc',
              'secure_hash_default.cc',
              'signature_creator_nss.cc',
              'signature_verifier_nss.cc',
              'symmetric_key_nss.cc',
              'third_party/nss/chromium-blapi.h',
              'third_party/nss/chromium-blapit.h',
              'third_party/nss/chromium-nss.h',
              'third_party/nss/chromium-sha256.h',
              'third_party/nss/pk11akey.cc',
              'third_party/nss/secsign.cc',
              'third_party/nss/sha512.cc',
            ],
          }, {
            'sources!': [
              'ec_private_key_openssl.cc',
              'ec_signature_creator_openssl.cc',
              'encryptor_openssl.cc',
              'hmac_openssl.cc',
              'openssl_util.cc',
              'openssl_util.h',
              'rsa_private_key_openssl.cc',
              'secure_hash_openssl.cc',
              'signature_creator_openssl.cc',
              'signature_verifier_openssl.cc',
              'symmetric_key_openssl.cc',
            ],
        },],
      ],
      'sources': [
        # NOTE: all transitive dependencies of HMAC on windows need
        #     to be placed in the source list above.
        '<@(hmac_win64_related_sources)',
        'apple_keychain.h',
        'apple_keychain_ios.mm',
        'apple_keychain_mac.mm',
        'capi_util.cc',
        'capi_util.h',
        'crypto_export.h',
        'crypto_module_blocking_password_delegate.h',
        'cssm_init.cc',
        'cssm_init.h',
        'ghash.cc',
        'ghash.h',
        'ec_private_key.h',
        'ec_private_key_nss.cc',
        'ec_private_key_openssl.cc',
        'ec_signature_creator.cc',
        'ec_signature_creator.h',
        'ec_signature_creator_impl.h',
        'ec_signature_creator_nss.cc',
        'ec_signature_creator_openssl.cc',
        'encryptor.cc',
        'encryptor.h',
        'encryptor_nss.cc',
        'encryptor_openssl.cc',
        'hmac_nss.cc',
        'hmac_openssl.cc',
        'mac_security_services_lock.cc',
        'mac_security_services_lock.h',
        'mock_apple_keychain.cc',
        'mock_apple_keychain.h',
        'mock_apple_keychain_ios.cc',
        'mock_apple_keychain_mac.cc',
        'p224_spake.cc',
        'p224_spake.h',
        'nss_util.cc',
        'nss_util.h',
        'nss_util_internal.h',
        'openpgp_symmetric_encryption.cc',
        'openpgp_symmetric_encryption.h',
        'openssl_util.cc',
        'openssl_util.h',
        'p224.cc',
        'p224.h',
        'random.h',
        'random.cc',
        'rsa_private_key.cc',
        'rsa_private_key.h',
        'rsa_private_key_ios.cc',
        'rsa_private_key_mac.cc',
        'rsa_private_key_nss.cc',
        'rsa_private_key_openssl.cc',
        'rsa_private_key_win.cc',
        'scoped_capi_types.h',
        'scoped_nss_types.h',
        'secure_hash.h',
        'secure_hash_default.cc',
        'secure_hash_openssl.cc',
        'sha2.cc',
        'sha2.h',
        'signature_creator.h',
        'signature_creator_mac.cc',
        'signature_creator_nss.cc',
        'signature_creator_openssl.cc',
        'signature_creator_win.cc',
        'signature_verifier.h',
        'signature_verifier_nss.cc',
        'signature_verifier_openssl.cc',
        'symmetric_key_nss.cc',
        'symmetric_key_openssl.cc',
        'third_party/nss/chromium-blapi.h',
        'third_party/nss/chromium-blapit.h',
        'third_party/nss/chromium-nss.h',
        'third_party/nss/pk11akey.cc',
        'third_party/nss/secsign.cc',
      ],
    },
    {
      'target_name': 'crypto_unittests',
      'type': '<(gtest_target_type)',
      'sources': [
        # Infrastructure files.
        'run_all_unittests.cc',

        # Tests.
        'ec_private_key_unittest.cc',
        'ec_signature_creator_unittest.cc',
        'encryptor_unittest.cc',
        'ghash_unittest.cc',
        'hmac_unittest.cc',
        'nss_util_unittest.cc',
        'p224_unittest.cc',
        'p224_spake_unittest.cc',
        'random_unittest.cc',
        'rsa_private_key_unittest.cc',
        'rsa_private_key_nss_unittest.cc',
        'secure_hash_unittest.cc',
        'sha2_unittest.cc',
        'signature_creator_unittest.cc',
        'signature_verifier_unittest.cc',
        'symmetric_key_unittest.cc',
        'openpgp_symmetric_encryption_unittest.cc',
      ],
      'dependencies': [
        'crypto',
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'conditions': [
        [ 'os_posix == 1 and OS != "mac" and OS != "android" and OS != "ios" and OS != "lb_shell"', {
          'conditions': [
            [ 'linux_use_tcmalloc==1', {
                'dependencies': [
                  '../base/allocator/allocator.gyp:allocator',
                ],
              },
            ],
          ],
          'dependencies': [
            '../build/linux/system.gyp:ssl',
          ],
        }, {  # os_posix != 1 or OS == "mac" or OS == "android" or OS == "ios"
          'sources!': [
            'rsa_private_key_nss_unittest.cc',
            'openpgp_symmetric_encryption_unittest.cc',
          ]
        }],
        [ 'OS == "mac" or OS == "ios" or OS == "win"', {
          'dependencies': [
            '../third_party/nss/nss.gyp:nss',
          ],
        }],
        ['OS == "ios"', {
          'sources!': [
            # These tests are excluded because they test classes that are not
            # implemented on iOS.
            'rsa_private_key_unittest.cc',
            'signature_creator_unittest.cc',
            'signature_verifier_unittest.cc',
          ],
        }],
        [ 'OS == "mac"', {
          'dependencies': [
            '../third_party/nss/nss.gyp:nspr',
          ],
        }],
        [ 'use_openssl==1', {
          'sources!': [
            'nss_util_unittest.cc',
            'openpgp_symmetric_encryption_unittest.cc',
            'rsa_private_key_nss_unittest.cc',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    [ 'OS == "win"', {
      'targets': [
        {
          'target_name': 'crypto_nacl_win64',
          # We do not want nacl_helper to depend on NSS because this would
          # require including a 64-bit copy of NSS. Thus, use the native APIs
          # for the helper.
          'type': '<(component)',
          'dependencies': [
            '../base/base.gyp:base_nacl_win64',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations_win64',
          ],
          'sources': [
            '<@(hmac_win64_related_sources)',
          ],
          'defines': [
           'CRYPTO_IMPLEMENTATION',
           '<@(nacl_win64_defines)',
          ],
          'msvs_disabled_warnings': [
            4018,
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
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
