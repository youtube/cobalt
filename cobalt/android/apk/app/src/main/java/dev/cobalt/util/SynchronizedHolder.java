// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.util;

import java.util.function.Supplier;

/**
 * Generic holder class to turn null checks into a known RuntimeException. Access is synchronized,
 * so access from multiple threads is safe.
 *
 * @param <T> The type of the value to hold.
 * @param <E> The type of the exception to throw if the value is null.
 */
public class SynchronizedHolder<T, E extends RuntimeException> {
  private T mValue;
  private final Supplier<E> mExceptionSupplier;

  public SynchronizedHolder(Supplier<E> exceptionSupplier) {
    mExceptionSupplier = exceptionSupplier;
  }

  public synchronized T get() {
    if (mValue == null) {
      throw mExceptionSupplier.get();
    }
    return mValue;
  }

  public synchronized void set(T value) {
    mValue = value;
  }
}
