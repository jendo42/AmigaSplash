# AmigaSplash

This simple tool is indended to show splash screen during boot process from HDD. For now it supports only OCS/ECS graphics and no HAM modes.

## Supported formats
 - `S:splash.wav`: stereo, mono, unsigned 8-bit. Length is limited to the Paula max audio buffer (131072 bytes).
 - `S:splash.iff`: Capabilities of the OS graphics subsystem. It will not open the screen if there is something that could not be displayed.

## Operation
 - Loads assets from disk (`S:splash.iff` and `S:splash.wav`)
 - Fade in the splash image
 - Waits for the system to create default workbench screen
 - Plays the `splash.wav`.
 - Fades out the splash image
 - Waits for audio to finish
 - Exits

Note: program can be also exited by pressing ESC on keyboard.

## Installation
Just put the `AmigaSplash` somewhere into your system (e.g. `C:`) and then add following line into `S:startup-sequence`
```
Run >NIL: C:AmigaSplash
```
