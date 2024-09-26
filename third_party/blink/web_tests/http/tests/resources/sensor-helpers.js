import {ReportingMode, Sensor, SensorClientRemote, SensorReceiver, SensorRemote, SensorType} from '/gen/services/device/public/mojom/sensor.mojom.m.js';
import {SensorCreationResult, SensorInitParams_READ_BUFFER_SIZE_FOR_TESTS, SensorProvider, SensorProviderReceiver} from '/gen/services/device/public/mojom/sensor_provider.mojom.m.js';

// Default sensor frequency in default configurations.
const DEFAULT_FREQUENCY = 5;

// A "sliding window" that iterates over |data| and returns one item at a
// time, advancing and wrapping around as needed. |data| must be an array of
// arrays.
class RingBuffer {
  constructor(data) {
    this.bufferPosition_ = 0;
    // Validate |data|'s format and deep-copy every element.
    this.data_ = Array.from(data, element => {
      if (!Array.isArray(element)) {
        throw new TypeError('Every |data| element must be an array.');
      }
      return Array.from(element);
    })
  }

  next() {
    const value = this.data_[this.bufferPosition_];
    this.bufferPosition_ = (this.bufferPosition_ + 1) % this.data_.length;
    return { done: false, value: value };
  }

  [Symbol.iterator]() {
    return this;
  }
}

export function sensorMocks() {
  // Class that mocks Sensor interface defined in sensor.mojom
  class MockSensor {
    constructor(sensorRequest, buffer, reportingMode) {
      this.client_ = null;
      this.startShouldFail_ = false;
      this.notifyOnReadingChange_ = true;
      this.reportingMode_ = reportingMode;
      this.sensorReadingTimerId_ = null;
      this.readingData_ = null;
      this.suspendCalled_ = null;
      this.resumeCalled_ = null;
      this.addConfigurationCalled_ = null;
      this.removeConfigurationCalled_ = null;
      this.requestedFrequencies_ = [];
      this.buffer_ = buffer;
      this.resetBuffer();
      this.receiver_ = new SensorReceiver(this);
      this.receiver_.$.bindHandle(sensorRequest.handle);
    }

    // device.mojom.Sensor implementation
    // Mojo functions that return a value must be async and return an object
    // whose keys match the names declared in Mojo.

    // GetDefaultConfiguration() => (SensorConfiguration configuration)
    // Returns default configuration.
    async getDefaultConfiguration() {
      return { frequency: DEFAULT_FREQUENCY };
    }

    // AddConfiguration(SensorConfiguration configuration) => (bool success)
    // Adds configuration for the sensor and starts reporting fake data
    // through setSensorReading function.
    async addConfiguration(configuration) {
      assert_not_equals(configuration, null, "Invalid sensor configuration.");

      this.requestedFrequencies_.push(configuration.frequency);
      // Sort using descending order.
      this.requestedFrequencies_.sort(
          (first, second) => { return second - first });

      if (!this.startShouldFail_ )
        this.startReading();

      if (this.addConfigurationCalled_ != null)
        this.addConfigurationCalled_(this);

      return { success: !this.startShouldFail_ };
    }

    // RemoveConfiguration(SensorConfiguration configuration)
    // Removes sensor configuration from the list of active configurations and
    // stops notification about sensor reading changes if
    // requestedFrequencies_ is empty.
    removeConfiguration(configuration) {
      if (this.removeConfigurationCalled_ != null) {
        this.removeConfigurationCalled_(this);
      }

      const index = this.requestedFrequencies_.indexOf(configuration.frequency);
      if (index == -1)
        return;

      this.requestedFrequencies_.splice(index, 1);
      if (this.requestedFrequencies_.length === 0)
        this.stopReading();
    }

    // Suspend()
    suspend() {
      this.stopReading();
      if (this.suspendCalled_ != null) {
        this.suspendCalled_(this);
      }
    }

    // Resume()
    resume() {
      assert_equals(this.sensorReadingTimerId_, null);
      this.startReading();
      if (this.resumeCalled_ != null) {
        this.resumeCalled_(this);
      }
    }

    // ConfigureReadingChangeNotifications(bool enabled)
    // Configures whether to report a reading change when in ON_CHANGE
    // reporting mode.
    configureReadingChangeNotifications(notifyOnReadingChange) {
      this.notifyOnReadingChange_ = notifyOnReadingChange;
    }

    // Mock functions

    // Resets mock Sensor state.
    reset() {
      this.stopReading();

      this.startShouldFail_ = false;
      this.notifyOnReadingChange_ = true;
      this.readingData_ = null;
      this.requestedFrequencies_ = [];
      this.suspendCalled_ = null;
      this.resumeCalled_ = null;
      this.addConfigurationCalled_ = null;
      this.removeConfigurationCalled_ = null;
      this.resetBuffer();
      this.receiver_.$.close();
    }

    // Zeroes shared buffer.
    resetBuffer() {
      this.buffer_.fill(0);
    }

    // Sets fake data that is used to deliver sensor reading updates.
    async setSensorReading(readingData) {
      this.readingData_ = new RingBuffer(readingData);
      return this;
    }

    // Sets flag that forces sensor to fail when addConfiguration is invoked.
    setStartShouldFail(shouldFail) {
      this.startShouldFail_ = shouldFail;
    }

    // Returns resolved promise if suspend() was called, rejected otherwise.
    suspendCalled() {
      return new Promise(resolve => {
        this.suspendCalled_ = resolve;
      });
    }

    // Returns resolved promise if resume() was called, rejected otherwise.
    resumeCalled() {
      return new Promise(resolve => {
        this.resumeCalled_ = resolve;
      });
    }

    // Resolves promise when addConfiguration() is called.
    addConfigurationCalled() {
      return new Promise(resolve => {
        this.addConfigurationCalled_ = resolve;
      });
    }

    // Resolves promise when removeConfiguration() is called.
    removeConfigurationCalled() {
      return new Promise(resolve => {
        this.removeConfigurationCalled_ = resolve;
      });
    }

    startReading() {
      if (this.readingData_ != null) {
        this.stopReading();
        const maxFrequencyUsed = this.requestedFrequencies_[0];
        const timeout = (1 / maxFrequencyUsed) * 1000;
        this.sensorReadingTimerId_ = window.setInterval(() => {
          if (this.readingData_) {
            // |buffer_| is a TypedArray, so we need to make sure we pass an
            // array to set().
            const reading = this.readingData_.next().value;
            assert_true(Array.isArray(reading), "The readings passed to " +
                "setSensorReading() must arrays.");
            this.buffer_.set(reading, 2);

            // For all tests sensor reading should have monotonically
            // increasing timestamp in seconds.
            this.buffer_[1] = window.performance.now() * 0.001;
          }
          if (this.reportingMode_ === ReportingMode.ON_CHANGE &&
              this.notifyOnReadingChange_) {
            this.client_.sensorReadingChanged();
          }
        }, timeout);
      }
    }

    stopReading() {
      if (this.sensorReadingTimerId_ != null) {
        window.clearInterval(this.sensorReadingTimerId_);
        this.sensorReadingTimerId_ = null;
      }
    }

    getSamplingFrequency() {
       assert_true(this.requestedFrequencies_.length > 0);
       return this.requestedFrequencies_[0];
    }
  }

  // This class aggregates information about a given sensor type that is used by
  // MockSensorProvider when it is asked to create a new MockSensor.
  class SensorTypeSettings {
    constructor(mojoSensorType) {
      this.mojoSensorType_ = mojoSensorType;
      assert_greater_than_equal(mojoSensorType, SensorType.MIN_VALUE);
      assert_less_than_equal(mojoSensorType, SensorType.MAX_VALUE);

      this.shouldDenyRequests_ = false;
      this.unavailable_ = false;
    }

    get mojoSensorType() {
      return this.mojoSensorType_;
    }

    get shouldDenyRequests() {
      return this.shouldDenyRequests_;
    }

    set shouldDenyRequests(deny) {
      this.shouldDenyRequests_ = deny;
    }

    get unavailable() {
      return this.unavailable_;
    }

    set unavailable(is_unavailable) {
      this.unavailable_ = is_unavailable;
    }
  }

  // Maps a given device.mojom.SensorType enum value to a suitable name as a
  // string.
  function getSensorTypeName(mojoSensorType) {
    switch (mojoSensorType) {
      case SensorType.ACCELEROMETER:
        return 'Accelerometer';
      case SensorType.LINEAR_ACCELERATION:
        return 'LinearAccelerationSensor';
      case SensorType.AMBIENT_LIGHT:
        return 'AmbientLightSensor';
      case SensorType.GRAVITY:
        return 'GravitySensor';
      case SensorType.GYROSCOPE:
        return 'Gyroscope';
      case SensorType.MAGNETOMETER:
        return 'Magnetometer';
      case SensorType.ABSOLUTE_ORIENTATION_QUATERNION:
        return 'AbsoluteOrientationSensor';
      case SensorType.ABSOLUTE_ORIENTATION_EULER_ANGLES:
        return 'AbsoluteOrientationEulerAngles';
      case SensorType.RELATIVE_ORIENTATION_QUATERNION:
        return 'RelativeOrientationSensor';
      case SensorType.RELATIVE_ORIENTATION_EULER_ANGLES:
        return 'RelativeOrientationEulerAngles';
    }
  }

  // Class that mocks SensorProvider interface defined in
  // sensor_provider.mojom
  class MockSensorProvider {
    constructor() {
      this.readingSizeInBytes_ =
          Number(SensorInitParams_READ_BUFFER_SIZE_FOR_TESTS);
      this.sharedBufferSizeInBytes_ =
          this.readingSizeInBytes_ * (SensorType.MAX_VALUE + 1);
      let rv = Mojo.createSharedBuffer(this.sharedBufferSizeInBytes_);
      assert_equals(
          rv.result, Mojo.RESULT_OK, 'Failed to create shared buffer');
      const handle = rv.handle;
      rv = handle.mapBuffer(0, this.sharedBufferSizeInBytes_);
      assert_equals(rv.result, Mojo.RESULT_OK, 'Failed to map shared buffer');
      this.shmemArrayBuffer_ = rv.buffer;
      rv = handle.duplicateBufferHandle({readOnly: true});
      assert_equals(
          rv.result, Mojo.RESULT_OK, 'Failed to duplicate shared buffer');
      this.readOnlySharedBufferHandle_ = rv.handle;
      this.activeSensors_ = new Map();
      this.resolveFuncs_ = new Map();
      this.isContinuous_ = false;
      this.maxFrequency_ = 60;
      this.minFrequency_ = 1;
      this.resetSensorTypeSettings();
      this.receiver_ = new SensorProviderReceiver(this);
      this.interceptor_ =
          new MojoInterfaceInterceptor(SensorProvider.$interfaceName);
      this.interceptor_.oninterfacerequest =
          e => this.receiver_.$.bindHandle(e.handle);
      this.interceptor_.start();
    }

    // device.mojom.SensorProvider implementation
    // Mojo functions that return a value must be async and return an object
    // whose keys match the names declared in Mojo.

    // GetSensor(SensorType type) => (SensorCreationResult result,
    //                                SensorInitParams? init_params)
    // Returns initialized Sensor proxy to the client.
    async getSensor(mojoSensorType) {
      const sensorSettings = this.sensorTypeSettings_.get(getSensorTypeName(mojoSensorType));
      if (sensorSettings.unavailable) {
        return {result: SensorCreationResult.ERROR_NOT_AVAILABLE,
                initParams: null};
      }
      if (sensorSettings.shouldDenyRequests) {
        return {result: SensorCreationResult.ERROR_NOT_ALLOWED,
                initParams: null};
      }

      const offset = mojoSensorType * this.readingSizeInBytes_;
      const reportingMode = this.isContinuous_ ?
          ReportingMode.CONTINUOUS : ReportingMode.ON_CHANGE;

      const sensorPtr = new SensorRemote();
      if (!this.activeSensors_.has(mojoSensorType)) {
        const shmemView = new Float64Array(
            this.shmemArrayBuffer_, offset,
            this.readingSizeInBytes_ / Float64Array.BYTES_PER_ELEMENT);
        const mockSensor = new MockSensor(
            sensorPtr.$.bindNewPipeAndPassReceiver(), shmemView, reportingMode);
        this.activeSensors_.set(mojoSensorType, mockSensor);
        this.activeSensors_.get(mojoSensorType).client_ = new SensorClientRemote();
      }

      const rv = this.readOnlySharedBufferHandle_.duplicateBufferHandle(
          {readOnly: true});

      assert_equals(rv.result, Mojo.RESULT_OK);

      const defaultConfig = { frequency: DEFAULT_FREQUENCY };
      // Consider sensor traits to meet assertions in C++ code (see
      // services/device/public/cpp/generic_sensor/sensor_traits.h)
      if (mojoSensorType == SensorType.AMBIENT_LIGHT ||
          mojoSensorType == SensorType.MAGNETOMETER) {
        this.maxFrequency_ = Math.min(10, this.maxFrequency_);
      }

      const client = this.activeSensors_.get(mojoSensorType).client_;
      const initParams = {
        sensor: sensorPtr,
        clientReceiver: client.$.bindNewPipeAndPassReceiver(),
        memory: {buffer: rv.handle},
        bufferOffset: BigInt(offset),
        mode: reportingMode,
        defaultConfiguration: defaultConfig,
        minimumFrequency: this.minFrequency_,
        maximumFrequency: this.maxFrequency_
      };

      if (this.resolveFuncs_.has(mojoSensorType)) {
        for (let resolveFunc of this.resolveFuncs_.get(mojoSensorType)) {
          resolveFunc(this.activeSensors_.get(mojoSensorType));
        }
        this.resolveFuncs_.delete(mojoSensorType);
      }

      return {result: SensorCreationResult.SUCCESS, initParams};
    }

    // Mock functions

    // Returns a SensorTypeSettings instance corresponding to the name |type|, a
    // string.
    getSensorTypeSettings(type) {
      return this.sensorTypeSettings_.get(type);
    }

    // Recreates |this.sensorTypeSettings_| with a new map and values reset to
    // their defaults.
    resetSensorTypeSettings() {
      this.sensorTypeSettings_ = new Map([
        ['Accelerometer', new SensorTypeSettings(SensorType.ACCELEROMETER)],
        ['LinearAccelerationSensor', new SensorTypeSettings(SensorType.LINEAR_ACCELERATION)],
        ['AmbientLightSensor', new SensorTypeSettings(SensorType.AMBIENT_LIGHT)],
        ['GravitySensor', new SensorTypeSettings(SensorType.GRAVITY)],
        ['Gyroscope', new SensorTypeSettings(SensorType.GYROSCOPE)],
        ['Magnetometer', new SensorTypeSettings(SensorType.MAGNETOMETER)],
        ['AbsoluteOrientationSensor', new SensorTypeSettings(SensorType.ABSOLUTE_ORIENTATION_QUATERNION)],
        ['AbsoluteOrientationEulerAngles', new SensorTypeSettings(SensorType.ABSOLUTE_ORIENTATION_EULER_ANGLES)],
        ['RelativeOrientationSensor', new SensorTypeSettings(SensorType.RELATIVE_ORIENTATION_QUATERNION)],
        ['RelativeOrientationEulerAngles', new SensorTypeSettings(SensorType.RELATIVE_ORIENTATION_EULER_ANGLES)]
      ]);
    }

    // Resets state of mock SensorProvider between test runs.
    reset() {
      for (const sensor of this.activeSensors_.values()) {
        sensor.reset();
      }
      this.activeSensors_.clear();
      this.resolveFuncs_.clear();
      this.resetSensorTypeSettings();
      this.maxFrequency_ = 60;
      this.minFrequency_ = 1;
      this.isContinuous_ = false;
      this.receiver_.$.close();
      this.interceptor_.stop();
    }

    // Returns mock sensor that was created in getSensor to the layout test.
    getCreatedSensor(sensorName) {
      const type = this.sensorTypeSettings_.get(sensorName).mojoSensorType;

      if (this.activeSensors_.has(type)) {
        return Promise.resolve(this.activeSensors_.get(type));
      }

      return new Promise(resolve => {
        if (!this.resolveFuncs_.has(type)) {
          this.resolveFuncs_.set(type, []);
        }
        this.resolveFuncs_.get(type).push(resolve);
      });
    }

    // Forces sensor to use |reportingMode| as an update mode.
    setContinuousReportingMode() {
      this.isContinuous_ = true;
    }

    // Sets the maximum frequency for a concrete sensor.
    setMaximumSupportedFrequency(frequency) {
      this.maxFrequency_ = frequency;
    }

    // Sets the minimum frequency for a concrete sensor.
    setMinimumSupportedFrequency(frequency) {
      this.minFrequency_ = frequency;
    }
  }

  return new MockSensorProvider();
}

export function sensor_test(func, name, properties) {
  promise_test(async t => {
    const sensorProvider = sensorMocks();

    // Clean up and reset mock sensor stubs asynchronously, so that the blink
    // side closes its proxies and notifies JS sensor objects before new test is
    // started.
    try {
      await func(t, sensorProvider);
    } finally {
      sensorProvider.reset();
      await new Promise(resolve => { setTimeout(resolve, 0); });
    };
  }, name, properties);
}

export async function setMockSensorDataForType(sensorProvider, sensorType, mockDataArray) {
  const createdSensor = await sensorProvider.getCreatedSensor(sensorType);
  return createdSensor.setSensorReading([mockDataArray]);
}

// Returns a promise that will be resolved when an event equal to the given
// event is fired.
export function waitForEvent(expectedEvent, targetWindow = window) {
  const stringify = (thing, targetWindow) => {
    if (thing instanceof targetWindow.Object && thing.constructor !== targetWindow.Object) {
      let str = '{';
      for (let key of Object.keys(Object.getPrototypeOf(thing))) {
        str += JSON.stringify(key) + ': ' + stringify(thing[key], targetWindow) + ', ';
      }
      return str + '}';
    } else if (thing instanceof Number) {
      return thing.toFixed(6);
    }
    return JSON.stringify(thing);
  };

  return new Promise((resolve, reject) => {
    let events = [];
    let timeoutId = null;

    const expectedEventString = stringify(expectedEvent, window);
    function listener(event) {
      const eventString = stringify(event, targetWindow);
      if (eventString === expectedEventString) {
        targetWindow.clearTimeout(timeoutId);
        targetWindow.removeEventListener(expectedEvent.type, listener);
        resolve();
      } else {
        events.push(eventString);
      }
    }
    targetWindow.addEventListener(expectedEvent.type, listener);

    timeoutId = targetWindow.setTimeout(() => {
      targetWindow.removeEventListener(expectedEvent.type, listener);
      let errorMessage = 'Timeout waiting for expected event: ' + expectedEventString;
      if (events.length == 0) {
        errorMessage += ', no events were fired';
      } else {
        errorMessage += ', received events: '
        for (let event of events) {
          errorMessage += event + ', ';
        }
      }
      reject(errorMessage);
    }, 500);
  });
}
