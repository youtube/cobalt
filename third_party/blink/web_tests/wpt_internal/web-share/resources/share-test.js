import {ShareService, ShareServiceReceiver} from '/gen/third_party/blink/public/mojom/webshare/webshare.mojom.m.js';

class MockShareService {
  constructor() {
    this.receiver_ = new ShareServiceReceiver(this);
    this.interceptor_ =
        new MojoInterfaceInterceptor(ShareService.$interfaceName);
    this.interceptor_.oninterfacerequest =
        e => this.receiver_.$.bindHandle(e.handle);
    this.interceptor_.start();
  }

  // Returns a Promise that gets rejected if the test should fail.
  init_() {
    // sequence of [expectedTitle, expectedText, result].
    this.shareResultQueue_ = [];

    return new Promise((resolve, reject) => {this.reject_ = reject});
  }

  share(title, text, url) {
    let callback = null;
    let result = new Promise(resolve => {callback = resolve;});

    if (!this.shareResultQueue_.length) {
      this.reject_('Unexpected call to mojo share method');
      return result;
    }

    let [expectedTitle, expectedText, expectedUrl, error] =
        this.shareResultQueue_.shift();
    try {
      assert_equals(title, expectedTitle);
      assert_equals(text, expectedText);
      assert_equals(url.url, expectedUrl);
    } catch (e) {
      this.reject_(e);
      return result;
    }
    callback({error: error});

    return result;
  }

  pushShareResult(expectedTitle, expectedText, expectedUrl, result) {
    this.shareResultQueue_.push(
        [expectedTitle, expectedText, expectedUrl, result]);
  }
}

let mockShareService = new MockShareService();

export function share_test(func, name, properties) {
  promise_test(() => {
    let mockPromise = mockShareService.init_();
    return Promise.race([func(mockShareService), mockPromise]);
  }, name, properties);
}

// Copied from resources/bluetooth/bluetooth-helpers.js.
export function callWithKeyDown(functionCalledOnKeyPress) {
  return new Promise((resolve, reject) => {
    function onKeyPress() {
      document.removeEventListener('keypress', onKeyPress, false);
      try {
        resolve(functionCalledOnKeyPress());
      } catch (e) {
        reject(e);
      }
    }
    document.addEventListener('keypress', onKeyPress, false);

    eventSender.keyDown(' ', []);
  });
}
