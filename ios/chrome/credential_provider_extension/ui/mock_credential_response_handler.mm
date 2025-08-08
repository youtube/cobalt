// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/mock_credential_response_handler.h"

#import "base/strings/string_number_conversions.h"
#import "ios/chrome/credential_provider_extension/passkey_util.h"

namespace {

NSArray<NSData*>* SecurityDomainSecrets() {
  std::vector<uint8_t> sds;
  base::HexStringToBytes(
      "1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF", &sds);
  return [NSArray arrayWithObjects:[NSData dataWithBytes:sds.data()
                                                  length:sds.size()],
                                   nil];
}

}  // namespace

@implementation MockCredentialResponseHandler

- (void)userSelectedPassword:(ASPasswordCredential*)credential {
  self.passwordCredential = credential;
  if (self.receivedCredentialBlock) {
    self.receivedCredentialBlock();
  }
}

- (void)userSelectedPasskey:(ASPasskeyAssertionCredential*)credential
    API_AVAILABLE(ios(17.0)) {
  self.passkeyCredential = credential;
  if (self.receivedPasskeyBlock) {
    self.receivedPasskeyBlock();
  }
}

- (void)userSelectedPasskey:(id<Credential>)passkey
              clientDataHash:(NSData*)clientDataHash
          allowedCredentials:(NSArray<NSData*>*)allowedCredentials
    userVerificationRequired:(BOOL)userVerificationRequired {
  if (@available(iOS 17.0, *)) {
    [self userSelectedPasskey:PerformPasskeyAssertion(passkey, clientDataHash,
                                                      nil,
                                                      SecurityDomainSecrets())];
  }
}

- (void)userCancelledRequestWithErrorCode:(ASExtensionErrorCode)errorCode {
  self.errorCode = errorCode;
  if (self.receivedErrorCodeBlock) {
    self.receivedErrorCodeBlock();
  }
}

- (void)completeExtensionConfigurationRequest {
  // No-op.
}

- (NSString*)gaia {
  return nil;
}

@end
