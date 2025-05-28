// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_REAUTH_IOS_DEVICE_AUTHENTICATOR_FACTORY_H_
#define IOS_CHROME_BROWSER_DEVICE_REAUTH_IOS_DEVICE_AUTHENTICATOR_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class DeviceAuthenticatorProxy;
class IOSDeviceAuthenticator;
class ProfileIOS;

namespace device_reauth {
class DeviceAuthParams;
}

@protocol ReauthenticationProtocol;

// Singleton that owns all DeviceAuthenticatorProxy and associates them with
// profiles.
class DeviceAuthenticatorProxyFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static DeviceAuthenticatorProxy* GetForProfile(ProfileIOS* profile);
  static DeviceAuthenticatorProxyFactory* GetInstance();

 private:
  friend class base::NoDestructor<DeviceAuthenticatorProxyFactory>;

  DeviceAuthenticatorProxyFactory();
  ~DeviceAuthenticatorProxyFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

// Creates an IOSDeviceAuthenticator. It is built on top of a
// DeviceAuthenticatorProxy. `reauth_module` is the component that provides the
// device reauth functionalities. `profile` is the ProfileIOS the
// underlying DeviceAuthenticatorProxy is attached to. `params` contains configs
// for the authentication.
std::unique_ptr<IOSDeviceAuthenticator> CreateIOSDeviceAuthenticator(
    id<ReauthenticationProtocol> reauth_module,
    ProfileIOS* profile,
    const device_reauth::DeviceAuthParams& params);

#endif  // IOS_CHROME_BROWSER_DEVICE_REAUTH_IOS_DEVICE_AUTHENTICATOR_FACTORY_H_
