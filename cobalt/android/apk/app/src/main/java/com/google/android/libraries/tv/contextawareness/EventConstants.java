package com.google.android.libraries.tv.contextawareness;

/**
 * Contains constants used to categorize events within the TV Context Awareness system. Events are
 * hierarchical, consisting of a main {@code TYPE_*} and a more specific {@code SUBTYPE_*}.
 */
public final class EventConstants {
  private EventConstants() {}

  // Event Types
  /** Default type for events that are not yet categorized or are unknown. */
  public static final int TYPE_UNKNOWN = 0;

  /**
   * Indicates that the event originated from a key being pressed on an input device, typically a
   * remote control. Further details are provided by subtypes.
   */
  public static final int TYPE_KEY_PRESS = 1;

  // Event Subtypes
  /**
   * A general subtype for {@link #TYPE_KEY_PRESS}, indicating that some key was pressed. This is
   * used when the specific key group is not distinguished or when listening for any key activity.
   */
  public static final int SUBTYPE_KEY_PRESS_ANY = 101;

  /**
   * A subtype for {@link #TYPE_KEY_PRESS} specifically for key presses related to audio volume
   * control, such as VOLUME_UP, VOLUME_DOWN, and MUTE/UNMUTE defined in {@link VolumeEventType}.
   * Events with this subtype also includes an aggregated count of how many times the volume was
   * changed.
   */
  public static final int SUBTYPE_KEY_PRESS_VOLUME_CHANGE = 102;
}
