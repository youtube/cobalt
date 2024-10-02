// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.webkit.JavascriptInterface;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameterBefore;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.JavaBridgeActivityTestRule.Controller;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.ContentJUnit4RunnerDelegate;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.ref.WeakReference;
import java.util.concurrent.CountDownLatch;

/**
 * Part of the test suite for the Java Bridge. Tests a number of features including ...
 * - The type of injected objects
 * - The type of their methods
 * - Replacing objects
 * - Removing objects
 * - Access control
 * - Calling methods on returned objects
 * - Multiply injected objects
 * - Threading
 * - Inheritance
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ContentJUnit4RunnerDelegate.class)
@Batch(JavaBridgeActivityTestRule.BATCH)
public class JavaBridgeBasicsTest {
    @Rule
    public JavaBridgeActivityTestRule mActivityTestRule = new JavaBridgeActivityTestRule();

    private static class TestController extends Controller {
        private int mIntValue;
        private long mLongValue;
        private String mStringValue;
        private boolean mBooleanValue;

        @JavascriptInterface
        public synchronized void setIntValue(int x) {
            mIntValue = x;
            notifyResultIsReady();
        }
        @JavascriptInterface
        public synchronized void setLongValue(long x) {
            mLongValue = x;
            notifyResultIsReady();
        }
        @JavascriptInterface
        public synchronized void setStringValue(String x) {
            mStringValue = x;
            notifyResultIsReady();
        }
        @JavascriptInterface
        public synchronized void setBooleanValue(boolean x) {
            mBooleanValue = x;
            notifyResultIsReady();
        }

        public synchronized int waitForIntValue() {
            waitForResult();
            return mIntValue;
        }
        public synchronized long waitForLongValue() {
            waitForResult();
            return mLongValue;
        }
        public synchronized String waitForStringValue() {
            waitForResult();
            return mStringValue;
        }
        public synchronized boolean waitForBooleanValue() {
            waitForResult();
            return mBooleanValue;
        }

        public synchronized String getStringValue() {
            return mStringValue;
        }
    }

    private static class ObjectWithStaticMethod {
        @JavascriptInterface
        public static String staticMethod() {
            return "foo";
        }
    }

    @UseMethodParameterBefore(JavaBridgeActivityTestRule.MojoTestParams.class)
    public void setupMojoTest(boolean useMojo) {
        mActivityTestRule.setupMojoTest(useMojo);
    }

    TestController mTestController;

    @Before
    public void setUp() {
        mTestController = new TestController();
        mActivityTestRule.injectObjectAndReload(mTestController, "testController");
    }

    // Note that this requires that we can pass a JavaScript string to Java.
    protected String executeJavaScriptAndGetStringResult(String script) throws Throwable {
        mActivityTestRule.executeJavaScript("testController.setStringValue(" + script + ");");
        return mTestController.waitForStringValue();
    }

    // Note that this requires that we can pass a JavaScript boolean to Java.
    private void executeAndSetIfException(String script) throws Throwable {
        mActivityTestRule.executeJavaScript("try {" + script + ";"
                + "  testController.setBooleanValue(false);"
                + "} catch (exception) {"
                + "  testController.setBooleanValue(true);"
                + "}");
    }

    private void assertRaisesException(String script) throws Throwable {
        executeAndSetIfException(script);
        Assert.assertTrue(mTestController.waitForBooleanValue());
    }

    private void assertNoRaisedException(String script) throws Throwable {
        executeAndSetIfException(script);
        Assert.assertFalse(mTestController.waitForBooleanValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testTypeOfInjectedObject(boolean useMojo) throws Throwable {
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testAdditionNotReflectedUntilReload(boolean useMojo) throws Throwable {
        Assert.assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject"));
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getJavascriptInjector(useMojo).addPossiblyUnsafeInterface(
                        new Object(), "testObject", null);
            }
        });
        Assert.assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject"));
        mActivityTestRule.synchronousPageReload();
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testReplaceWithoutReloading(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            @JavascriptInterface
            public void method() {
                mTestController.setStringValue("object 1");
            }
        }, "testObject");
        mActivityTestRule.executeJavaScript("testObject.method()");
        Assert.assertEquals("object 1", mTestController.waitForStringValue());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getJavascriptInjector(useMojo).addPossiblyUnsafeInterface(
                        new Object() {
                            @JavascriptInterface
                            public void method() {
                                mTestController.setStringValue("object 2");
                            }
                        },
                        "testObject", null);
            }
        });
        mActivityTestRule.executeJavaScript("testObject.method()");
        // should still return object 1 as the page hasn't reloaded
        Assert.assertEquals("object 1", mTestController.waitForStringValue());
        mActivityTestRule.synchronousPageReload();
        mActivityTestRule.executeJavaScript("testObject.method()");
        Assert.assertEquals("object 2", mTestController.waitForStringValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testRemovalNotReflectedUntilReload(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            @JavascriptInterface
            public void method() {
                mTestController.setStringValue("I'm here");
            }
        }, "testObject");
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
        mActivityTestRule.executeJavaScript("testObject.method()");
        Assert.assertEquals("I'm here", mTestController.waitForStringValue());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getJavascriptInjector(useMojo).removeInterface("testObject");
            }
        });
        // Check that the Java object is being held by the Java bridge, thus it's not
        // collected. Note that despite that what JavaDoc says about invoking "gc()", both Dalvik
        // and ART actually run the collector if called via Runtime.
        Runtime.getRuntime().gc();
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
        mActivityTestRule.executeJavaScript("testObject.method()");
        Assert.assertEquals("I'm here", mTestController.waitForStringValue());
        mActivityTestRule.synchronousPageReload();
        Assert.assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testRemoveObjectNotAdded(boolean useMojo) throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mActivityTestRule.getTestCallBackHelperContainer().getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getJavascriptInjector(useMojo).removeInterface("foo");
                mActivityTestRule.getWebContents().getNavigationController().reload(true);
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount);
        Assert.assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof foo"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testTypeOfMethod(boolean useMojo) throws Throwable {
        Assert.assertEquals("function",
                executeJavaScriptAndGetStringResult("typeof testController.setStringValue"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testTypeOfInvalidMethod(boolean useMojo) throws Throwable {
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testController.foo"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testCallingInvalidMethodRaisesException(boolean useMojo) throws Throwable {
        assertRaisesException("testController.foo()");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testUncaughtJavaExceptionRaisesJavaScriptException(boolean useMojo)
            throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            @JavascriptInterface
            public void method() {
                throw new RuntimeException("foo");
            }
        }, "testObject");
        assertRaisesException("testObject.method()");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testCallingAsConstructorRaisesException(boolean useMojo) throws Throwable {
        assertRaisesException("new testController.setStringValue('foo')");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testCallingOnNonInjectedObjectRaisesException(boolean useMojo) throws Throwable {
        assertRaisesException("testController.setStringValue.call({}, 'foo')");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testCallingOnInstanceOfOtherClassRaisesException(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object(), "testObject");
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
        Assert.assertEquals("function",
                executeJavaScriptAndGetStringResult("typeof testController.setStringValue"));
        assertRaisesException("testController.setStringValue.call(testObject, 'foo')");
    }

    // Note that this requires that we can pass a JavaScript string to Java.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testTypeOfStaticMethod(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new ObjectWithStaticMethod(), "testObject");
        mActivityTestRule.executeJavaScript(
                "testController.setStringValue(typeof testObject.staticMethod)");
        Assert.assertEquals("function", mTestController.waitForStringValue());
    }

    // Note that this requires that we can pass a JavaScript string to Java.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testCallStaticMethod(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new ObjectWithStaticMethod(), "testObject");
        mActivityTestRule.executeJavaScript(
                "testController.setStringValue(testObject.staticMethod())");
        Assert.assertEquals("foo", mTestController.waitForStringValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testPrivateMethodNotExposed(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            private void method() {}
            protected void method2() {}
            @JavascriptInterface
            private void method3() {}
            @JavascriptInterface
            protected void method4() {}
        }, "testObject");
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.method"));
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.method2"));
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.method3"));
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.method4"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testReplaceInjectedObject(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            @JavascriptInterface
            public void method() {
                mTestController.setStringValue("object 1");
            }
        }, "testObject");
        mActivityTestRule.executeJavaScript("testObject.method()");
        Assert.assertEquals("object 1", mTestController.waitForStringValue());

        mActivityTestRule.injectObjectAndReload(new Object() {
            @JavascriptInterface
            public void method() {
                mTestController.setStringValue("object 2");
            }
        }, "testObject");
        mActivityTestRule.executeJavaScript("testObject.method()");
        Assert.assertEquals("object 2", mTestController.waitForStringValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testInjectNullObjectIsIgnored(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(null, "testObject");
        Assert.assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testReplaceInjectedObjectWithNullObjectIsIgnored(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object(), "testObject");
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
        mActivityTestRule.injectObjectAndReload(null, "testObject");
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testCallOverloadedMethodWithDifferentNumberOfArguments(boolean useMojo)
            throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            @JavascriptInterface
            public void method() {
                mTestController.setStringValue("0 args");
            }

            @JavascriptInterface
            public void method(int x) {
                mTestController.setStringValue("1 arg");
            }

            @JavascriptInterface
            public void method(int x, int y) {
                mTestController.setStringValue("2 args");
            }
        }, "testObject");
        mActivityTestRule.executeJavaScript("testObject.method()");
        Assert.assertEquals("0 args", mTestController.waitForStringValue());
        mActivityTestRule.executeJavaScript("testObject.method(42)");
        Assert.assertEquals("1 arg", mTestController.waitForStringValue());
        mActivityTestRule.executeJavaScript("testObject.method(null)");
        Assert.assertEquals("1 arg", mTestController.waitForStringValue());
        mActivityTestRule.executeJavaScript("testObject.method(undefined)");
        Assert.assertEquals("1 arg", mTestController.waitForStringValue());
        mActivityTestRule.executeJavaScript("testObject.method(42, 42)");
        Assert.assertEquals("2 args", mTestController.waitForStringValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testCallMethodWithWrongNumberOfArgumentsRaisesException(boolean useMojo)
            throws Throwable {
        assertRaisesException("testController.setIntValue()");
        assertRaisesException("testController.setIntValue(42, 42)");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testObjectPersistsAcrossPageLoads(boolean useMojo) throws Throwable {
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
        mActivityTestRule.synchronousPageReload();
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testCustomPropertiesCleanedUpOnPageReloads(boolean useMojo) throws Throwable {
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
        mActivityTestRule.executeJavaScript("testController.myProperty = 42;");
        Assert.assertEquals("42", executeJavaScriptAndGetStringResult("testController.myProperty"));
        mActivityTestRule.synchronousPageReload();
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("testController.myProperty"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testSameObjectInjectedMultipleTimes(boolean useMojo) throws Throwable {
        class TestObject {
            private int mNumMethodInvocations;

            public void method() {
                mTestController.setIntValue(++mNumMethodInvocations);
            }
        }
        final TestObject testObject = new TestObject();
        mActivityTestRule.injectObjectsAndReload(
                testObject, "testObject1", testObject, "testObject2", null);
        mActivityTestRule.executeJavaScript("testObject1.method()");
        Assert.assertEquals(1, mTestController.waitForIntValue());
        mActivityTestRule.executeJavaScript("testObject2.method()");
        Assert.assertEquals(2, mTestController.waitForIntValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testCallMethodOnReturnedObject(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            @JavascriptInterface
            public Object getInnerObject() {
                return new Object() {
                    @JavascriptInterface
                    public void method(int x) {
                        mTestController.setIntValue(x);
                    }
                };
            }
        }, "testObject");
        mActivityTestRule.executeJavaScript("testObject.getInnerObject().method(42)");
        Assert.assertEquals(42, mTestController.waitForIntValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testReturnedObjectInjectedElsewhere(boolean useMojo) throws Throwable {
        class InnerObject {
            private int mNumMethodInvocations;

            @JavascriptInterface
            public void method() {
                mTestController.setIntValue(++mNumMethodInvocations);
            }
        }
        final InnerObject innerObject = new InnerObject();
        final Object object = new Object() {
            @JavascriptInterface
            public InnerObject getInnerObject() {
                return innerObject;
            }
        };
        mActivityTestRule.injectObjectsAndReload(
                object, "testObject", innerObject, "innerObject", null);
        mActivityTestRule.executeJavaScript("testObject.getInnerObject().method()");
        Assert.assertEquals(1, mTestController.waitForIntValue());
        mActivityTestRule.executeJavaScript("innerObject.method()");
        Assert.assertEquals(2, mTestController.waitForIntValue());
    }

    // Verify that Java objects returned from bridge object methods are dereferenced
    // on the Java side once they have been fully dereferenced on the JS side.
    // Failing this test would mean that methods returning objects effectively create a memory
    // leak.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @CommandLineFlags.Add("js-flags=--expose-gc")
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testReturnedObjectIsGarbageCollected(boolean useMojo) throws Throwable {
        Assert.assertEquals("function", executeJavaScriptAndGetStringResult("typeof gc"));
        class InnerObject {
        }
        class TestObject {
            @JavascriptInterface
            public InnerObject getInnerObject() {
                InnerObject inner = new InnerObject();
                mWeakRefForInner = new WeakReference<InnerObject>(inner);
                return inner;
            }
            // A weak reference is used to check InnerObject instance reachability.
            WeakReference<InnerObject> mWeakRefForInner;
        }
        TestObject object = new TestObject();
        mActivityTestRule.injectObjectAndReload(object, "testObject");
        // Initially, store a reference to the inner object in JS to make sure it's not
        // garbage-collected prematurely.
        Assert.assertEquals("object",
                executeJavaScriptAndGetStringResult("(function() { "
                        + "globalInner = testObject.getInnerObject(); return typeof globalInner; "
                        + "})()"));
        Assert.assertTrue(object.mWeakRefForInner.get() != null);
        // Check that returned Java object is being held by the Java bridge, thus it's not
        // collected.  Note that despite that what JavaDoc says about invoking "gc()", both Dalvik
        // and ART actually run the collector.
        Runtime.getRuntime().gc();
        Assert.assertTrue(object.mWeakRefForInner.get() != null);
        // Now dereference the inner object in JS and run GC to collect the interface object.
        Assert.assertEquals("true",
                executeJavaScriptAndGetStringResult("(function() { "
                        + "delete globalInner; gc(); return (typeof globalInner == 'undefined'); "
                        + "})()"));
        // Force GC on the Java side again. The bridge had to release the inner object, so it must
        // be collected this time.
        Runtime.getRuntime().gc();
        Assert.assertEquals(null, object.mWeakRefForInner.get());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testSameReturnedObjectUsesSameWrapper(boolean useMojo) throws Throwable {
        class InnerObject {
        }
        final InnerObject innerObject = new InnerObject();
        final Object injectedTestObject = new Object() {
            @JavascriptInterface
            public InnerObject getInnerObject() {
                return innerObject;
            }
        };
        mActivityTestRule.injectObjectAndReload(injectedTestObject, "injectedTestObject");
        mActivityTestRule.executeJavaScript("inner1 = injectedTestObject.getInnerObject()");
        mActivityTestRule.executeJavaScript("inner2 = injectedTestObject.getInnerObject()");
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof inner1"));
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof inner2"));
        Assert.assertEquals("true", executeJavaScriptAndGetStringResult("inner1 === inner2"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @CommandLineFlags.Add("js-flags=--expose-gc")
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testSameWrapperObjectsAreGarbageCollected(boolean useMojo) throws Throwable {
        class InnerObject {}
        class TestObject {
            @JavascriptInterface
            public InnerObject getInnerObject() {
                if (mWeakForInnerObject == null) {
                    InnerObject innerObject = new InnerObject();
                    mWeakForInnerObject = new WeakReference<InnerObject>(innerObject);
                    return innerObject;
                }
                return mWeakForInnerObject.get();
            }
            // A weak reference is used to check InnerObject instance reachability.
            WeakReference<InnerObject> mWeakForInnerObject;
        };
        final TestObject injectedTestObject = new TestObject();

        mActivityTestRule.injectObjectAndReload(injectedTestObject, "injectedTestObject");
        mActivityTestRule.executeJavaScript("inner1 = injectedTestObject.getInnerObject()");
        mActivityTestRule.executeJavaScript("inner2 = injectedTestObject.getInnerObject()");
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof inner1"));
        Assert.assertEquals("object", executeJavaScriptAndGetStringResult("typeof inner2"));

        Assert.assertTrue(injectedTestObject.mWeakForInnerObject.get() != null);
        Runtime.getRuntime().gc();
        Assert.assertTrue(injectedTestObject.mWeakForInnerObject.get() != null);

        // Now dereference the inner object in JS and run GC to collect the interface object.
        Assert.assertEquals("true",
                executeJavaScriptAndGetStringResult("(function() { "
                        + "delete inner1; delete inner2; gc(); "
                        + "return (typeof inner1 == 'undefined'); })()"));
        // Force GC on the Java side again. The bridge had to release the inner object, so it must
        // be collected this time.
        Runtime.getRuntime().gc();
        Assert.assertEquals(null, injectedTestObject.mWeakForInnerObject.get());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testMethodInvokedOnBackgroundThread(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            @JavascriptInterface
            public void captureThreadId() {
                mTestController.setLongValue(Thread.currentThread().getId());
            }
        }, "testObject");
        mActivityTestRule.executeJavaScript("testObject.captureThreadId()");
        final long threadId = mTestController.waitForLongValue();
        Assert.assertFalse(threadId == Thread.currentThread().getId());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                Assert.assertFalse(threadId == Thread.currentThread().getId());
            }
        });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testBlockingUiThreadDoesNotBlockCallsFromJs(boolean useMojo) {
        class TestObject {
            private CountDownLatch mLatch;
            public TestObject() {
                mLatch = new CountDownLatch(1);
            }
            public boolean waitOnTheLatch() throws Exception {
                return mLatch.await(scaleTimeout(10000),
                        java.util.concurrent.TimeUnit.MILLISECONDS);
            }
            @JavascriptInterface
            public void unlockTheLatch() {
                mTestController.setStringValue("unlocked");
                mLatch.countDown();
            }
        }
        final TestObject testObject = new TestObject();
        mActivityTestRule.injectObjectAndReload(testObject, "testObject");
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // loadUrl is asynchronous, the JS code will start running on the renderer
                // thread. As soon as we exit loadUrl, the browser UI thread will be stuck waiting
                // on the latch. If blocking the browser thread blocks Java Bridge, then the call
                // to "unlockTheLatch()" will be executed after the waiting timeout, thus the
                // string value will not yet be updated by the injected object.
                mTestController.setStringValue("locked");
                mActivityTestRule.getWebContents().getNavigationController().loadUrl(
                        new LoadUrlParams(
                                "javascript:(function() { testObject.unlockTheLatch() })()"));
                try {
                    Assert.assertTrue(testObject.waitOnTheLatch());
                } catch (Exception e) {
                    android.util.Log.e("JavaBridgeBasicsTest", "Wait exception", e);
                    Assert.fail("Wait exception");
                }
                Assert.assertEquals("unlocked", mTestController.getStringValue());
            }
        });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testPublicInheritedMethod(boolean useMojo) throws Throwable {
        class Base {
            @JavascriptInterface
            public void method(int x) {
                mTestController.setIntValue(x);
            }
        }
        class Derived extends Base {
        }
        mActivityTestRule.injectObjectAndReload(new Derived(), "testObject");
        Assert.assertEquals(
                "function", executeJavaScriptAndGetStringResult("typeof testObject.method"));
        mActivityTestRule.executeJavaScript("testObject.method(42)");
        Assert.assertEquals(42, mTestController.waitForIntValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testPrivateInheritedMethod(boolean useMojo) throws Throwable {
        class Base {
            @JavascriptInterface
            private void method() {}
        }
        class Derived extends Base {
        }
        mActivityTestRule.injectObjectAndReload(new Derived(), "testObject");
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.method"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testOverriddenMethod(boolean useMojo) throws Throwable {
        class Base {
            @JavascriptInterface
            public void method() {
                mTestController.setStringValue("base");
            }
        }
        class Derived extends Base {
            @Override
            @JavascriptInterface
            public void method() {
                mTestController.setStringValue("derived");
            }
        }
        mActivityTestRule.injectObjectAndReload(new Derived(), "testObject");
        mActivityTestRule.executeJavaScript("testObject.method()");
        Assert.assertEquals("derived", mTestController.waitForStringValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testEnumerateMembers(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            public void method() {}
            private void privateMethod() {}
            public int field;
            private int mPrivateField;
        }, "testObject", null);
        mActivityTestRule.executeJavaScript("var result = \"\"; "
                + "for (x in testObject) { result += \" \" + x } "
                + "testController.setStringValue(result);");
        Assert.assertEquals(" equals getClass hashCode method notify notifyAll toString wait",
                mTestController.waitForStringValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testReflectPublicMethod(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            public Class<?> myGetClass() {
                return getClass();
            }

            public String method() {
                return "foo";
            }
        }, "testObject", null);
        Assert.assertEquals("foo",
                executeJavaScriptAndGetStringResult(
                        "testObject.myGetClass().getMethod('method', null).invoke(testObject, null)"
                        + ".toString()"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testReflectPublicField(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            public Class<?> myGetClass() {
                return getClass();
            }

            public String field = "foo";
        }, "testObject", null);
        Assert.assertEquals("foo",
                executeJavaScriptAndGetStringResult(
                        "testObject.myGetClass().getField('field').get(testObject).toString()"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testReflectPrivateMethodRaisesException(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            public Class<?> myGetClass() {
                return getClass();
            }

            private void method() {};
        }, "testObject", null);
        assertRaisesException("testObject.myGetClass().getMethod('method', null)");
        // getDeclaredMethod() is able to get a reference to a private method, but actually invoking
        // the method throws.
        assertRaisesException(
                "testObject.myGetClass().getDeclaredMethod('method', null)."
                + "invoke(testObject, null)");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testReflectPrivateFieldRaisesException(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            @JavascriptInterface
            public Class<?> myGetClass() {
                return getClass();
            }

            private int mField;
        }, "testObject", null);
        String fieldName = "mField";
        assertRaisesException("testObject.myGetClass().getField('" + fieldName + "')");
        // getDeclaredField() is able to get a reference to a private field, but actually retrieving
        // the value of the field throws.
        assertNoRaisedException("testObject.myGetClass().getDeclaredField('" + fieldName + "')");
        assertRaisesException(
                "testObject.myGetClass().getDeclaredField('" + fieldName + "').getInt(testObject)");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testAllowNonAnnotatedMethods(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            @JavascriptInterface
            public String allowed() {
                return "foo";
            }
        }, "testObject", null);

        // Test calling a method of an explicitly inherited class (Base#allowed()).
        Assert.assertEquals("foo", executeJavaScriptAndGetStringResult("testObject.allowed()"));

        // Test calling a method of an implicitly inherited class (Object#toString()).
        Assert.assertEquals(
                "string", executeJavaScriptAndGetStringResult("typeof testObject.toString()"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testAllowOnlyAnnotatedMethods(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object() {
            @JavascriptInterface
            public String allowed() {
                return "foo";
            }

            public String disallowed() {
                return "bar";
            }
        }, "testObject", JavascriptInterface.class);

        // getClass() is an Object method and does not have the @JavascriptInterface annotation and
        // should not be able to be called.
        assertRaisesException("testObject.getClass()");
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.getClass"));

        // allowed() is marked with the @JavascriptInterface annotation and should be allowed to be
        // called.
        Assert.assertEquals("foo", executeJavaScriptAndGetStringResult("testObject.allowed()"));

        // disallowed() is not marked with the @JavascriptInterface annotation and should not be
        // able to be called.
        assertRaisesException("testObject.disallowed()");
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.disallowed"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testAnnotationRequirementRetainsPropertyAcrossObjects(boolean useMojo)
            throws Throwable {
        class Test {
            @JavascriptInterface
            public String safe() {
                return "foo";
            }

            public String unsafe() {
                return "bar";
            }
        }

        class TestReturner {
            @JavascriptInterface
            public Test getTest() {
                return new Test();
            }
        }

        // First test with safe mode off.
        mActivityTestRule.injectObjectAndReload(new TestReturner(), "unsafeTestObject", null);

        // safe() should be able to be called regardless of whether or not we are in safe mode.
        Assert.assertEquals(
                "foo", executeJavaScriptAndGetStringResult("unsafeTestObject.getTest().safe()"));
        // unsafe() should be able to be called because we are not in safe mode.
        Assert.assertEquals(
                "bar", executeJavaScriptAndGetStringResult("unsafeTestObject.getTest().unsafe()"));

        // Now test with safe mode on.
        mActivityTestRule.injectObjectAndReload(
                new TestReturner(), "safeTestObject", JavascriptInterface.class);

        // safe() should be able to be called regardless of whether or not we are in safe mode.
        Assert.assertEquals(
                "foo", executeJavaScriptAndGetStringResult("safeTestObject.getTest().safe()"));
        // unsafe() should not be able to be called because we are in safe mode.
        assertRaisesException("safeTestObject.getTest().unsafe()");
        Assert.assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof safeTestObject.getTest().unsafe"));
        // getClass() is an Object method and does not have the @JavascriptInterface annotation and
        // should not be able to be called.
        assertRaisesException("safeTestObject.getTest().getClass()");
        Assert.assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof safeTestObject.getTest().getClass"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testAnnotationDoesNotGetInherited(boolean useMojo) throws Throwable {
        class Base {
            @JavascriptInterface
            public void base() { }
        }

        class Child extends Base {
            @Override
            public void base() { }
        }

        mActivityTestRule.injectObjectAndReload(
                new Child(), "testObject", JavascriptInterface.class);

        // base() is inherited.  The inherited method does not have the @JavascriptInterface
        // annotation and should not be able to be called.
        assertRaisesException("testObject.base()");
        Assert.assertEquals(
                "undefined", executeJavaScriptAndGetStringResult("typeof testObject.base"));
    }

    @SuppressWarnings("javadoc")
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.METHOD})
    @interface TestAnnotation {
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testCustomAnnotationRestriction(boolean useMojo) throws Throwable {
        class Test {
            @TestAnnotation
            public String checkTestAnnotationFoo() {
                return "bar";
            }

            @JavascriptInterface
            public String checkJavascriptInterfaceFoo() {
                return "bar";
            }
        }

        // Inject javascriptInterfaceObj and require the JavascriptInterface annotation.
        mActivityTestRule.injectObjectAndReload(
                new Test(), "javascriptInterfaceObj", JavascriptInterface.class);

        // Test#testAnnotationFoo() should fail, as it isn't annotated with JavascriptInterface.
        assertRaisesException("javascriptInterfaceObj.checkTestAnnotationFoo()");
        Assert.assertEquals("undefined",
                executeJavaScriptAndGetStringResult(
                        "typeof javascriptInterfaceObj.checkTestAnnotationFoo"));

        // Test#javascriptInterfaceFoo() should pass, as it is annotated with JavascriptInterface.
        Assert.assertEquals("bar",
                executeJavaScriptAndGetStringResult(
                        "javascriptInterfaceObj.checkJavascriptInterfaceFoo()"));

        // Inject testAnnotationObj and require the TestAnnotation annotation.
        mActivityTestRule.injectObjectAndReload(
                new Test(), "testAnnotationObj", TestAnnotation.class);

        // Test#testAnnotationFoo() should pass, as it is annotated with TestAnnotation.
        Assert.assertEquals("bar",
                executeJavaScriptAndGetStringResult("testAnnotationObj.checkTestAnnotationFoo()"));

        // Test#javascriptInterfaceFoo() should fail, as it isn't annotated with TestAnnotation.
        assertRaisesException("testAnnotationObj.checkJavascriptInterfaceFoo()");
        Assert.assertEquals("undefined",
                executeJavaScriptAndGetStringResult(
                        "typeof testAnnotationObj.checkJavascriptInterfaceFoo"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testObjectsInspection(boolean useMojo) throws Throwable {
        class Test {
            @JavascriptInterface
            public String m1() {
                return "foo";
            }

            @JavascriptInterface
            public String m2() {
                return "bar";
            }

            @JavascriptInterface
            public String m2(int x) {
                return "bar " + x;
            }
        }

        final String jsObjectKeysTestTemplate = "Object.keys(%s).toString()";
        final String jsForInTestTemplate =
                "(function(){"
                + "  var s=[]; for(var m in %s) s.push(m); return s.join(\",\")"
                + "})()";
        final String inspectableObjectName = "testObj1";
        final String nonInspectableObjectName = "testObj2";

        // Inspection is enabled by default.
        mActivityTestRule.injectObjectAndReload(
                new Test(), inspectableObjectName, JavascriptInterface.class);

        Assert.assertEquals("m1,m2",
                executeJavaScriptAndGetStringResult(
                        String.format(jsObjectKeysTestTemplate, inspectableObjectName)));
        Assert.assertEquals("m1,m2",
                executeJavaScriptAndGetStringResult(
                        String.format(jsForInTestTemplate, inspectableObjectName)));

        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getJavascriptInjector(useMojo).setAllowInspection(false);
            }
        });

        mActivityTestRule.injectObjectAndReload(
                new Test(), nonInspectableObjectName, JavascriptInterface.class);

        Assert.assertEquals("",
                executeJavaScriptAndGetStringResult(
                        String.format(jsObjectKeysTestTemplate, nonInspectableObjectName)));
        Assert.assertEquals("",
                executeJavaScriptAndGetStringResult(
                        String.format(jsForInTestTemplate, nonInspectableObjectName)));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testAccessToObjectGetClassIsBlocked(boolean useMojo) throws Throwable {
        mActivityTestRule.injectObjectAndReload(new Object(), "testObject", null);
        Assert.assertEquals(
                "function", executeJavaScriptAndGetStringResult("typeof testObject.getClass"));
        assertRaisesException("testObject.getClass()");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testReplaceJavascriptInterface(boolean useMojo) throws Throwable {
        class Test {
            public Test(int value) {
                mValue = value;
            }
            @JavascriptInterface
            public int getValue() {
                return mValue;
            }
            private int mValue;
        }
        mActivityTestRule.injectObjectAndReload(new Test(13), "testObject");
        Assert.assertEquals("13", executeJavaScriptAndGetStringResult("testObject.getValue()"));
        // The documentation doesn't specify, what happens if the embedder is trying
        // to inject a different object under the same name. The current implementation
        // simply replaces the old object with the new one.
        mActivityTestRule.injectObjectAndReload(new Test(42), "testObject");
        Assert.assertEquals("42", executeJavaScriptAndGetStringResult("testObject.getValue()"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testMethodCalledOnAnotherInstance(boolean useMojo) throws Throwable {
        class TestObject {
            private int mIndex;
            TestObject(int index) {
                mIndex = index;
            }
            @JavascriptInterface
            public void method() {
                mTestController.setIntValue(mIndex);
            }
        }
        final TestObject testObject1 = new TestObject(1);
        final TestObject testObject2 = new TestObject(2);
        mActivityTestRule.injectObjectsAndReload(
                testObject1, "testObject1", testObject2, "testObject2", null);
        mActivityTestRule.executeJavaScript("testObject1.method()");
        Assert.assertEquals(1, mTestController.waitForIntValue());
        mActivityTestRule.executeJavaScript("testObject2.method()");
        Assert.assertEquals(2, mTestController.waitForIntValue());
        mActivityTestRule.executeJavaScript("testObject1.method.call(testObject2)");
        Assert.assertEquals(2, mTestController.waitForIntValue());
        mActivityTestRule.executeJavaScript("testObject2.method.call(testObject1)");
        Assert.assertEquals(1, mTestController.waitForIntValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @UseMethodParameter(JavaBridgeActivityTestRule.LegacyTestParams.class)
    public void testWebViewAfterRenderViewSwapped(boolean useMojo) throws Throwable {
        class TestObject {
            private int mIndex;
            TestObject(int index) {
                mIndex = index;
            }
            @JavascriptInterface
            public void method() {
                mTestController.setIntValue(mIndex);
            }
        }
        final TestObject testObject = new TestObject(1);
        mActivityTestRule.injectObjectAndReload(testObject, "testObject");

        // This needs renderer swap but not end up in an error page.
        mActivityTestRule.loadUrl(mActivityTestRule.getWebContents().getNavigationController(),
                mActivityTestRule.getTestCallBackHelperContainer(),
                new LoadUrlParams("chrome://process-internals"));

        mActivityTestRule.handleBlockingCallbackAction(
                mActivityTestRule.getTestCallBackHelperContainer().getOnPageFinishedHelper(),
                new Runnable() {
                    @Override
                    public void run() {
                        mActivityTestRule.getWebContents().getNavigationController().goBack();
                    }
                });

        mActivityTestRule.executeJavaScript("testObject.method()");
        Assert.assertEquals(1, mTestController.waitForIntValue());
    }
}
