# ControllsFix

A mod for Eternal Fighter Zero that lets you use the parts of a controller the
game cannot read by itself: the triggers of Xbox-type controllers, the right
stick, and optionally the D-Pad and buttons above the sixteenth.

The game only knows sixteen buttons per controller and the left stick. With
this mod, pulling a trigger or pushing the right stick looks to the game like
one of those sixteen buttons being pressed, so it can be bound like any other
button.

The mod works in online play too, and your opponent does not need to have it.
It only changes how your own controller is read.

## Install

The mod is loaded by [EfzModManager](https://github.com/fishshapedfish/EfzModManager).

The easy way is [EfzConfig+](https://github.com/Aquat1c/EfzConfigPlus): click a
slot on its Keys page and pull a trigger or push the right stick. It offers to
install and enable the mod for you.

By hand: put `ControllsFix.dll` in `mods\ControllsFix\` in the game folder and
add `ControllsFix=1` under `[Mods]` in `EfzModManager.ini`.

## What becomes which button

| Input | Button |
|---|---|
| Left trigger (Xbox-type controllers) | 11 |
| Right trigger (Xbox-type controllers) | 12 |
| Right stick up | 13 |
| Right stick down | 14 |
| Right stick left | 15 |
| Right stick right | 16 |

Both triggers can be held at the same time.

On PlayStation controllers the triggers already are buttons 7 and 8 and work
without the mod; the mod adds the right stick. Buttons 13 to 16 are also the
PS, touchpad and mute buttons on those controllers, so pressing one of them
counts as the same button as the stick direction that shares its number.

## Settings

The numbers can be changed in `mods\ControllsFix\ControllsFix.ini`, or from
the mod's Settings page in EfzConfig+. A missing file means the defaults
above.

```ini
[General]
; Auto, Xbox or Classic
Layout=Auto
; how far a stick or trigger has to move, in percent (10 to 90)
Threshold=50

[Buttons]
; the button number the game sees, 1 to 16. 0 turns the input off.
LeftTrigger=11
RightTrigger=12
RightStickUp=13
RightStickDown=14
RightStickLeft=15
RightStickRight=16
DPadUp=0
DPadDown=0
DPadLeft=0
DPadRight=0
; Button17 to Button32: fold a higher button down onto one of the sixteen
Button17=0
```

- **Layout**: Auto recognises Xbox-type controllers; every other controller is
  treated as Classic. Xbox means the triggers share one axis and the right
  stick is on the X and Y rotation axes. Classic means the right stick is on
  the Z axis and Z rotation, as on PlayStation controllers and most others.
  Set it by hand if your controller's right stick does nothing or the wrong
  thing.
- **D-Pad**: EfzRevival already uses the D-Pad for movement. Give the D-Pad
  directions numbers only if you want to bind them as buttons instead.
- Close the game before changing the settings; they are read when it starts.

If you change the numbers, bind the keys again in EfzConfig+ afterwards, since
`KEY.ini` stores button numbers.

## Building

Requires Visual Studio with the `v141_xp` toolset and CMake.

```
cmake -B build-xp -A Win32 -T v141_xp
cmake --build build-xp --config Release
ctest --test-dir build-xp -C Release
```

The result is `build-xp\Release\ControllsFix.dll`.

`padmap.c` and `padmap.h` hold the rules that turn a controller's state into
buttons. EfzConfig+ compiles the same two files, so the button it shows when
you bind is the button the game will see.

## Credits

Credits to fishshapedfish for creating
[DPadFix](https://github.com/SokuDev/SokuMods/tree/master/modules/DPadFix),
which this mod's way of changing what the game reads from a controller is
based on, and for allowing its use here.

## License

MIT, see [LICENSE](LICENSE).
