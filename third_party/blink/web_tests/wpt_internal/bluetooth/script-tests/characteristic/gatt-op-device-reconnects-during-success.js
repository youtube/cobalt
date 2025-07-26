bluetooth_test(() => {
  let val = new Uint8Array([1]);
  return setBluetoothFakeAdapter('DisconnectingDuringSuccessGATTOperationAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['health_thermometer']}]}))
    .then(device => device.gatt.connect())
    .then(gatt => gatt.getPrimaryService('health_thermometer'))
    .then(service => service.getCharacteristic('measurement_interval'))
    .then(characteristic => {
      let disconnected = eventPromise(characteristic.service.device, 'gattserverdisconnected');
      let promise = assert_promise_rejects_with_message(
        characteristic.CALLS([
          readValue()|
          writeValue(val)|
          writeValueWithResponse(val)|
          writeValueWithoutResponse(val)|
          startNotifications()]),
        new DOMException('GATT Server is disconnected. Cannot perform GATT operations. ' +
                         '(Re)connect first with `device.gatt.connect`.',
                         'NetworkError'));
      return disconnected.then(() => characteristic.service.device.gatt.connect())
                         .then(() => promise);
    });
}, 'Device reconnects during a FUNCTION_NAME call that succeeds. Reject ' +
   'with NetworkError.');
