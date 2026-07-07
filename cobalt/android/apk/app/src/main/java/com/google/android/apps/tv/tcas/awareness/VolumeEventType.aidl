package com.google.android.apps.tv.tcas.awareness;

/**
 * Volume event type
 */
@Backing(type="int")
enum VolumeEventType {
    UNDEFINED = 0,
    UP = 1,
    DOWN = 2,
    MUTE = 3,
    UNMUTE = 4,
}
