---
layout: doc
title: "Starboard Module Reference: key.h"
---

Defines the canonical set of Starboard key codes.

## Enums ##

### SbKey ###

A standard set of key codes, ordered by value, that represent each possible
input key across all kinds of devices. Starboard uses the semi-standard Windows
virtual key codes documented at: [https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731%28v=vs.85%29.aspx](https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731%28v=vs.85%29.aspx)

#### Values ####

*   `kSbKeyUnknown`
*   `kSbKeyCancel`
*   `kSbKeyBackspace`
*   `kSbKeyBack`
*   `kSbKeyTab`

    semantic "back".
*   `kSbKeyBacktab`
*   `kSbKeyClear`
*   `kSbKeyReturn`
*   `kSbKeyShift`
*   `kSbKeyControl`
*   `kSbKeyMenu`
*   `kSbKeyPause`
*   `kSbKeyCapital`
*   `kSbKeyKana`
*   `kSbKeyHangul`
*   `kSbKeyJunja`
*   `kSbKeyFinal`
*   `kSbKeyHanja`
*   `kSbKeyKanji`
*   `kSbKeyEscape`
*   `kSbKeyConvert`
*   `kSbKeyNonconvert`
*   `kSbKeyAccept`
*   `kSbKeyModechange`
*   `kSbKeySpace`
*   `kSbKeyPrior`
*   `kSbKeyNext`
*   `kSbKeyEnd`
*   `kSbKeyHome`
*   `kSbKeyLeft`
*   `kSbKeyUp`
*   `kSbKeyRight`
*   `kSbKeyDown`
*   `kSbKeySelect`
*   `kSbKeyPrint`
*   `kSbKeyExecute`
*   `kSbKeySnapshot`
*   `kSbKeyInsert`
*   `kSbKeyDelete`
*   `kSbKeyHelp`
*   `kSbKey0`
*   `kSbKey1`
*   `kSbKey2`
*   `kSbKey3`
*   `kSbKey4`
*   `kSbKey5`
*   `kSbKey6`
*   `kSbKey7`
*   `kSbKey8`
*   `kSbKey9`
*   `kSbKeyA`
*   `kSbKeyB`
*   `kSbKeyC`
*   `kSbKeyD`
*   `kSbKeyE`
*   `kSbKeyF`
*   `kSbKeyG`
*   `kSbKeyH`
*   `kSbKeyI`
*   `kSbKeyJ`
*   `kSbKeyK`
*   `kSbKeyL`
*   `kSbKeyM`
*   `kSbKeyN`
*   `kSbKeyO`
*   `kSbKeyP`
*   `kSbKeyQ`
*   `kSbKeyR`
*   `kSbKeyS`
*   `kSbKeyT`
*   `kSbKeyU`
*   `kSbKeyV`
*   `kSbKeyW`
*   `kSbKeyX`
*   `kSbKeyY`
*   `kSbKeyZ`
*   `kSbKeyLwin`
*   `kSbKeyCommand`
*   `kSbKeyRwin`
*   `kSbKeyApps`
*   `kSbKeySleep`
*   `kSbKeyNumpad0`
*   `kSbKeyNumpad1`
*   `kSbKeyNumpad2`
*   `kSbKeyNumpad3`
*   `kSbKeyNumpad4`
*   `kSbKeyNumpad5`
*   `kSbKeyNumpad6`
*   `kSbKeyNumpad7`
*   `kSbKeyNumpad8`
*   `kSbKeyNumpad9`
*   `kSbKeyMultiply`
*   `kSbKeyAdd`
*   `kSbKeySeparator`
*   `kSbKeySubtract`
*   `kSbKeyDecimal`
*   `kSbKeyDivide`
*   `kSbKeyF1`
*   `kSbKeyF2`
*   `kSbKeyF3`
*   `kSbKeyF4`
*   `kSbKeyF5`
*   `kSbKeyF6`
*   `kSbKeyF7`
*   `kSbKeyF8`
*   `kSbKeyF9`
*   `kSbKeyF10`
*   `kSbKeyF11`
*   `kSbKeyF12`
*   `kSbKeyF13`
*   `kSbKeyF14`
*   `kSbKeyF15`
*   `kSbKeyF16`
*   `kSbKeyF17`
*   `kSbKeyF18`
*   `kSbKeyF19`
*   `kSbKeyF20`
*   `kSbKeyF21`
*   `kSbKeyF22`
*   `kSbKeyF23`
*   `kSbKeyF24`
*   `kSbKeyNumlock`
*   `kSbKeyScroll`
*   `kSbKeyWlan`
*   `kSbKeyPower`
*   `kSbKeyLshift`
*   `kSbKeyRshift`
*   `kSbKeyLcontrol`
*   `kSbKeyRcontrol`
*   `kSbKeyLmenu`
*   `kSbKeyRmenu`
*   `kSbKeyBrowserBack`
*   `kSbKeyBrowserForward`
*   `kSbKeyBrowserRefresh`
*   `kSbKeyBrowserStop`
*   `kSbKeyBrowserSearch`
*   `kSbKeyBrowserFavorites`
*   `kSbKeyBrowserHome`
*   `kSbKeyVolumeMute`
*   `kSbKeyVolumeDown`
*   `kSbKeyVolumeUp`
*   `kSbKeyMediaNextTrack`
*   `kSbKeyMediaPrevTrack`
*   `kSbKeyMediaStop`
*   `kSbKeyMediaPlayPause`
*   `kSbKeyMediaLaunchMail`
*   `kSbKeyMediaLaunchMediaSelect`
*   `kSbKeyMediaLaunchApp1`
*   `kSbKeyMediaLaunchApp2`
*   `kSbKeyOem1`
*   `kSbKeyOemPlus`
*   `kSbKeyOemComma`
*   `kSbKeyOemMinus`
*   `kSbKeyOemPeriod`
*   `kSbKeyOem2`
*   `kSbKeyOem3`
*   `kSbKeyBrightnessDown`
*   `kSbKeyBrightnessUp`
*   `kSbKeyKbdBrightnessDown`
*   `kSbKeyOem4`
*   `kSbKeyOem5`
*   `kSbKeyOem6`
*   `kSbKeyOem7`
*   `kSbKeyOem8`
*   `kSbKeyOem102`
*   `kSbKeyKbdBrightnessUp`
*   `kSbKeyDbeSbcschar`
*   `kSbKeyDbeDbcschar`
*   `kSbKeyPlay`
*   `kSbKeyMediaRewind`

    Other supported CEA 2014 keys.
*   `kSbKeyMediaFastForward`
*   `kSbKeyRed`

    Key codes from the DTV Application Software Environment, [http://www.atsc.org/wp-content/uploads/2015/03/a_100_4.pdf](http://www.atsc.org/wp-content/uploads/2015/03/a_100_4.pdf)
*   `kSbKeyGreen`
*   `kSbKeyYellow`
*   `kSbKeyBlue`
*   `kSbKeyChannelUp`
*   `kSbKeyChannelDown`
*   `kSbKeySubtitle`
*   `kSbKeyClosedCaption`
*   `kSbKeyInfo`
*   `kSbKeyGuide`
*   `kSbKeyLast`

    Key codes from OCAP, [https://apps.cablelabs.com/specification/opencable-application-platform-ocap/](https://apps.cablelabs.com/specification/opencable-application-platform-ocap/)
*   `kSbKeyPreviousChannel`
*   `kSbKeyLaunchThisApplication`

    A button that will directly launch the current application.
*   `kSbKeyMediaAudioTrack`

    A button that will switch between different available audio tracks.
*   `kSbKeyMouse1`

    Mouse buttons, starting with the left mouse button.
*   `kSbKeyMouse2`
*   `kSbKeyMouse3`
*   `kSbKeyMouse4`
*   `kSbKeyMouse5`
*   `kSbKeyGamepad1`

    Xbox A, PS O or X (depending on region), WiiU B
*   `kSbKeyGamepad2`

    Xbox B, PS X or O (depending on region), WiiU A key
*   `kSbKeyGamepad3`

    Xbox X, PS square, WiiU Y
*   `kSbKeyGamepad4`

    Xbox Y, PS triangle, WiiU X
*   `kSbKeyGamepadLeftBumper`

    Pretty much every gamepad has bumpers at the top front of the controller,
    and triggers at the bottom front of the controller.
*   `kSbKeyGamepadRightBumper`
*   `kSbKeyGamepadLeftTrigger`
*   `kSbKeyGamepadRightTrigger`
*   `kSbKeyGamepad5`

    Xbox 360 Back, XB1 minimize, PS and WiiU Select
*   `kSbKeyGamepad6`

    Xbox 360 Play, XB1 Menu, PS and WiiU Start
*   `kSbKeyGamepadLeftStick`

    This refers to pressing the left stick like a button.
*   `kSbKeyGamepadRightStick`

    This refers to pressing the right stick like a button.
*   `kSbKeyGamepadDPadUp`
*   `kSbKeyGamepadDPadDown`
*   `kSbKeyGamepadDPadLeft`
*   `kSbKeyGamepadDPadRight`
*   `kSbKeyGamepadSystem`

    The system key in the middle of the gamepad, if it exists.
*   `kSbKeyGamepadLeftStickUp`

    Codes for thumbstick to virtual dpad conversions.
*   `kSbKeyGamepadLeftStickDown`
*   `kSbKeyGamepadLeftStickLeft`
*   `kSbKeyGamepadLeftStickRight`
*   `kSbKeyGamepadRightStickUp`
*   `kSbKeyGamepadRightStickDown`
*   `kSbKeyGamepadRightStickLeft`
*   `kSbKeyGamepadRightStickRight`

### SbKeyModifiers ###

Bit-mask of key modifiers.

#### Values ####

*   `kSbKeyModifiersNone`
*   `kSbKeyModifiersAlt`
*   `kSbKeyModifiersCtrl`
*   `kSbKeyModifiersMeta`
*   `kSbKeyModifiersShift`
*   `kSbKeyModifiersPointerButtonLeft`
*   `kSbKeyModifiersPointerButtonRight`
*   `kSbKeyModifiersPointerButtonMiddle`
*   `kSbKeyModifiersPointerButtonBack`
*   `kSbKeyModifiersPointerButtonForward`

