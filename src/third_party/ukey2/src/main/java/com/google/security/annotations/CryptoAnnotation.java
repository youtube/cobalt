/* Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// Copyright 2007 Google Inc. All Rights Reserved

package com.google.security.annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Crypto Key Annotation: Label any cryptographic keys in code with this 
 * annotation. This will help identify cryptographic keys that are exposed in 
 * source code. Keys in source code should be annotated with an owner, purpose,
 * removal priority, and leak severity.
 * 
 * Example of usage:
 *  @CryptoAnnotation(
 *     purpose = CryptoAnnotation.Purpose.AUTHENTICATION,
 *     owner = "sweis",
 *     bugId = 7041243,
 *     leakSeverity = CryptoAnnotation.LeakSeverity.S2,
 *     removalPriority = CryptoAnnotation.RemovalPriority.P1,
 *     description = "This key is used to sign blah blah blah."
 *     removalDate = "9/2007
 * )
 * byte[] keyBytes = {0xDE, 0xAD, 0xBE, 0xEF};
 * 
 * @author sweis@google.com (Steve Weis)
 */
@Retention(RetentionPolicy.SOURCE)
@Target({ElementType.FIELD, ElementType.LOCAL_VARIABLE})
public @interface CryptoAnnotation {
  /*
   * Keys with "encryption" and "authentication" purposes should be removed
   * from source code.
   * 
   * Keys with "obfuscation" and "integrity check" purposes do not necessarily
   * need to be cryptographically strong. They may or may not be removed from
   * code at the discretion of the code owner.
   */
  public enum Purpose {ENCRYPTION, AUTHENTICATION, OBFUSCATION,
    INTEGRITY_CHECK, PASSWORD, OTHER}
  public enum LeakSeverity {S0, S1, S2, S3, S4, NoRisk}
  public enum RemovalPriority {P0, P1, P2, P3, P4, WillNotFix}
  
  LeakSeverity leakSeverity();
  RemovalPriority removalPriority();
  int bugId() default 0;
  String owner(); // Will be contacted in the event a key is leaked
  Purpose purpose();
  String description() default "";
  String removalDate() default "";
}

