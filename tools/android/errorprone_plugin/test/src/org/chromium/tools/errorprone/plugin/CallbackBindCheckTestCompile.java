// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import org.chromium.base.Callback;

/** Test for CallbackBindCheck that verifies valid code compiles without warnings. */
public class CallbackBindCheckTestCompile {
    private String mField;

    public static <T> T verify(T mock) {
        return mock;
    }

    public String someFunc() {
        return "dynamic";
    }

    public void testNoTrigger(Callback<Object> callback, String value, Runnable otherRunnable) {
        // No trigger: Not a Runnable target type (it's Callback)
        Callback<Object> c1 = (val) -> callback.onResult(val);

        // No trigger: More than one statement
        Runnable r1 =
                () -> {
                    callback.onResult(value);
                    otherRunnable.run();
                };

        // No trigger: Not a Callback receiver
        class NonCallback {
            void onResult(Object s) {}
        }
        NonCallback nonCallback = new NonCallback();
        Runnable r2 = () -> nonCallback.onResult(value);

        // No trigger: Calling different method
        class CallbackWrapper implements Callback<Object> {
            @Override
            public void onResult(Object result) {}

            public void otherMethod(Object result) {}
        }
        CallbackWrapper wrapper = new CallbackWrapper();
        Runnable r3 = () -> wrapper.otherMethod(value);

        // No trigger: Using explicit field (this.mField)
        Runnable r4 = () -> callback.onResult(this.mField);

        // No trigger: Using implicit field (mField)
        Runnable r5 = () -> callback.onResult(mField);

        // No trigger: Method invocation in argument
        Runnable r6 = () -> callback.onResult(someFunc());

        // No trigger: New class creation in argument
        Runnable r7 = () -> callback.onResult(new Object());

        // No trigger: Receiver has method call (callbackCaptor.getValue())
        class Captor {
            Callback<Object> getValue() {
                return null;
            }
        }
        Captor callbackCaptor = new Captor();
        Runnable r8 = () -> callbackCaptor.getValue().onResult(value);

        // No trigger: Mockito verify style receiver
        Runnable r9 = () -> verify(callback).onResult(value);

        // No trigger: Array access in argument (array[0])
        String[] array = new String[] {value};
        Runnable r10 = () -> callback.onResult(array[0]);
    }
}
