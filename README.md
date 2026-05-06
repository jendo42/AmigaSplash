# AmigaSplash

This simple tool is indended to show splash screen during boot process from HDD. For now it supports only OCS/ECS graphics and no HAM modes.

## Supported formats
 - `S:splash.wav`: stereo, mono, unsigned 8-bit. Length is limited to the Paula max audio buffer (131072 bytes).
 - `S:splash.iff`: Capabilities of the OS graphics subsystem. It will not open the screen if there is something that could not be displayed.

## Operation
 - Loads assets from disk (`S:splash.iff` and `S:splash.wav`)
 - Fade in the splash image (`S:splash.iff`)
 - Waits for the system to create default workbench screen (mostly splash window loses focus)
 - Plays the splash sound (`S:splash.wav`)
 - Fades out the splash image
 - Fades the workbench in
 - Waits for audio to finish
 - Exits

Note: program can be also exited by pressing ESC on keyboard.

## Installation
Just copy the `AmigaSplash` somewhere into your system (e.g. `C:`) and then in the beginning of `S:startup-sequence` insert this line:
```
Run >NIL: C:AmigaSplash >RAM:splash.log
```

Program can be run with one parameter `C:AmigaSplash signal`. This will just find the running AmigaSplash task in the memory and sends break C signal into it. This makes possible to trigger end of the 'loading' by adding anywhere into `startup-sequence`.
