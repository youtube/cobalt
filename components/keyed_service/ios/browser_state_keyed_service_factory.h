// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_IOS_BROWSER_STATE_KEYED_SERVICE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_IOS_BROWSER_STATE_KEYED_SERVICE_FACTORY_H_

#include <memory>

#include "base/compiler_specific.h"
#include "components/keyed_service/core/keyed_service_export.h"
#include "components/keyed_service/core/keyed_service_factory.h"

class BrowserStateDependencyManager;
class KeyedService;

namespace web {
class BrowserState;
}

// Base class for Factories that take a BrowserState object and return some
// service on a one-to-one mapping. Each factory that derives from this class
// *must* be a Singleton (only unit tests don't do that).
//
// We do this because services depend on each other and we need to control
// shutdown/destruction order. In each derived classes' constructors, the
// implementors must explicitly state which services are depended on.
class KEYED_SERVICE_EXPORT BrowserStateKeyedServiceFactory
    : public KeyedServiceFactory {
 public:
  // A callback that supplies the instance of a KeyedService for a given
  // BrowserState. This is used primarily for testing, where we want to feed
  // a specific test double into the BSKSF system.
  using TestingFactory = base::RepeatingCallback<std::unique_ptr<KeyedService>(
      web::BrowserState* context)>;

  BrowserStateKeyedServiceFactory(const BrowserStateKeyedServiceFactory&) =
      delete;
  BrowserStateKeyedServiceFactory& operator=(
      const BrowserStateKeyedServiceFactory&) = delete;

  // Associates |testing_factory| with |context| so that |testing_factory| is
  // used to create the KeyedService when requested.  |testing_factory| can be
  // empty to signal that KeyedService should be null. Multiple calls to
  // SetTestingFactory() are allowed; previous services will be shut down.
  void SetTestingFactory(web::BrowserState* context,
                         TestingFactory testing_factory);

 protected:
  // BrowserStateKeyedServiceFactories must communicate with a
  // BrowserStateDependencyManager. For all non-test code, write your subclass
  // constructors like this:
  //
  //   MyServiceFactory::MyServiceFactory()
  //     : BrowserStateKeyedServiceFactory(
  //         "MyService",
  //         BrowserStateDependencyManager::GetInstance()) {
  //   }
  BrowserStateKeyedServiceFactory(const char* name,
                                  BrowserStateDependencyManager* manager);
  ~BrowserStateKeyedServiceFactory() override;

  // Common implementation that maps |context| to some service object. Deals
  // with incognito and testing contexts per subclass instructions.  If |create|
  // is true, the service will be created using BuildServiceInstanceFor() if it
  // doesn't already exist.
  KeyedService* GetServiceForBrowserState(web::BrowserState* context,
                                          bool create);

  // Interface for people building a concrete FooServiceFactory: --------------

  // Finds which browser state (if any) to use.
  virtual web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const;

  // By default, we create instances of a service lazily and wait until
  // GetForBrowserState() is called on our subclass. Some services need to be
  // created as soon as the BrowserState has been brought up.
  virtual bool ServiceIsCreatedWithBrowserState() const;

  // By default, TestingBrowserStates will be treated like normal contexts.
  // You can override this so that by default, the service associated with the
  // TestingBrowserState is NULL. (This is just a shortcut around
  // SetTestingFactory() to make sure our contexts don't directly refer to the
  // services they use.)
  bool ServiceIsNULLWhileTesting() const override;

  // Interface for people building a type of BrowserStateKeyedFactory: -------

  // All subclasses of BrowserStateKeyedServiceFactory must return a
  // KeyedService instead of just a BrowserStateKeyedBase.
  virtual std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const = 0;

  // A helper object actually listens for notifications about BrowserState
  // destruction, calculates the order in which things are destroyed and then
  // does a two pass shutdown.
  //
  // First, BrowserStateShutdown() is called on every ServiceFactory and will
  // usually call KeyedService::Shutdown(), which gives each
  // KeyedService a chance to remove dependencies on other
  // services that it may be holding.
  //
  // Secondly, BrowserStateDestroyed() is called on every ServiceFactory
  // and the default implementation removes it from |mapping_| and deletes
  // the pointer.
  virtual void BrowserStateShutdown(web::BrowserState* context);
  virtual void BrowserStateDestroyed(web::BrowserState* context);

 private:
  // Registers any user preferences on this service. This should be overriden by
  // any service that wants to register profile-specific preferences.
  virtual void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) {}

  // KeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      void* context) const final;
  bool IsOffTheRecord(void* context) const final;

  // KeyedServiceBaseFactory:
  void* GetContextToUse(void* context) const final;
  bool ServiceIsCreatedWithContext() const final;
  void ContextShutdown(void* context) final;
  void ContextDestroyed(void* context) final;
  void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) final;
  void CreateServiceNow(void* context) final;
};

#endif  // COMPONENTS_KEYED_SERVICE_IOS_BROWSER_STATE_KEYED_SERVICE_FACTORY_H_
