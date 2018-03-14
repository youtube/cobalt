// Copyright 2017 Google Inc. All Rights Reserved.
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

import java.lang.annotation.ElementType;
import java.lang.annotation.Target;

/**
 * Annotation used for marking methods and fields that are called from native code. Useful for
 * keeping components that would otherwise be removed by Proguard.
 */
@Target({ElementType.METHOD, ElementType.FIELD, ElementType.TYPE, ElementType.CONSTRUCTOR})
public @interface UsedByNative {}
