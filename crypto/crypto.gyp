# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'crypto',
      'product_name': 'crcrypto',  # Avoid colliding with OpenSSL's libcrypto
      'type': '<(library)',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'msvs_disabled_warnings': [
        4018,
      ],
      'conditions': [
        [ 'OS == "linux" or OS == "freebsd" or OS == "openbsd" or OS == "solaris"', {
          'conditions': [
            [ 'chromeos==1', {
                'sources/': [ ['include', '_chromeos\\.cc$'] ]
              },
            ],
            [ 'use_openssl==1', {
                'dependencies': [
                  '../third_party/openssl/openssl.gyp:openssl',
                ],
              }, {  # use_openssl==0
                'dependencies': [
                  '../build/linux/system.gyp:nss',
                ],
                'export_dependent_settings': [
                  '../build/linux/system.gyp:nss',
                ],
              }
            ],
          ],
        }, {  # OS != "linux" and OS != "freebsd" and OS != "openbsd" and OS != "solaris"
            'sources/': [
              ['exclude', '_nss\.cc$'],
            ],
        }],
        [ 'OS == "freebsd" or OS == "openbsd"', {
          'link_settings': {
            'libraries': [
              '-L/usr/local/lib -lexecinfo',
              ],
            },
          },
        ],
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
        [ 'OS == "mac" or OS == "win"', {
            'dependencies': [
              '../third_party/nss/nss.gyp:nss',
            ],
        },],
        [ 'OS != "win"', {
            'sources!': [
              'capi_util.h',
              'capi_util.cc',
            ],
        },],
        [ 'use_openssl==1', {
            # TODO(joth): Use a glob to match exclude patterns once the
            #             OpenSSL file set is complete.
            'sources!': [
              'encryptor_nss.cc',
              'hmac_nss.cc',
              'nss_util.cc',
              'nss_util.h',
              'rsa_private_key_nss.cc',
              'secure_hash_default.cc',
              'signature_creator_nss.cc',
              'signature_verifier_nss.cc',
              'symmetric_key_nss.cc',
              'third_party/nss/blapi.h',
              'third_party/nss/blapit.h',
              'third_party/nss/sha256.h',
              'third_party/nss/sha512.cc',
            ],
          }, {
            'sources!': [
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
        'capi_util.cc',
        'capi_util.h',
        'crypto_module_blocking_password_delegate.h',
        'cssm_init.cc',
        'cssm_init.h',
        'encryptor.h',
        'encryptor_mac.cc',
        'encryptor_nss.cc',
        'encryptor_openssl.cc',
        'encryptor_win.cc',
        'hmac.h',
        'hmac_mac.cc',
        'hmac_nss.cc',
        'hmac_openssl.cc',
        'hmac_win.cc',
        'mac_security_services_lock.cc',
        'mac_security_services_lock.h',
        'openssl_util.cc',
        'openssl_util.h',
        'nss_util.cc',
        'nss_util.h',
        'nss_util_internal.h',
        'rsa_private_key.h',
        'rsa_private_key.cc',
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
        'signature_verifier_mac.cc',
        'signature_verifier_nss.cc',
        'signature_verifier_openssl.cc',
        'signature_verifier_win.cc',
        'symmetric_key.h',
        'symmetric_key_mac.cc',
        'symmetric_key_nss.cc',
        'symmetric_key_openssl.cc',
        'symmetric_key_win.cc',
        'third_party/nss/blapi.h',
        'third_party/nss/blapit.h',
        'third_party/nss/sha256.h',
        'third_party/nss/sha512.cc',
      ],
    },
    {
      'target_name': 'crypto_unittests',
      'type': 'executable',
      'sources': [
        # Infrastructure files.
        'run_all_unittests.cc',

        # Tests.
        'encryptor_unittest.cc',
        'hmac_unittest.cc',
        'rsa_private_key_unittest.cc',
        'rsa_private_key_nss_unittest.cc',
        'secure_hash_unittest.cc',
        'sha2_unittest.cc',
        'signature_creator_unittest.cc',
        'signature_verifier_unittest.cc',
        'symmetric_key_unittest.cc',
      ],
      'dependencies': [
        'crypto',
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'conditions': [
        [ 'OS == "linux" or OS == "freebsd" or OS == "openbsd" or OS == "solaris"', {
          'conditions': [
            [ 'linux_use_tcmalloc==1', {
                'dependencies': [
                  '../base/allocator/allocator.gyp:allocator',
                ],
              },
            ],
          ],
          'dependencies': [
            '../build/linux/system.gyp:nss',
          ],
        }, {  # OS != "linux" and OS != "freebsd" and OS != "openbsd" and OS != "solaris"
          'sources!': [
            'rsa_private_key_nss_unittest.cc',
          ]
        }],
        [ 'OS == "mac" or OS == "win"', {
          'dependencies': [
            '../third_party/nss/nss.gyp:nss',
          ],
        }],
        [ 'use_openssl==1', {
          'sources!': [
            'rsa_private_key_nss_unittest.cc',
          ],
        }],
      ],
    },
  ],
}
