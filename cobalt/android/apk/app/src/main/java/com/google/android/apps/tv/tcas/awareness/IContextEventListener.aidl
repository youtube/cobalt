package com.google.android.apps.tv.tcas.awareness;

import android.os.Bundle;

oneway interface IContextEventListener {

    /**
     * Callback for events for simple state change.
     *
     * @param eventType Can be in:
     *     - {@link EventConstants#TYPE_UNKNOWN}: Unknown event.
     *     - {@link EventConstants#TYPE_KEY_PRESS}: TV remote key press event.
     * @param eventSubtype Can be in:
     *     - {@link EventConstants#SUBTYPE_KEY_PRESS_ANY}: Any key press.
     *     - {@link EventConstants#SUBTYPE_KEY_PRESS_VOLUME_CHANGE}: Key press
     *       specific to volume change.
     */
    void onContextEvent(int eventType, int eventSubtype);

    /**
     * Callback for events that carry a magnitude or scalar value.
     *
     * @param eventType Can be in:
     *     - {@link EventConstants#TYPE_UNKNOWN}: Unknown event.
     *     - {@link EventConstants#TYPE_KEY_PRESS}: TV remote key press event.
     * @param eventSubtype Can be in:
     *     - {@link EventConstants#SUBTYPE_KEY_PRESS_ANY}: Any key press.
     *     - {@link EventConstants#SUBTYPE_KEY_PRESS_VOLUME_CHANGE}: Key press
     *        specific to volume change.
     * @param data A bundle that contains event specific data.
     *     - For {@link EventConstants#SUBTYPE_KEY_PRESS_VOLUME_CHANGE} event,
     *       bundle contains "direction" {@link VolumeEventType} (int)
     *       and "count" (int), indicating how many times
     *       volume up or down was pressed. The count is 1 for mute and unmute
     *       events.
     */
    void onContextEventWithData(int eventType, int eventSubtype, in Bundle data);
}
