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
package com.google.security.cryptauth.lib.securegcm;

/**
 * A container for GCM related constants used by SecureGcm channels.
 */
public final class SecureGcmConstants {
  private SecureGcmConstants() {} // Do not instantiate

  public static final int SECURE_GCM_VERSION = 1;

  /**
   * The GCM sender identity used by this library (GMSCore).
   */
  public static final String SENDER_ID = "745476177629";

  /**
   * The key used for indexing the GCM {@link TransportCryptoOps.Payload} within {@code AppData}.
   */
  public static final String MESSAGE_KEY = "P";

  /**
   * The origin that should be use for GCM device enrollments.
   */
  public static final String GOOGLE_ORIGIN = "google.com";

  /**
   * The origin that should be use for GCM Legacy android device enrollments.
   */
  public static final String LEGACY_ANDROID_ORIGIN = "c.g.a.gms";

  /**
   * The name of the protocol this library speaks.
   */
  public static final String PROTOCOL_TYPE_NAME = "gcmV1";
}
