# How ControllsFix works

## What the game reads

Eternal Fighter Zero reads each controller through DirectInput. For every
binding in `KEY.ini` it looks at one of two things: one of the first sixteen
buttons of the controller, or the left stick pushed past half of its travel.
It never looks at any other axis, at the D-Pad, or at buttons above the
sixteenth. EfzRevival adds the D-Pad as movement and nothing else.

On Xbox-type controllers the two triggers are not buttons. Windows reports
them to DirectInput as one shared axis: the left trigger pushes it one way and
the right trigger the other. The right stick is two more axes. None of this
reaches the game.

## What the mod changes

The mod changes the answer DirectInput gives when a program asks for the state
of a controller. After the real state has been read, the mod marks extra
buttons as pressed in it, according to the rules below, and hands the result
on. The game then finds a pressed button where it would have found nothing.

Both the game and EfzRevival ask through the same DirectInput call, so both
see the same result. A trigger can therefore also be used for an EfzRevival
hotkey.

Only buttons are ever added. A button the controller really reports as pressed
stays pressed.

## EfzRevival and the D-Pad

EfzRevival reads the D-Pad itself and uses it as movement for a player who has
nothing bound to the four directions. The mod never removes the D-Pad from the
controller state, so this keeps working with the mod loaded.

Giving the D-Pad directions button numbers in the mod's settings does not
switch that off. A D-Pad press is then the chosen button and, for a player
whose directions are unbound, movement at the same time. The setting is meant
for a player whose directions are bound to the stick or the keyboard and who
wants the D-Pad for something else.

## Why it works in online play

EfzRevival takes each player's input after the game has turned the controller
state into its own small set of bits: four directions and four buttons. Those
bits are what is sent to the other player and written into replays. The mod
acts before that step and only on the local machine, so nothing about the
game's simulation or about what is sent changes. The other player does not
need the mod.

## How the hook is installed

EfzModManager loads the mod's DLL into the game. The mod starts a short
thread that:

1. reads `ControllsFix.ini` from the mod's own folder,
2. loads the DirectInput of Windows from the system folder by its full path,
   because the `dinput.dll` in the game folder is the mod loader,
3. creates a DirectInput keyboard device, only to get at the table of
   functions that all DirectInput devices of a program share,
4. replaces the "get device state" entry of that table with its own function,
5. releases the keyboard device again.

Keyboard and controller devices use the same table, so the replacement covers
the controllers the game opens later. The mod's function leaves every answer
alone that is not a controller state: it only acts when the caller asked for
the standard joystick structure and the read succeeded.

## Telling controllers apart

The first time a controller's state passes through, the mod notes two things
about it and keeps them:

- whether it is an Xbox-type controller. Windows marks those with `IG_` in the
  device path.
- which of the four extra axes exist (Z, X rotation, Y rotation, Z rotation)
  and what range of values each one has. Ranges differ: the game leaves them
  at the DirectInput default, while EfzConfig+ sets them to -1000 to 1000. The
  rules below work in percent of the range, so both give the same result.

## The rules

A stick direction or a trigger counts as pressed when it is further from its
resting position than the threshold, which is a percentage of the way to the
end of its travel. The default is 50.

**Xbox layout** (Xbox-type controllers, or `Layout=Xbox`):

- The right stick is the X rotation axis (left and right) and the Y rotation
  axis (up and down).
- The triggers are read through XInput when possible, see the next section.
  Otherwise the shared Z axis is used: pushed one way it is the left trigger,
  the other way the right trigger. In that case holding both triggers cancels
  out and neither is seen.

**Classic layout** (all other controllers, or `Layout=Classic`):

- If the controller has a Z rotation axis, the right stick is the Z axis (left
  and right) and Z rotation (up and down). This is the PlayStation
  arrangement.
- If it has no Z rotation axis, the right stick is taken from the X and Y
  rotation axes.
- No triggers are mapped. On these controllers the triggers are ordinary
  buttons already. Their analog axes rest at one end instead of in the middle
  and are deliberately not read as a stick.

**Any layout:**

- Each D-Pad direction can be given a button. A diagonal presses both of its
  directions. This is off by default, because EfzRevival uses the D-Pad for
  movement.
- Each of the buttons 17 to 32 can be given one of the sixteen buttons the
  game reads. This is off by default.

Each input has a button number from 1 to 16 in the ini file; 0 turns it off.
The defaults are 11 and 12 for the left and right trigger and 13, 14, 15 and
16 for right stick up, down, left and right.

## Reading both triggers at once

XInput reports the two triggers separately, but it numbers controllers one to
four on its own, and nothing says directly which of those is the DirectInput
device the game is reading. The mod finds out by comparing:

- If only one XInput controller is connected, it is that one.
- Otherwise it waits until something is pressed on the DirectInput device, a
  button or a trigger, and looks for the one XInput controller that shows the
  same ten buttons pressed and a trigger pulled the same way. If exactly one
  matches, that is the controller, and the mod remembers it.
- Until a match is found the shared axis is used.
- If the remembered XInput controller stops answering, the mod forgets it and
  starts over.

XInput is loaded from the Windows system folder. On a system without it the
shared axis is always used.

## Settings

`ControllsFix.ini` sits next to the DLL. A missing file, a missing key or a
value out of range means the default. The file is read once, when the game
starts.

The DLL exports a table of its settings named `EFZ_ConfigSchema`, in the
format EfzConfig+ reads, so the tool can show a Settings page for the mod
without the ini file having to exist first.

## Shared with EfzConfig+

`padmap.c` and `padmap.h` contain everything described under "Telling
controllers apart", "The rules" and "Reading both triggers at once". The mod
uses them inside the game; EfzConfig+ compiles the same files and applies them
to what it reads while you bind a key. That is why the tool shows button 11
when you pull the left trigger: it is what the game will see with the mod
installed. The tool reads the mod's ini file as well, so a changed number is
followed.

## Building

- Windows XP is the target: the v141_xp toolset, 32 bit, static C runtime, and
  only functions that exist on XP.
- The test, `test_padmap`, checks the rules against made-up controller states.
  It needs no controller.
