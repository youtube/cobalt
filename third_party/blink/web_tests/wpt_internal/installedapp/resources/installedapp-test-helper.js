import {InstalledAppProvider, InstalledAppProviderReceiver} from '/gen/third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.m.js';

function assert_relatedapplication_equals(actual, expected, description) {
  assert_equals(actual.platform, expected.platform, description);
  assert_equals(actual.url, expected.url, description);
  assert_equals(actual.id, expected.id, description);
}

export function assert_array_relatedapplication_equals(
    actual, expected, description) {
  assert_equals(actual.length, expected.length, description);

  for (let i = 0; i < actual.length; i++)
    assert_relatedapplication_equals(actual[i], expected[i], description);
}

class MockInstalledAppProvider {
  constructor(interfaceProvider) {
    this.receiver_ = new InstalledAppProviderReceiver(this);

    this.interceptor_ =
        new MojoInterfaceInterceptor(InstalledAppProvider.$interfaceName);
    this.interceptor_.oninterfacerequest =
        e => this.receiver_.$.bindHandle(e.handle);
    this.interceptor_.start();
  }

  // Returns a Promise that gets rejected if the test should fail.
  init_() {
    // sequence of [expectedRelatedApps, installedApps].
    this.callQueue_ = [];

    return new Promise((resolve, reject) => { this.reject_ = reject });
  }

  async filterInstalledApps(relatedApps) {
    if (!this.callQueue_.length) {
      this.reject_('Unexpected call to mojo FilterInstalledApps method');
      return;
    }

    let [expectedRelatedApps, installedApps] = this.callQueue_.shift();
    try {
      assert_array_relatedapplication_equals(relatedApps, expectedRelatedApps);
    } catch (e) {
      this.reject_(e);
      return;
    }

    return { installedApps: installedApps };
  }

  pushExpectedCall(expectedRelatedApps, installedApps) {
    this.callQueue_.push([expectedRelatedApps, installedApps]);
  }
}

let mockInstalledAppProvider = new MockInstalledAppProvider();

// Creates a test case that uses a mock InstalledAppProvider.
// |func| is a function that takes (t, mock), where |mock| is a
// MockInstalledAppProvider that can have expectations set with
// pushExpectedCall. It should return a promise, the result of
// getInstalledRelatedApps().
// |name| and |properties| are standard testharness arguments.
export function installedapp_test(func, name, properties) {
  promise_test(t => {
    let mockPromise = mockInstalledAppProvider.init_();
    return Promise.race([func(t, mockInstalledAppProvider), mockPromise]);
  }, name, properties);
}
