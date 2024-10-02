// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_cbperipheral_mac.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "device/bluetooth/test/bluetooth_test_mac.h"
#include "device/bluetooth/test/mock_bluetooth_cbcharacteristic_mac.h"
#include "device/bluetooth/test/mock_bluetooth_cbdescriptor_mac.h"
#include "device/bluetooth/test/mock_bluetooth_cbservice_mac.h"

using base::mac::ObjCCast;
using base::scoped_nsobject;

@interface MockCBPeripheral () {
  scoped_nsobject<NSUUID> _identifier;
  scoped_nsobject<NSString> _name;
  id<CBPeripheralDelegate> _delegate;
  scoped_nsobject<NSMutableArray> _services;
}

@end

@implementation MockCBPeripheral

@synthesize state = _state;
@synthesize delegate = _delegate;
@synthesize bluetoothTestMac = _bluetoothTestMac;

- (instancetype)init {
  [self doesNotRecognizeSelector:_cmd];
  return self;
}

- (instancetype)initWithUTF8StringIdentifier:(const char*)utf8Identifier {
  return [self initWithUTF8StringIdentifier:utf8Identifier name:nil];
}

- (instancetype)initWithUTF8StringIdentifier:(const char*)utf8Identifier
                                        name:(NSString*)name {
  scoped_nsobject<NSUUID> identifier(
      [[NSUUID alloc] initWithUUIDString:@(utf8Identifier)]);
  return [self initWithIdentifier:identifier name:name];
}

- (instancetype)initWithIdentifier:(NSUUID*)identifier name:(NSString*)name {
  self = [super init];
  if (self) {
    _identifier.reset([identifier retain]);
    if (name) {
      _name.reset([name retain]);
    }
    _state = CBPeripheralStateDisconnected;
  }
  return self;
}

- (BOOL)isKindOfClass:(Class)aClass {
  if (aClass == [CBPeripheral class] ||
      [aClass isSubclassOfClass:[CBPeripheral class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (BOOL)isMemberOfClass:(Class)aClass {
  if (aClass == [CBPeripheral class] ||
      [aClass isSubclassOfClass:[CBPeripheral class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (void)setState:(CBPeripheralState)state {
  _state = state;
  if (_state == CBPeripheralStateDisconnected) {
    _services.reset();
  }
}

- (void)discoverServices:(NSArray*)serviceUUIDs {
  if (_bluetoothTestMac) {
    _bluetoothTestMac->OnFakeBluetoothServiceDiscovery();
  }
}

- (void)discoverCharacteristics:(NSArray*)characteristics
                     forService:(CBService*)service {
  if (_bluetoothTestMac) {
    _bluetoothTestMac->OnFakeBluetoothCharacteristicDiscovery();
  }
}

- (void)discoverDescriptorsForCharacteristic:(CBCharacteristic*)characteristic {
}

- (void)readValueForCharacteristic:(CBCharacteristic*)characteristic {
  DCHECK(_bluetoothTestMac);
  _bluetoothTestMac->OnFakeBluetoothCharacteristicReadValue();
}

- (void)writeValue:(NSData*)data
    forCharacteristic:(CBCharacteristic*)characteristic
                 type:(CBCharacteristicWriteType)type {
  DCHECK(_bluetoothTestMac);
  const uint8_t* buffer = static_cast<const uint8_t*>(data.bytes);
  std::vector<uint8_t> value(buffer, buffer + data.length);
  _bluetoothTestMac->OnFakeBluetoothCharacteristicWriteValue(value);
}

- (void)readValueForDescriptor:(CBDescriptor*)descriptor {
  DCHECK(_bluetoothTestMac);
  _bluetoothTestMac->OnFakeBluetoothDescriptorReadValue();
}

- (void)writeValue:(NSData*)data forDescriptor:(CBDescriptor*)descriptor {
  DCHECK(_bluetoothTestMac);
  const uint8_t* buffer = static_cast<const uint8_t*>(data.bytes);
  std::vector<uint8_t> value(buffer, buffer + data.length);
  _bluetoothTestMac->OnFakeBluetoothDescriptorWriteValue(value);
}

- (void)removeAllServices {
  [_services removeAllObjects];
}

- (void)addServices:(NSArray*)services {
  if (!_services) {
    _services.reset([[NSMutableArray alloc] init]);
  }
  for (CBUUID* uuid in services) {
    base::scoped_nsobject<MockCBService> service([[MockCBService alloc]
        initWithPeripheral:self.peripheral
                    CBUUID:uuid
                   primary:YES]);
    [_services addObject:[service service]];
  }
}

- (void)mockDidDiscoverServicesWithError:(NSError*)error {
  [_delegate peripheral:self.peripheral didDiscoverServices:error];
}

- (void)removeService:(CBService*)service {
  base::scoped_nsobject<CBService> serviceToRemove(service,
                                                   base::scoped_policy::RETAIN);
  DCHECK(serviceToRemove);
  [_services removeObject:serviceToRemove];
  [self didModifyServices:@[ serviceToRemove ]];
}

- (void)mockDidDiscoverServices {
  [_delegate peripheral:self.peripheral didDiscoverServices:nil];
}

- (void)mockDidDiscoverCharacteristicsForService:(CBService*)service {
  [_delegate peripheral:self.peripheral
      didDiscoverCharacteristicsForService:service
                                     error:nil];
}

- (void)mockDidDiscoverCharacteristicsForService:(CBService*)service
                                       WithError:(NSError*)error {
  [_delegate peripheral:self.peripheral
      didDiscoverCharacteristicsForService:service
                                     error:error];
}

- (void)mockDidDiscoverDescriptorsForCharacteristic:
    (CBCharacteristic*)characteristic {
  [_delegate peripheral:self.peripheral
      didDiscoverDescriptorsForCharacteristic:characteristic
                                        error:nil];
}

- (void)mockDidDiscoverDescriptorsForCharacteristic:
            (CBCharacteristic*)characteristic
                                          WithError:(NSError*)error {
  [_delegate peripheral:self.peripheral
      didDiscoverDescriptorsForCharacteristic:characteristic
                                        error:error];
}

- (void)mockDidDiscoverEvents {
  [self mockDidDiscoverServices];
  // BluetoothLowEnergyDeviceMac is expected to call
  // -[CBPeripheral discoverCharacteristics:forService:] for each services,
  // so -[<CBPeripheralDelegate peripheral:didDiscoverCharacteristicsForService:
  // error:] needs to be called for all services.
  for (CBService* service in _services.get()) {
    [self mockDidDiscoverCharacteristicsForService:service];
    for (CBCharacteristic* characteristic in service.characteristics) {
      // After discovering services, BluetoothLowEnergyDeviceMac is expected to
      // discover characteristics for all services.
      [_delegate peripheral:self.peripheral
          didDiscoverDescriptorsForCharacteristic:characteristic
                                            error:nil];
    }
  }
}

- (void)didModifyServices:(NSArray*)invalidatedServices {
  [_delegate peripheral:self.peripheral didModifyServices:invalidatedServices];
}

- (void)didDiscoverDescriptorsWithCharacteristic:
    (MockCBCharacteristic*)characteristic_mock {
  [_delegate peripheral:self.peripheral
      didDiscoverDescriptorsForCharacteristic:characteristic_mock.characteristic
                                        error:nil];
}

- (NSUUID*)identifier {
  return _identifier;
}

- (NSString*)name {
  return _name;
}

- (NSArray*)services {
  return _services;
}

- (CBPeripheral*)peripheral {
  return ObjCCast<CBPeripheral>(self);
}

- (void)setNotifyValue:(BOOL)notification
     forCharacteristic:(CBCharacteristic*)characteristic {
  _bluetoothTestMac->OnFakeBluetoothGattSetCharacteristicNotification(
      notification == YES);
}

@end
